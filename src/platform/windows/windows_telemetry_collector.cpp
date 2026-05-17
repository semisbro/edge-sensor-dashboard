#ifdef _WIN32

#define _WIN32_DCOM
#include <windows.h>
#include <wbemidl.h>

#include <chrono>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "portfolio/platform/telemetry_collector.hpp"

namespace portfolio::platform {
namespace {

// ─── RtlGetVersion forward declaration (ntdll, always present on Windows) ────
typedef LONG NTSTATUS;
struct RtlOsVersionInfo {
    ULONG Size;
    ULONG Major;
    ULONG Minor;
    ULONG Build;
    ULONG Platform;
    WCHAR CSDVersion[128];
};
typedef NTSTATUS(WINAPI* RtlGetVersionFn)(RtlOsVersionInfo*);

// ─── Helpers ──────────────────────────────────────────────────────────────────

static ULONGLONG filetime_to_ull(const FILETIME& ft) {
    return (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

static std::string utf8_from_wide(const wchar_t* input, int len = -1) {
    if (!input || (len == 0)) return {};
    const int required = WideCharToMultiByte(
        CP_UTF8, 0, input, len, nullptr, 0, nullptr, nullptr);
    if (required <= 0) return {};
    std::string out(static_cast<size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, 0, input, len, out.data(), required, nullptr, nullptr);
    // Strip embedded null from registry strings
    while (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

static std::string reg_read_sz(HKEY root, const wchar_t* subkey, const wchar_t* value) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, subkey, 0, KEY_READ, &key) != ERROR_SUCCESS) return {};
    wchar_t buf[512] = {};
    DWORD size = sizeof(buf);
    RegQueryValueExW(key, value, nullptr, nullptr, reinterpret_cast<LPBYTE>(buf), &size);
    RegCloseKey(key);
    std::string result = utf8_from_wide(buf);
    // Trim leading/trailing spaces (common in ProcessorNameString)
    auto first = result.find_first_not_of(' ');
    auto last  = result.find_last_not_of(' ');
    return (first == std::string::npos) ? "" : result.substr(first, last - first + 1);
}

static std::string read_cpu_vendor() {
    return reg_read_sz(
        HKEY_LOCAL_MACHINE,
        L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        L"VendorIdentifier");
}

static std::string read_cpu_brand() {
    return reg_read_sz(
        HKEY_LOCAL_MACHINE,
        L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        L"ProcessorNameString");
}

static std::uint32_t read_cpu_base_freq_mhz() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(
            HKEY_LOCAL_MACHINE,
            L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
            0, KEY_READ, &key) != ERROR_SUCCESS) return 0;
    DWORD mhz = 0, size = sizeof(mhz);
    RegQueryValueExW(key, L"~MHz", nullptr, nullptr,
                     reinterpret_cast<LPBYTE>(&mhz), &size);
    RegCloseKey(key);
    return static_cast<std::uint32_t>(mhz);
}

static std::string read_architecture() {
    SYSTEM_INFO si{};
    GetNativeSystemInfo(&si);
    switch (si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: return "x64";
        case PROCESSOR_ARCHITECTURE_ARM64: return "ARM64";
        case PROCESSOR_ARCHITECTURE_ARM:   return "ARM32";
        case PROCESSOR_ARCHITECTURE_INTEL: return "x86";
        default: return "unknown";
    }
}

static std::uint32_t read_logical_core_count() {
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    return static_cast<std::uint32_t>(si.dwNumberOfProcessors);
}

// Fills physical_cores, l2_cache_kb (per-core), l3_cache_kb (total shared).
static void read_cpu_topology(std::uint32_t& physical_cores,
                               std::uint32_t& l2_cache_kb,
                               std::uint32_t& l3_cache_kb) {
    DWORD length = 0;
    GetLogicalProcessorInformation(nullptr, &length);
    if (length == 0) return;

    std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> buf(
        length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
    if (!GetLogicalProcessorInformation(buf.data(), &length)) return;

    bool l2_set = false;
    for (const auto& info : buf) {
        switch (info.Relationship) {
            case RelationProcessorCore:
                ++physical_cores;
                break;
            case RelationCache:
                if (info.Cache.Level == 2 && !l2_set) {
                    l2_cache_kb = info.Cache.Size / 1024;
                    l2_set = true;
                } else if (info.Cache.Level == 3) {
                    l3_cache_kb += info.Cache.Size / 1024;
                }
                break;
            default:
                break;
        }
    }
}

static std::string read_os_product_name() {
    return reg_read_sz(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
        L"ProductName");
}

static std::string read_os_display_version() {
    // "24H2", "23H2", etc.  Falls back to ReleaseId on older builds.
    std::string v = reg_read_sz(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
        L"DisplayVersion");
    if (v.empty()) {
        v = reg_read_sz(
            HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
            L"ReleaseId");
    }
    return v;
}

static std::string read_os_build() {
    // Use RtlGetVersion for the true build number (GetVersion lies on newer Windows)
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll) {
        auto fn = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"));
        if (fn) {
            RtlOsVersionInfo ovi{};
            ovi.Size = sizeof(ovi);
            if (fn(&ovi) == 0) {
                return std::to_string(ovi.Build);
            }
        }
    }
    // Fallback: registry
    return reg_read_sz(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
        L"CurrentBuildNumber");
}

static std::string read_hostname() {
    wchar_t buf[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD len = static_cast<DWORD>(std::size(buf));
    if (GetComputerNameW(buf, &len)) return utf8_from_wide(buf);
    return "unknown";
}

static bool is_running_as_admin() {
    BOOL elevated = FALSE;
    HANDLE token  = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION elev{};
        DWORD size = sizeof(elev);
        if (GetTokenInformation(token, TokenElevation, &elev, size, &size)) {
            elevated = elev.TokenIsElevated;
        }
        CloseHandle(token);
    }
    return elevated == TRUE;
}

static std::string drive_type_name(UINT t) {
    switch (t) {
        case DRIVE_FIXED:    return "fixed";
        case DRIVE_REMOVABLE:return "removable";
        case DRIVE_REMOTE:   return "network";
        case DRIVE_CDROM:    return "cdrom";
        case DRIVE_RAMDISK:  return "ramdisk";
        default:             return "unknown";
    }
}

static std::vector<DiskInfo> read_disk_inventory() {
    DWORD required = GetLogicalDriveStringsW(0, nullptr);
    if (required == 0) return {};
    std::wstring buf(static_cast<size_t>(required), L'\0');
    if (GetLogicalDriveStringsW(required, buf.data()) == 0) return {};

    constexpr double bytes_per_gb = 1024.0 * 1024.0 * 1024.0;
    std::vector<DiskInfo> disks;

    for (const wchar_t* cur = buf.c_str(); *cur; cur += wcslen(cur) + 1) {
        std::wstring root(cur);
        DiskInfo disk;
        disk.mount_point = utf8_from_wide(root.c_str());
        disk.drive_type  = drive_type_name(GetDriveTypeW(root.c_str()));

        wchar_t vol[MAX_PATH + 1] = {}, fs[MAX_PATH + 1] = {};
        if (GetVolumeInformationW(root.c_str(), vol, MAX_PATH,
                                   nullptr, nullptr, nullptr, fs, MAX_PATH)) {
            disk.volume_label = utf8_from_wide(vol);
            disk.filesystem   = utf8_from_wide(fs);
        }

        ULARGE_INTEGER avail{}, total{}, free{};
        if (GetDiskFreeSpaceExW(root.c_str(), &avail, &total, &free)) {
            disk.ready    = true;
            disk.total_gb = static_cast<double>(total.QuadPart) / bytes_per_gb;
            disk.free_gb  = static_cast<double>(avail.QuadPart) / bytes_per_gb;
            disk.used_gb  = disk.total_gb - disk.free_gb;
            disk.used_pct = (disk.total_gb > 0.0)
                                ? 100.0 * (disk.used_gb / disk.total_gb)
                                : 0.0;
        }
        disks.push_back(std::move(disk));
    }
    return disks;
}

// ─── Collector ────────────────────────────────────────────────────────────────

class WindowsTelemetryCollector final : public TelemetryCollector {
public:
    void start() override {
        std::lock_guard<std::mutex> lock(start_mutex_);
        if (started_) return;
        started_ = true;

        // ── Read static system info before any background threads start ──────
        cpu_static_.vendor        = read_cpu_vendor();
        cpu_static_.brand         = read_cpu_brand();
        cpu_static_.architecture  = read_architecture();
        cpu_static_.logical_cores = read_logical_core_count();
        cpu_static_.base_freq_mhz = read_cpu_base_freq_mhz();
        read_cpu_topology(cpu_static_.physical_cores,
                          cpu_static_.l2_cache_kb,
                          cpu_static_.l3_cache_kb);

        os_info_.name            = read_os_product_name();
        os_info_.display_version = read_os_display_version();
        os_info_.build           = read_os_build();
        os_info_.hostname        = read_hostname();
        os_info_.architecture    = cpu_static_.architecture;
        os_info_.is_elevated     = is_running_as_admin();

        {
            std::lock_guard<std::mutex> data_lock(data_mutex_);
            sensors_.cpu_info = cpu_static_;
            sensors_.os_info  = os_info_;
        }

        std::thread(&WindowsTelemetryCollector::system_monitor_loop, this).detach();
        std::thread(&WindowsTelemetryCollector::temperature_monitor_loop, this).detach();
    }

