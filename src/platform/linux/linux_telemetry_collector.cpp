#ifdef __linux__

#include <chrono>
#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "portfolio/platform/telemetry_collector.hpp"

namespace portfolio::platform {
namespace {

// ── /proc/stat ────────────────────────────────────────────────────────────────

struct CpuTicks {
    unsigned long long user=0, nice=0, sys=0, idle=0, iowait=0, irq=0, softirq=0;
    unsigned long long total() const { return user+nice+sys+idle+iowait+irq+softirq; }
    unsigned long long busy()  const { return user+nice+sys+irq+softirq; }
};

static CpuTicks read_cpu_ticks() {
    std::ifstream f("/proc/stat");
    CpuTicks t;
    std::string label;
    f >> label >> t.user >> t.nice >> t.sys >> t.idle >> t.iowait >> t.irq >> t.softirq;
    return t;
}

// ── /proc/meminfo ─────────────────────────────────────────────────────────────

struct MemInfo {
    double total_mb=0, avail_mb=0, used_pct=0;
    double commit_total_mb=0, commit_used_mb=0;
};

static MemInfo read_meminfo() {
    std::ifstream f("/proc/meminfo");
    std::string line;
    unsigned long long mem_total=0, mem_avail=0, commit_limit=0, committed=0;
    while (std::getline(f, line)) {
        unsigned long long v = 0;
        if      (sscanf(line.c_str(), "MemTotal: %llu kB",     &v) == 1) mem_total    = v;
        else if (sscanf(line.c_str(), "MemAvailable: %llu kB", &v) == 1) mem_avail    = v;
        else if (sscanf(line.c_str(), "CommitLimit: %llu kB",  &v) == 1) commit_limit = v;
        else if (sscanf(line.c_str(), "Committed_AS: %llu kB", &v) == 1) committed    = v;
    }
    MemInfo m;
    m.total_mb        = static_cast<double>(mem_total)    / 1024.0;
    m.avail_mb        = static_cast<double>(mem_avail)    / 1024.0;
    double used       = m.total_mb - m.avail_mb;
    m.used_pct        = m.total_mb > 0 ? (used / m.total_mb) * 100.0 : 0.0;
    m.commit_total_mb = static_cast<double>(commit_limit) / 1024.0;
    m.commit_used_mb  = static_cast<double>(committed)    / 1024.0;
    return m;
}

// ── /proc/uptime ──────────────────────────────────────────────────────────────

static double read_uptime() {
    std::ifstream f("/proc/uptime");
    double up = 0.0;
    f >> up;
    return up;
}

// ── /proc/cpuinfo ─────────────────────────────────────────────────────────────

static CpuStaticInfo build_cpu_static() {
    CpuStaticInfo info;
    std::ifstream f("/proc/cpuinfo");
    std::string line;
    int logical = 0;
    bool got_cores = false;

    auto trim = [](std::string s) -> std::string {
        while (!s.empty() && (s.back() == '\r' || s.back() == ' ')) s.pop_back();
        return s;
    };
    auto after_colon = [&](const std::string& l) -> std::string {
        auto p = l.find(':');
        if (p == std::string::npos) return {};
        return trim(l.substr(p + 2));
    };

    while (std::getline(f, line)) {
        if (line.find("processor") == 0) {
            ++logical;
        } else if (line.find("vendor_id") == 0 && info.vendor.empty()) {
            info.vendor = after_colon(line);
        } else if ((line.find("model name") == 0 || line.find("Processor") == 0)
                   && info.brand.empty()) {
            info.brand = after_colon(line);
        } else if (line.find("cpu MHz") == 0 && info.base_freq_mhz == 0) {
            try { info.base_freq_mhz = static_cast<uint32_t>(std::stof(after_colon(line))); }
            catch (...) {}
        } else if (!got_cores && line.find("cpu cores") == 0) {
            try {
                info.physical_cores = static_cast<uint32_t>(std::stoi(after_colon(line)));
                got_cores = true;
            } catch (...) {}
        } else if (line.find("cache size") == 0 && info.l2_cache_kb == 0) {
            unsigned int kb = 0;
            if (sscanf(line.c_str(), "cache size : %u KB", &kb) == 1) info.l2_cache_kb = kb;
        }
    }
    info.logical_cores = static_cast<uint32_t>(logical);
    if (info.physical_cores == 0) info.physical_cores = info.logical_cores;

    // Max clock from cpufreq (not available in all VMs / ARM configurations)
    {
        std::ifstream mhz("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
        unsigned long long hz = 0;
        if (mhz >> hz) info.max_freq_mhz = static_cast<uint32_t>(hz / 1000);
    }

    struct utsname u{};
    uname(&u);
    info.architecture = u.machine;
    return info;
}

// ── /etc/os-release ───────────────────────────────────────────────────────────

static OsInfo build_os_info() {
    OsInfo info;
    {
        std::ifstream f("/etc/os-release");
        std::string line;
        auto unquote = [](const std::string& s) -> std::string {
            if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
                return s.substr(1, s.size() - 2);
            return s;
        };
        while (std::getline(f, line)) {
            if      (line.find("PRETTY_NAME=") == 0) info.name            = unquote(line.substr(12));
            else if (line.find("VERSION_ID=")  == 0) info.display_version = unquote(line.substr(11));
            else if (line.find("BUILD_ID=")    == 0) info.build           = unquote(line.substr(9));
        }
    }
    if (info.name.empty()) info.name = "Linux";

    struct utsname u{};
    uname(&u);
    info.architecture = u.machine;
    if (info.build.empty()) info.build = u.release;

    char hostname[256]{};
    gethostname(hostname, sizeof(hostname));
    info.hostname    = hostname;
    info.is_elevated = (getuid() == 0);
    return info;
}

// ── primary disk (/) ──────────────────────────────────────────────────────────

static void sample_disk(SensorData& s) {
    struct statvfs st{};
    if (statvfs("/", &st) != 0) return;
    double total = static_cast<double>(st.f_blocks) * st.f_frsize;
    double free_ = static_cast<double>(st.f_bfree)  * st.f_frsize;
    s.disk_mount_point = "/";
    s.disk_total_gb    = total / (1024.0 * 1024.0 * 1024.0);
    s.disk_free_gb     = free_ / (1024.0 * 1024.0 * 1024.0);
    s.disk_used_pct    = total > 0 ? ((total - free_) / total) * 100.0 : 0.0;
}

// ── /sys/class/thermal ────────────────────────────────────────────────────────

static std::vector<TemperatureReading> read_thermals() {
    std::vector<TemperatureReading> out;
    for (int i = 0; i < 32; ++i) {
        std::string base = "/sys/class/thermal/thermal_zone" + std::to_string(i);
        std::ifstream temp_f(base + "/temp");
        int millideg = 0;
        if (!(temp_f >> millideg)) break;
        std::ifstream type_f(base + "/type");
        std::string zone;
        if (!std::getline(type_f, zone) || zone.empty())
            zone = "zone" + std::to_string(i);
        out.push_back({zone, static_cast<double>(millideg) / 1000.0});
    }
    return out;
}

// ── disk inventory (/proc/mounts) ─────────────────────────────────────────────

static std::vector<DiskInfo> build_disk_inventory() {
    static const char* SKIP_FS[] = {
        "sysfs","proc","devtmpfs","devpts","tmpfs","cgroup","cgroup2",
        "pstore","bpf","tracefs","hugetlbfs","mqueue","debugfs","securityfs",
        "fusectl","efivarfs","autofs","ramfs","overlay","squashfs",nullptr
    };

    std::vector<DiskInfo> out;
    std::ifstream mounts("/proc/mounts");
    std::string device, mount, fstype, opts;
    int dump, pass;
    while (mounts >> device >> mount >> fstype >> opts >> dump >> pass) {
        bool skip = false;
        for (int i = 0; SKIP_FS[i]; ++i) {
            if (fstype == SKIP_FS[i]) { skip = true; break; }
        }
        if (skip) continue;
        if (mount.find("/proc") == 0 || mount.find("/sys") == 0 ||
            mount.find("/dev")  == 0 || mount.find("/run") == 0) continue;

        struct statvfs st{};
        if (statvfs(mount.c_str(), &st) != 0) continue;

        DiskInfo d;
        d.mount_point  = mount;
        d.filesystem   = fstype;
        d.volume_label = device;
        d.drive_type   = "Fixed";
        d.ready        = true;

        double total = static_cast<double>(st.f_blocks) * st.f_frsize;
        double free_ = static_cast<double>(st.f_bfree)  * st.f_frsize;
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

class LinuxTelemetryCollector final : public TelemetryCollector {
public:
    LinuxTelemetryCollector() {
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
            auto dt = cur.total() - prev_.total();
            if (dt > 0)
                cpu_pct = 100.0 * static_cast<double>(cur.busy() - prev_.busy())
                                / static_cast<double>(dt);
            prev_ = cur;

            MemInfo mem = read_meminfo();

            std::lock_guard<std::mutex> lk(mtx_);
            snapshot_.cpu_usage_pct   = cpu_pct;
            snapshot_.mem_total_mb    = mem.total_mb;
            snapshot_.mem_avail_mb    = mem.avail_mb;
            snapshot_.mem_used_pct    = mem.used_pct;
            snapshot_.commit_total_mb = mem.commit_total_mb;
            snapshot_.commit_used_mb  = mem.commit_used_mb;
            snapshot_.uptime_seconds  = read_uptime();
            sample_disk(snapshot_);
            snapshot_.temperatures    = read_thermals();
        }
    }

    mutable std::mutex mtx_;
    SensorData         snapshot_;
    CpuTicks           prev_{};
};

}  // namespace

std::unique_ptr<TelemetryCollector> make_default_telemetry_collector() {
    return std::make_unique<LinuxTelemetryCollector>();
}

}  // namespace portfolio::platform

#endif  // __linux__
