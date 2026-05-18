#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace portfolio::platform {

struct CpuStaticInfo {
    std::string   vendor;
    std::string   brand;
    std::string   architecture;
    std::uint32_t logical_cores  = 0;
    std::uint32_t physical_cores = 0;
    std::uint32_t base_freq_mhz  = 0;
    std::uint32_t max_freq_mhz   = 0;
    std::uint32_t l2_cache_kb    = 0;
    std::uint32_t l3_cache_kb    = 0;
};

struct OsInfo {
    std::string name;
    std::string display_version;
    std::string build;
    std::string hostname;
    std::string architecture;
    bool        is_elevated = false;
};

struct TemperatureReading {
    std::string zone;
    double      celsius = 0.0;
};

struct DiskInfo {
    std::string mount_point;
    std::string drive_type;
    std::string volume_label;
    std::string filesystem;
    bool        ready    = false;
    double      total_gb = 0.0;
    double      free_gb  = 0.0;
    double      used_gb  = 0.0;
    double      used_pct = 0.0;
};

struct SensorData {
    double        cpu_usage_pct   = 0.0;
    CpuStaticInfo cpu_info        = {};
    OsInfo        os_info         = {};

    double mem_total_mb    = 0.0;
    double mem_avail_mb    = 0.0;
    double mem_used_pct    = 0.0;
    double commit_total_mb = 0.0;
    double commit_used_mb  = 0.0;

    std::string disk_mount_point;
    double      disk_total_gb  = 0.0;
    double      disk_free_gb   = 0.0;
    double      disk_used_pct  = 0.0;

    std::vector<TemperatureReading> temperatures;

    double uptime_seconds = 0.0;
};

}  // namespace portfolio::platform