    SensorData sensor_snapshot() const override {
        std::lock_guard<std::mutex> lock(data_mutex_);
        return sensors_;
    }

    std::vector<DiskInfo> disk_inventory_snapshot() const override {
        return read_disk_inventory();
    }

private:
    void system_monitor_loop() {
        FILETIME idle_prev{}, kernel_prev{}, user_prev{};
        GetSystemTimes(&idle_prev, &kernel_prev, &user_prev);

        constexpr double MB = 1024.0 * 1024.0;
        constexpr double GB = MB * 1024.0;

        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // ── CPU usage ────────────────────────────────────────────────────
            FILETIME idle_cur{}, kernel_cur{}, user_cur{};
            GetSystemTimes(&idle_cur, &kernel_cur, &user_cur);

            const ULONGLONG idle   = filetime_to_ull(idle_cur)   - filetime_to_ull(idle_prev);
            const ULONGLONG kernel = filetime_to_ull(kernel_cur) - filetime_to_ull(kernel_prev);
            const ULONGLONG user   = filetime_to_ull(user_cur)   - filetime_to_ull(user_prev);
            const ULONGLONG total  = kernel + user;
            const double cpu_pct   = (total > 0)
                ? 100.0 * (1.0 - static_cast<double>(idle) / static_cast<double>(total))
                : 0.0;

            idle_prev   = idle_cur;
            kernel_prev = kernel_cur;
            user_prev   = user_cur;

            // ── Memory ───────────────────────────────────────────────────────
            MEMORYSTATUSEX mem{};
            mem.dwLength = sizeof(mem);
            GlobalMemoryStatusEx(&mem);

            // ── Disk (C:) ────────────────────────────────────────────────────
            ULARGE_INTEGER d_free{}, d_total{}, d_total_free{};
            GetDiskFreeSpaceExW(L"C:\\", &d_free, &d_total, &d_total_free);
            const double disk_total = static_cast<double>(d_total.QuadPart) / GB;
            const double disk_free  = static_cast<double>(d_free.QuadPart)  / GB;
            const double disk_used_pct = (disk_total > 0.0)
                ? 100.0 * (1.0 - disk_free / disk_total) : 0.0;

            // ── Build snapshot ───────────────────────────────────────────────
            SensorData next;
            next.cpu_usage_pct    = cpu_pct;
            next.cpu_info         = cpu_static_;
            next.os_info          = os_info_;

            next.mem_total_mb     = static_cast<double>(mem.ullTotalPhys) / MB;
            next.mem_avail_mb     = static_cast<double>(mem.ullAvailPhys) / MB;
            next.mem_used_pct     = static_cast<double>(mem.dwMemoryLoad);
            next.commit_total_mb  = static_cast<double>(mem.ullTotalPageFile) / MB;
            next.commit_used_mb   = static_cast<double>(
                mem.ullTotalPageFile - mem.ullAvailPageFile) / MB;

            next.disk_mount_point = "C:";
            next.disk_total_gb    = disk_total;
            next.disk_free_gb     = disk_free;
            next.disk_used_pct    = disk_used_pct;
            next.uptime_seconds   = static_cast<double>(GetTickCount64()) / 1000.0;

            {
                std::lock_guard<std::mutex> data_lock(data_mutex_);
                next.temperatures = sensors_.temperatures;
                sensors_ = std::move(next);
            }
        }
    }

