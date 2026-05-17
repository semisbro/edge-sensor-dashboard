# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project purpose

Portfolio PoC simulating a hardware-adjacent edge sensor node that streams telemetry via REST to a React dashboard. Target audience: aerospace/defense hiring (Elbit Systems, Thales). Target deployment hardware: ARM Linux (Raspberry Pi), though active development is Windows.

## Build — C++ backend

```bash
# Configure (from repo root)
cmake -B build -S .

# Build
cmake --build build

# Run (default port 18080; override with PORT env var)
./build/portfolio_cpp
```

On Windows CMake links `ws2_32 mswsock wbemuuid ole32 oleaut32` automatically. No manual steps needed.

## Frontend

```bash
cd frontend

# Dev server — hot reload, proxies /api and /openapi.json to localhost:18080
yarn dev          # http://localhost:5173

# Production build — output goes to frontend/dist/
yarn build

# Preview the production build
yarn preview      # http://localhost:4173
```

## Architecture

The project has three layers, each in its own namespace:

### 1. Platform layer — `portfolio::platform`

`include/portfolio/platform/telemetry_collector.hpp` defines the abstract `TelemetryCollector` interface (`start()`, `sensor_snapshot()`, `disk_inventory_snapshot()`). The factory function `make_default_telemetry_collector()` is **conditionally compiled**:

- `src/platform/windows/windows_telemetry_collector.cpp` (`#ifdef _WIN32`) — real implementation using WinAPI (`GetSystemTimes`, `GlobalMemoryStatusEx`, `GetDiskFreeSpaceExW`, registry for CPU freq) and WMI for thermal zones. Runs two detached background threads: `system_monitor_loop` (1 s interval) and `temperature_monitor_loop` (5 s interval). Thread-safe via `data_mutex_`.
- `src/platform/fallback/null_telemetry_collector.cpp` (`#ifndef _WIN32`) — stub that returns zero-value structs. **There is no Linux implementation yet** — this is the next major work item.

Data structs live in `include/portfolio/telemetry_models.hpp`: `SensorData`, `TemperatureReading`, `DiskInfo`.

### 2. Web layer — `portfolio::web`

`src/web/app_routes.cpp` wires all Crow routes via `configure_routes()`:

| Route | Notes |
|---|---|
| `GET /` | Serves `frontend/dist/index.html` if built, else `/api/meta` JSON |
| `GET /api/sensors` | Live `SensorData` snapshot |
| `GET /api/disks` | Live disk inventory |
| `GET /api/meta` | Service metadata |
| `GET /api/hello/<string>` | Sanity-check echo |
| `GET /openapi.json` | OpenAPI document |
| `GET /assets/<path>` | Static Vite assets |
| `GET /<path>` | SPA fallback to `index.html` (skips paths with file extensions) |

`src/web/frontend_assets.cpp` resolves paths and detects whether `frontend/dist/index.html` exists. Port is read from `PORT` env var, defaulting to `18080`.

### 3. Presentation layer — React frontend

`frontend/src/api/sensors.js` fetches `GET /api/sensors` and polls every **2 s** via TanStack React Query. `frontend/src/App.jsx` is the single view: hero/status header, 8 stat cards, thermal zones panel. No router.

In development, Vite proxies `/api` and `/openapi.json` to `localhost:18080` so the C++ backend must be running. In production, the C++ server serves `frontend/dist/` directly — no separate web server needed.

## Code conventions (C++)

- Strict C++17 — no C++20 features. Must remain compilable on older/embedded toolchains.
- Smart pointers throughout; no raw owning pointers.
- All syscalls must be fault-tolerant (check return values, `try-catch` where appropriate).
- Console logging format: `[SYS-LOG] [INFO] <message>`.
- No external C++ dependencies beyond Crow and Asio (vendored in `third_party/`).

## Key gaps / next work

- **Linux telemetry** — replace `NullTelemetryCollector` with a real implementation reading `/proc/stat`, `/proc/meminfo`, `/sys/class/thermal/`, etc.
- **Docker** — multi-stage build (C++ compile stage → minimal runtime image).
- **Space Grotesk font** — declared in `frontend/src/styles.css` but never loaded; add a `<link>` to `index.html` or a local `@font-face`.
