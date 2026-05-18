#ifdef __APPLE__

#include <chrono>
#include <cstring>
#include <ctime>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <mach/mach.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "portfolio/platform/telemetry_collector.hpp"

namespace portfolio::platform {
namespace {

// ── sysctl helpers ────────────────────────────────────────────────────────────

static std::string sysctl_str(const char* name) {
    char buf[512]{};
    size_t len = sizeof(buf);
    sysctlbyname(name, buf, &len, nullptr, 0);
    return {buf, strnlen(buf, sizeof(buf))};
}

static uint64_t sysctl_u64(const char* name) {
    uint64_t v = 0;
    size_t len = sizeof(v);
    sysctlbyname(name, &v, &len, nullptr, 0);
    return v;
}

static int32_t sysctl_i32(const char* name) {
    int32_t v = 0;
    size_t len = sizeof(v);
    sysctlbyname(name, &v, &len, nullptr, 0);
    return v;
}

// ── CPU ticks (for usage %) ───────────────────────────────────────────────────

struct CpuTicks {
    uint64_t user = 0, sys = 0, idle = 0, nice = 0;
    uint64_t total() const { return user + sys + idle + nice; }
    uint64_t busy()  const { return user + sys + nice; }
};

static CpuTicks read_cpu_ticks() {
    host_cpu_load_info_data_t info{};
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO,
                    reinterpret_cast<host_info_t>(&info), &count);
    return {info.cpu_ticks[CPU_STATE_USER],
            info.cpu_ticks[CPU_STATE_SYSTEM],
            info.cpu_ticks[CPU_STATE_IDLE],
            info.cpu_ticks[CPU_STATE_NICE]};
}

// ── one-time static info ──────────────────────────────────────────────────────

static CpuStaticInfo build_cpu_static() {
    CpuStaticInfo info;
    info.brand  = sysctl_str("machdep.cpu.brand_string");
    info.vendor = sysctl_str("machdep.cpu.vendor");
    // Apple Silicon doesn't expose machdep.cpu.* — fall back to hw.model
    if (info.brand.empty())  info.brand  = sysctl_str("hw.model");
    if (info.vendor.empty()) info.vendor = "Apple";

    struct utsname u{};
    uname(&u);
    info.architecture = u.machine;

    info.logical_cores  = static_cast<uint32_t>(sysctl_i32("hw.logicalcpu"));
    info.physical_cores = static_cast<uint32_t>(sysctl_i32("hw.physicalcpu"));
    // hw.cpufrequency is absent on Apple Silicon — stays 0, shown as "–" in UI
    info.base_freq_mhz  = static_cast<uint32_t>(sysctl_u64("hw.cpufrequency")     / 1'000'000);
    info.max_freq_mhz   = static_cast<uint32_t>(sysctl_u64("hw.cpufrequency_max") / 1'000'000);
    info.l2_cache_kb    = static_cast<uint32_t>(sysctl_u64("hw.l2cachesize")      / 1024);
    info.l3_cache_kb    = static_cast<uint32_t>(sysctl_u64("hw.l3cachesize")      / 1024);
    return info;
}

static OsInfo build_os_info() {
    OsInfo info;
    info.name            = "macOS";
    info.display_version = sysctl_str("kern.osproductversion");
    info.build           = sysctl_str("kern.osversion");
    char hostname[256]{};
    gethostname(hostname, sizeof(hostname));
    info.hostname     = hostname;
    struct utsname u{};
    uname(&u);
    info.architecture = u.machine;
    info.is_elevated  = (getuid() == 0);
    return info;
}

// ── per-sample reads ──────────────────────────────────────────────────────────

static void sample_memory(SensorData& s) {
    vm_statistics64_data_t vm{};
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    host_statistics64(mach_host_self(), HOST_VM_INFO64,
                      reinterpret_cast<host_info64_t>(&vm), &count);
    vm_size_t page_size = 0;
    host_page_size(mach_host_self(), &page_size);

    double page_mb  = static_cast<double>(page_size) / (1024.0 * 1024.0);
    double total_mb = static_cast<double>(sysctl_u64("hw.memsize")) / (1024.0 * 1024.0);
    double avail_mb = static_cast<double>(vm.free_count + vm.inactive_count) * page_mb;
    double used_mb  = total_mb - avail_mb;

    s.mem_total_mb    = total_mb;
    s.mem_avail_mb    = avail_mb;
    s.mem_used_pct    = total_mb > 0 ? (used_mb / total_mb) * 100.0 : 0.0;
    // wired + active = "pressured" memory — closest macOS analogue to Windows commit
    s.commit_used_mb  = static_cast<double>(vm.wire_count + vm.active_count) * page_mb;
    s.commit_total_mb = total_mb;
}

static void sample_disk(SensorData& s) {
    struct statfs st{};
    if (statfs("/", &st) != 0) return;
    double total = static_cast<double>(st.f_blocks) * st.f_bsize;
    double free_ = static_cast<double>(st.f_bfree)  * st.f_bsize;
    s.disk_mount_point = "/";
    s.disk_total_gb    = total / (1024.0 * 1024.0 * 1024.0);
    s.disk_free_gb     = free_ / (1024.0 * 1024.0 * 1024.0);
    s.disk_used_pct    = total > 0 ? ((total - free_) / total) * 100.0 : 0.0;
}

static double read_uptime() {
    int mib[2] = {CTL_KERN, KERN_BOOTTIME};
    struct timeval tv{};
    size_t len = sizeof(tv);
    sysctl(mib, 2, &tv, &len, nullptr, 0);
    return static_cast<double>(time(nullptr) - tv.tv_sec);
}

// ── disk inventory ────────────────────────────────────────────────────────────

static std::vector<DiskInfo> build_disk_inventory() {
    std::vector<DiskInfo> out;
    struct statfs* mounts = nullptr;
    int n = getmntinfo(&mounts, MNT_NOWAIT);
    for (int i = 0; i < n; ++i) {
        const auto& m = mounts[i];
        std::string fs = m.f_fstypename;
        if (fs == "devfs" || fs == "autofs" || fs == "nullfs" ||
            fs == "fdesc"  || fs == "synthfs" || fs == "dnssd") continue;

        DiskInfo d;
        d.mount_point  = m.f_mntonname;
        d.filesystem   = fs;
        d.volume_label = m.f_mntfromname;
        d.drive_type   = "Fixed";
        d.ready        = true;

        double total = static_cast<double>(m.f_blocks) * m.f_bsize;
        double free_ = static_cast<double>(m.f_bfree)  * m.f_bsize;
        double used  = total - free_;
        d.total_gb = total / (1024.0 * 1024.0 * 1024.0);
        d.free_gb  = free_ / (1024.0 * 1024.0 * 1024.0);
        d.used_gb  = used  / (1024.0 * 1024.0 * 1024.0);
        d.used_pct = total > 0 ? (used / total) * 100.0 : 0.0;
        out.push_back(std::move(d));
    }
    return out;
}

// ── collector ─────────────────────────────────────────────────────────────────

class MacOSTelemetryCollector final : public TelemetryCollector {
public:
    MacOSTelemetryCollector() {
        snapshot_.cpu_info = build_cpu_static();
        snapshot_.os_info  = build_os_info();
    }