    void temperature_monitor_loop() {
        if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return;

        CoInitializeSecurity(nullptr, -1, nullptr, nullptr,
                             RPC_C_AUTHN_LEVEL_DEFAULT,
                             RPC_C_IMP_LEVEL_IMPERSONATE,
                             nullptr, EOAC_NONE, nullptr);

        IWbemLocator* locator = nullptr;
        if (FAILED(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                                     IID_IWbemLocator,
                                     reinterpret_cast<LPVOID*>(&locator)))) {
            CoUninitialize();
            return;
        }

        IWbemServices* services = nullptr;
        BSTR wmi_path = SysAllocString(L"ROOT\\WMI");
        const HRESULT connect_result = locator->ConnectServer(
            wmi_path, nullptr, nullptr, nullptr, 0, nullptr, nullptr, &services);
        SysFreeString(wmi_path);

        if (FAILED(connect_result)) {
            locator->Release();
            CoUninitialize();
            return;
        }

        CoSetProxyBlanket(services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                          RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
                          nullptr, EOAC_NONE);

        while (true) {
            std::vector<TemperatureReading> temps;
            IEnumWbemClassObject* enumerator = nullptr;

            BSTR ql  = SysAllocString(L"WQL");
            BSTR q   = SysAllocString(
                L"SELECT CurrentTemperature FROM MSAcpi_ThermalZoneTemperature");
            const HRESULT qr = services->ExecQuery(
                ql, q,
                WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                nullptr, &enumerator);
            SysFreeString(ql);
            SysFreeString(q);

            if (SUCCEEDED(qr) && enumerator) {
                IWbemClassObject* obj = nullptr;
                ULONG returned = 0;
                std::size_t idx = 0;

                while (enumerator->Next(WBEM_INFINITE, 1, &obj, &returned) == S_OK) {
                    VARIANT val{};
                    if (SUCCEEDED(obj->Get(L"CurrentTemperature", 0, &val, nullptr, nullptr))
                        && val.vt == VT_I4) {
                        TemperatureReading r;
                        r.zone    = "zone_" + std::to_string(idx++);
                        r.celsius = (static_cast<double>(val.lVal) / 10.0) - 273.15;
                        temps.push_back(std::move(r));
                    }
                    VariantClear(&val);
                    obj->Release();
                }
                enumerator->Release();
            }

            {
                std::lock_guard<std::mutex> lock(data_mutex_);
                sensors_.temperatures = std::move(temps);
            }

            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }

    mutable std::mutex data_mutex_;
    std::mutex         start_mutex_;
    bool               started_ = false;
    SensorData         sensors_{};
    CpuStaticInfo      cpu_static_{};
    OsInfo             os_info_{};
};

}  // namespace

std::unique_ptr<TelemetryCollector> make_default_telemetry_collector() {
    return std::make_unique<WindowsTelemetryCollector>();
}

}  // namespace portfolio::platform

#endif
