# portfolio_cpp — Tactical Edge Node Dashboard

A full-stack systems telemetry project built for an aerospace / defense portfolio.
A C++17 service collects live hardware metrics and streams them over a REST API;
a React dashboard visualises the data in real time.

The concept mirrors a field-deployed edge node feeding a C4I command dashboard —
compact binary, no runtime dependencies, self-hosted frontend.

---

## Tech Stack

| Layer | Technology |
|---|---|
| Backend | C++17 · [Crow](https://crowcpp.org) HTTP micro-framework · Asio (async I/O) |
| System APIs | WinAPI · WMI · Windows Registry · `GetLogicalProcessorInformation` |
| Frontend | React 18 · Vite 5 · TanStack React Query v5 |
| Build | CMake 3.15+ · Yarn · multi-stage Docker image |

---

## What This Project Demonstrates

- **Systems-level C++17** — two background threads collecting CPU, memory, disk and thermal data; mutex-guarded snapshot pattern; smart-pointer ownership throughout
- **Windows internals** — registry reads, `GetLogicalProcessorInformation` for CPU topology, `RtlGetVersion` for true OS build number, WMI thermal queries, UAC elevation via embedded manifest
- **REST API design** — Crow routes, JSON serialisation, OpenAPI 3.0 document served at runtime, SPA fallback routing, static file serving
- **Modern React** — TanStack React Query for polling / cache management, dark/light theming via CSS custom properties, tab navigation, responsive layout
- **Full-stack integration** — Vite dev-proxy in development; production build served directly by the C++ binary with no web server in front of it

---

## Dashboard Features

| Feature | Detail |
|---|---|
| **4 tabs** | Overview · Disks · Thermal · System |
| **Overview** | CPU usage + clock · memory used/available/commit · primary disk · uptime |
| **Disks** | Full logical drive inventory — mount point, filesystem, volume label, capacity bars |
| **Thermal** | Per-zone temperature cards, colour-coded by severity, alert banner above 60 °C |
| **System** | OS name, version, build, hostname, privilege level · CPU brand, vendor, architecture, physical/logical cores, base/boost clocks, L2 and L3 cache |
| **Server health** | Navbar chip shows Crow backend URL, live/offline dot, and round-trip latency (ms) |
| **Demo mode** | One-click toggle replaces live data with static realistic values — useful for presentations when no backend is running |
| **Dark / Light theme** | Persisted to `localStorage`; dark is the default |
| **UAC elevation** | Manifest embedded in the `.exe` requests Administrator on launch, which is required for WMI thermal sensor access |

---

## API

The service starts on port `18080` by default; override with the `PORT` environment variable.

| Endpoint | Description |
|---|---|
| `GET /` | Serves the React SPA if `frontend/dist/` is present, otherwise returns service metadata |
| `GET /api/sensors` | Live telemetry snapshot (see shape below) |
| `GET /api/disks` | Full logical drive inventory |
| `GET /api/meta` | Service name, framework, endpoint list |
| `GET /openapi.json` | OpenAPI 3.0 document |
| `GET /api/hello/<name>` | Sanity-check echo endpoint |

### `GET /api/sensors` — response shape

```json
{
  "cpu": {
    "usage_percent": 34.7,
    "logical_cores": 16,
    "physical_cores": 8,
    "base_freq_mhz": 3600,
    "max_freq_mhz": 5200,
    "vendor": "GenuineIntel",
    "brand": "Intel(R) Core(TM) i9-13900K @ 3.00GHz",
    "architecture": "x64",
    "l2_cache_kb": 2048,
    "l3_cache_kb": 36864
  },
  "memory": {
    "total_mb": 32768.0,
    "available_mb": 14336.0,
    "used_percent": 56.2,
    "commit_total_mb": 49152.0,
    "commit_used_mb": 28672.0
  },
  "disk": {
    "drive": "C:",
    "total_gb": 512.0,
    "free_gb": 187.4,
    "used_percent": 63.4
  },
  "temperatures": [
    { "zone": "zone_0", "celsius": 48.3 }
  ],
  "system": {
    "uptime_seconds": 1323794.0
  },
  "os": {
    "name": "Windows 11 Pro",
    "display_version": "24H2",
    "build": "26100",
    "hostname": "EDGE-NODE-01",
    "architecture": "x64",
    "is_elevated": true
  }
}
```

---

## Architecture

```
┌──────────────────────────────────────────────────┐
│  Presentation layer  (React 18 + Vite)           │
│  Polls /api/sensors every 2 s via React Query    │
│  Polls /api/disks every 10 s                     │
│  Pings /api/meta every 5 s for health status     │
└─────────────────────┬────────────────────────────┘
                      │ HTTP / JSON
┌─────────────────────▼────────────────────────────┐
│  Network layer  (Crow + Asio)                    │
│  configure_routes() wires all endpoints          │
│  Serves frontend/dist/ as static files           │
│  Emits OpenAPI document at /openapi.json         │
└─────────────────────┬────────────────────────────┘
                      │ C++ structs
┌─────────────────────▼────────────────────────────┐
│  Platform layer  (TelemetryCollector interface)  │
│  WindowsTelemetryCollector  — two background     │
│  threads: system metrics (1 s) + WMI thermal     │
│  (5 s). Static CPU/OS info read once at start.   │
│  NullTelemetryCollector     — non-Windows stub   │
└──────────────────────────────────────────────────┘
```

Platform implementations are selected at **compile time** via `#ifdef _WIN32` — the same factory function `make_default_telemetry_collector()` is used on all platforms.

---

## Quick Start

### Prerequisites

- CMake 3.15+, a C++17 compiler (MSVC or MinGW-w64)
- Node.js + Yarn (for the frontend)
- Crow and Asio are vendored in `third_party/` — no package manager needed

### Build and run

```powershell
# 1. Build the C++ backend
cmake -S . -B build
cmake --build build

# 2. Build the frontend (optional — the binary works without it)
cd frontend
yarn install
yarn build
cd ..

# 3. Run (UAC prompt will appear — required for thermal sensors)
.\build\portfolio_cpp.exe
```

The dashboard is then available at `http://localhost:18080`.

### Frontend dev server

```powershell
cd frontend
yarn dev        # http://localhost:5173
                # Proxies /api and /openapi.json → localhost:18080
```

### Docker (Linux target)

```bash
docker build -t portfolio_cpp .
docker run -p 18080:18080 portfolio_cpp
```

> The Docker image targets Linux / Ubuntu and uses the `NullTelemetryCollector` (no Windows APIs available). The Linux sensor implementation is on the roadmap.

---

## Roadmap

- **Linux telemetry** — replace `NullTelemetryCollector` with a real implementation reading `/proc/stat`, `/proc/meminfo` and `/sys/class/thermal/`; enables Raspberry Pi / ARM deployment
- **GPS + signal simulation** — mock geo-coordinates and RF signal strength for a more complete C4I mockup
- **Docker parity** — extend the multi-stage image to also bundle the built React frontend

---

## Project Layout

```
portfolio_cpp/
├── main.cpp                          Entry point
├── CMakeLists.txt
├── portfolio_cpp.manifest            UAC elevation manifest
├── portfolio_cpp.rc                  Embeds manifest into the .exe
├── include/portfolio/
│   ├── telemetry_models.hpp          SensorData, CpuStaticInfo, OsInfo, DiskInfo
│   ├── platform/telemetry_collector.hpp
│   └── web/
├── src/
│   ├── platform/windows/             WindowsTelemetryCollector
│   ├── platform/fallback/            NullTelemetryCollector (Linux / CI)
│   └── web/                          Routes, payloads, static file serving
├── frontend/                         React 18 + Vite dashboard
│   └── src/
│       ├── api/                      sensors.js · disks.js · ping.js
│       ├── config.js                 CROW_SERVER_URL
│       ├── mockData.js               Static demo data
│       └── App.jsx
├── third_party/crow/                 Vendored — no install needed
└── third_party/asio/
```