    void start() override {
        prev_ = read_cpu_ticks();
        std::thread([this] { loop(); }).detach();
    }

    SensorData sensor_snapshot() const override {
        std::lock_guard<std::mutex> lk(mtx_);
        return snapshot_;
    }

    std::vector<DiskInfo> disk_inventory_snapshot() const override {
        return build_disk_inventory();
    }

private:
    void loop() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            CpuTicks cur = read_cpu_ticks();
            double cpu_pct = 0.0;
            uint64_t dt = cur.total() - prev_.total();
            if (dt > 0)
                cpu_pct = 100.0 * static_cast<double>(cur.busy() - prev_.busy())
                                / static_cast<double>(dt);
            prev_ = cur;

            std::lock_guard<std::mutex> lk(mtx_);
            snapshot_.cpu_usage_pct  = cpu_pct;
            snapshot_.uptime_seconds = read_uptime();
            sample_memory(snapshot_);
            sample_disk(snapshot_);
            // Thermal: macOS SMC requires a third-party driver — left empty
        }
    }

    mutable std::mutex mtx_;
    SensorData         snapshot_;
    CpuTicks           prev_{};
};

}  // namespace

std::unique_ptr<TelemetryCollector> make_default_telemetry_collector() {
    return std::make_unique<MacOSTelemetryCollector>();
}

}  // namespace portfolio::platform

#endif  // __APPLE__
