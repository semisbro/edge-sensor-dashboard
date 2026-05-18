# portfolio_cpp — Live Edge Sensor Dashboard

> **Full-stack systems project** · C++17 backend · React 18 frontend · zero cloud dependencies

Think of a sensor box bolted to a satellite ground station or a battlefield vehicle — it monitors every heartbeat of the hardware and streams that data to a command dashboard in real time. This project is exactly that, running on your own machine.

One self-contained binary. No cloud. No Docker required to try it. Just build and run.

---

## Why This Project Exists

Off-the-shelf monitoring tools are black boxes. This project goes one layer deeper:

- **Talks directly to the OS** — WinAPI, WMI, and the Windows Registry on Windows; Mach kernel APIs and sysctl on macOS; `/proc`/`/sys` on Linux/ARM; no third-party agent in between
- **Ships its own web server** — the C++ binary serves the React dashboard as static files; no Nginx, no Node server in front of it
- **Designed for embedded targets** — strict C++17, smart pointers only, no exceptions across API boundaries; the same binary is meant to compile for ARM Linux (Raspberry Pi)

---

## What It Does

- Samples CPU usage, memory pressure, disk capacity, and thermal zone temperatures — updated every second in the background
- Exposes everything over a clean REST/JSON API with an OpenAPI spec served at runtime
- Drives a live React dashboard that auto-refreshes every 2 seconds via polling
- Runs fully offline — no external calls, no telemetry sent anywhere

---

## Tech Stack

**Backend — where the interesting bits are**
- **C++17** — mutex-guarded background threads; 1 s system-metrics loop, 5 s WMI thermal loop (Windows); snapshot pattern keeps the HTTP path lock-free across all platforms
- **[Crow](https://crowcpp.org)** — lightweight C++ HTTP micro-framework (think Express, but a single header)
- **Asio** — async I/O without Boost; vendored directly, no install needed

**Frontend**
- **React 18 + Vite** — fast build, small bundle
- **TanStack React Query** — polling, caching, and stale-data management out of the box

**Build**
- **CMake 3.15+** — platform-conditional compilation; the right collector is selected at build time, not runtime
- **Yarn** — frontend only

---

## Dashboard at a Glance

- **Overview** — CPU %, memory used/available/commit, primary disk, uptime counter
- **Disks** — full logical drive inventory with capacity bars, filesystem type, and volume labels
- **Thermal** — per-zone temperature cards color-coded by severity; alert banner fires above 60 °C
- **System** — OS version, build number, hostname, CPU brand, physical/logical cores, L2/L3 cache, privilege level
- **Server health chip** — live/offline dot + round-trip latency in ms, always visible in the navbar
- **Demo mode** — swaps live data for realistic static values in one click; great for presentations without a running backend
- **Dark / Light theme** — persisted to `localStorage`

---

## How It's Built (Three Layers)

```
[ React dashboard ]  ←─── HTTP/JSON, polls every 2 s ───→  [ Crow HTTP server ]
                                                                    │
                                                        [ TelemetryCollector ]
                                                         abstract interface;
                                                         Windows / macOS / Linux
                                                         impl selected at compile time
```

| Layer | Responsibility |
|---|---|
| **Platform** | OS-specific data collection; `make_default_telemetry_collector()` factory wired via `#ifdef` — real implementations for Windows, macOS, and Linux/ARM |
| **Web** | Crow routes, JSON serialisation, OpenAPI doc, SPA fallback, static file serving |
| **Frontend** | React app — served by the binary in production, proxied via Vite in dev |

---

## API

Default port `18080` — override with the `PORT` environment variable.

| Endpoint | Returns |
|---|---|
| `GET /` | React SPA (falls back to service metadata if frontend isn't built) |
| `GET /api/sensors` | Live snapshot — CPU, memory, disk, thermals, OS info |
| `GET /api/disks` | Full logical drive inventory |
| `GET /api/meta` | Service name, framework, endpoint list |
| `GET /openapi.json` | OpenAPI 3.0 spec, served at runtime |

---

## Quick Start

**You need:** CMake 3.15+, a C++17 compiler, Node.js, Yarn

```bash
# 1 — build the backend
cmake -S . -B build
cmake --build build

# 2 — build the frontend  (optional — the binary works without it)
cd frontend && yarn install && yarn build && cd ..

# 3 — run
./build/portfolio_cpp           # macOS / Linux
.\build\portfolio_cpp.exe       # Windows  (UAC prompt = needed for WMI thermal access)
```

Go to `http://localhost:18080`. That's it.

**Live frontend dev (hot reload):**
```bash
cd frontend && yarn dev    # http://localhost:5173  — proxies /api to the C++ backend
```

---

## Helper Scripts

Two convenience scripts live at the repo root.

### `run_crow_server_in_foreground.sh`

Builds the backend if the binary is missing, checks that the port is free, then runs the server in the foreground with coloured log output. `Ctrl+C` shuts it down cleanly.

```bash
./run_crow_server_in_foreground.sh          # default port 18080
PORT=9090 ./run_crow_server_in_foreground.sh   # custom port
```

Use this instead of calling the binary directly — it handles the build step and port-collision check for you.

### `build_index_entry_for_frontend.sh`

Builds the frontend with `--base ./` (relative asset paths) and copies the output to the repo root (`index.html` + `assets/`) for **GitHub Pages deployment**. Not needed for normal local development.

```bash
./build_index_entry_for_frontend.sh
# or, pointing at a different backend host:
VITE_CROW_URL=my-host:18080 ./build_index_entry_for_frontend.sh
```

---

## Project Layout

```
portfolio_cpp/
├── main.cpp                          Entry point + Windows UAC elevation logic
├── CMakeLists.txt
├── include/portfolio/
│   ├── telemetry_models.hpp          SensorData, CpuStaticInfo, OsInfo, DiskInfo
│   ├── platform/telemetry_collector.hpp   Abstract interface + factory
│   └── web/                          Route, payload, and asset headers
├── src/
│   ├── platform/windows/             WinAPI + WMI — CPU, RAM, disk, thermals, registry
│   ├── platform/macos/               Mach + sysctl — CPU, RAM, disk, OS; thermals in progress
│   ├── platform/linux/               /proc + /sys — CPU, RAM, disk, thermals, OS; ARM-compatible
│   └── web/                          Routes, JSON payloads, static file serving
├── frontend/src/
│   ├── api/                          sensors.js · disks.js · ping.js
│   ├── mockData.js                   Demo mode static data
│   └── App.jsx
└── third_party/                      Crow + Asio vendored — nothing to install
```

---

## Platform Status

| Platform | CPU | RAM | Disk | Thermals | Notes |
|---|---|---|---|---|---|
| **Windows** | ✅ WinAPI | ✅ WinAPI | ✅ WinAPI | ✅ WMI | UAC prompt needed for thermal access |
| **macOS** | ✅ Mach | ✅ Mach VM | ✅ statfs | 🔜 SMC | Apple SMC requires elevated reads — in progress |
| **Linux / ARM** | ✅ /proc/stat | ✅ /proc/meminfo | ✅ statvfs | ✅ /sys/class/thermal | Runs on Raspberry Pi (aarch64 + armv7) |

---

## What's Next

- **macOS thermals** — SMC temperature reads via IOKit; requires `sudo` or a helper daemon; implementation in progress
- **Elevated reads helper** — privilege escalation path for macOS SMC and any future Linux hwmon reads that need root
- **Docker** — multi-stage build: C++ compile stage → minimal runtime image with frontend bundled in
- **GPS + RF simulation** — mock coordinates and signal strength for a more complete C4I scenario demo
