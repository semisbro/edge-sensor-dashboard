export const MOCK_SENSORS = {
  cpu: {
    usage_percent:  34.7,
    logical_cores:  16,
    physical_cores: 8,
    base_freq_mhz:  3600,
    max_freq_mhz:   5200,
    vendor:         "GenuineIntel",
    brand:          "Intel(R) Core(TM) i9-13900K @ 3.00GHz",
    architecture:   "x64",
    l2_cache_kb:    2048,   // 2 MB per P-core
    l3_cache_kb:    36864,  // 36 MB shared
  },
  memory: {
    total_mb:        32768,
    available_mb:    14336,
    used_percent:    56.2,
    commit_total_mb: 49152,
    commit_used_mb:  28672,
  },
  disk: {
    drive:        "C:",
    total_gb:     512.0,
    free_gb:      187.4,
    used_percent: 63.4,
  },
  temperatures: [
    { zone: "zone_0", celsius: 48.3 },
    { zone: "zone_1", celsius: 52.1 },
    { zone: "zone_2", celsius: 44.7 },
    { zone: "zone_3", celsius: 39.2 },
  ],
  system: {
    uptime_seconds: 15 * 86400 + 6 * 3600 + 23 * 60 + 14,
  },
  os: {
    name:            "Windows 11 Pro",
    display_version: "24H2",
    build:           "26100",
    hostname:        "EDGE-NODE-01",
    architecture:    "x64",
    is_elevated:     true,
  },
};

export const MOCK_DISKS = {
  count: 3,
  disks: [
    {
      mount_point:  "C:\\",
      drive_type:   "fixed",
      volume_label: "System",
      filesystem:   "NTFS",
      ready:        true,
      total_gb:     512.0,
      free_gb:      187.4,
      used_gb:      324.6,
      used_percent: 63.4,
    },
    {
      mount_point:  "D:\\",
      drive_type:   "fixed",
      volume_label: "DataStore",
      filesystem:   "NTFS",
      ready:        true,
      total_gb:     2000.0,
      free_gb:      1243.8,
      used_gb:      756.2,
      used_percent: 37.8,
    },
    {
      mount_point:  "E:\\",
      drive_type:   "removable",
      volume_label: "FieldBackup",
      filesystem:   "exFAT",
      ready:        true,
      total_gb:     128.0,
      free_gb:      42.1,
      used_gb:      85.9,
      used_percent: 67.1,
    },
  ],
};
