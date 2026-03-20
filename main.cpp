#include "crow.h"

// Must come before WMI headers
#define _WIN32_DCOM
#include <windows.h>
#include <wbemidl.h>
#include <pdh.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// ============================================================================
// Global sensor cache — written by background threads, read by route handlers
// ============================================================================

struct SensorData {
    // CPU
    double   cpu_usage_pct   = 0.0;
    DWORD    cpu_core_count  = 0;
    DWORD    cpu_freq_mhz    = 0;

    // Memory
    double   mem_total_mb    = 0.0;
    double   mem_avail_mb    = 0.0;
    double   mem_used_pct    = 0.0;
    double   commit_total_mb = 0.0;
    double   commit_used_mb  = 0.0;

    // Disk (system drive, typically C:\)
    double   disk_total_gb   = 0.0;
    double   disk_free_gb    = 0.0;
    double   disk_used_pct   = 0.0;

    // Temperatures (one entry per WMI thermal zone, in °C)
    std::vector<double> temperatures_c;

    // System
    double   uptime_seconds  = 0.0;
};

static SensorData  g_sensors;
static std::mutex  g_sensors_mtx;

// ============================================================================
// Helpers
// ============================================================================

namespace {

int read_port() {
    if (const char* p = std::getenv("PORT")) return std::stoi(p);
    return 18080;
}

ULONGLONG filetime_to_ull(const FILETIME& ft) {
    return (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

// Read CPU base frequency from registry (HKLM\HARDWARE\...\CentralProcessor\0)
DWORD read_cpu_freq_mhz() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                      0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return 0;

    DWORD mhz = 0;
    DWORD size = sizeof(mhz);
    RegQueryValueExW(hKey, L"~MHz", nullptr, nullptr,
                     reinterpret_cast<LPBYTE>(&mhz), &size);
    RegCloseKey(hKey);
    return mhz;
}

// Read logical processor count
DWORD read_core_count() {
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    return si.dwNumberOfProcessors;
}

// ============================================================================
// Background thread: CPU + Memory + Disk + Uptime  (every 1 s)
// ============================================================================

void system_monitor() {
    FILETIME idle_prev{}, kernel_prev{}, user_prev{};
    GetSystemTimes(&idle_prev, &kernel_prev, &user_prev);

    const DWORD  core_count = read_core_count();
    const DWORD  freq_mhz   = read_cpu_freq_mhz();

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // --- CPU usage ---
        FILETIME idle_cur{}, kernel_cur{}, user_cur{};
        GetSystemTimes(&idle_cur, &kernel_cur, &user_cur);

        ULONGLONG idle   = filetime_to_ull(idle_cur)   - filetime_to_ull(idle_prev);
        ULONGLONG kernel = filetime_to_ull(kernel_cur)  - filetime_to_ull(kernel_prev);
        ULONGLONG user   = filetime_to_ull(user_cur)    - filetime_to_ull(user_prev);
        ULONGLONG total  = kernel + user;
        double cpu_pct   = (total > 0) ? 100.0 * (1.0 - static_cast<double>(idle) / total) : 0.0;

        idle_prev   = idle_cur;
        kernel_prev = kernel_cur;
        user_prev   = user_cur;

        // --- Memory ---
        MEMORYSTATUSEX mem{};
        mem.dwLength = sizeof(mem);
        GlobalMemoryStatusEx(&mem);

        const double mb = 1024.0 * 1024.0;
        const double gb = mb * 1024.0;

        double commit_total = static_cast<double>(mem.ullTotalPageFile) / mb;
        double commit_used  = static_cast<double>(mem.ullTotalPageFile - mem.ullAvailPageFile) / mb;

        // --- Disk (system drive) ---
        ULARGE_INTEGER free_bytes{}, total_bytes{}, total_free_bytes{};
        GetDiskFreeSpaceExW(L"C:\\", &free_bytes, &total_bytes, &total_free_bytes);
        double disk_total = static_cast<double>(total_bytes.QuadPart)     / gb;
        double disk_free  = static_cast<double>(free_bytes.QuadPart)      / gb;
        double disk_used_pct = (disk_total > 0.0)
                               ? 100.0 * (1.0 - disk_free / disk_total)
                               : 0.0;

        // --- Uptime ---
        double uptime_sec = static_cast<double>(GetTickCount64()) / 1000.0;

        // --- Commit to cache ---
        {
            std::lock_guard<std::mutex> lock(g_sensors_mtx);
            g_sensors.cpu_usage_pct   = cpu_pct;
            g_sensors.cpu_core_count  = core_count;
            g_sensors.cpu_freq_mhz    = freq_mhz;
            g_sensors.mem_total_mb    = static_cast<double>(mem.ullTotalPhys) / mb;
            g_sensors.mem_avail_mb    = static_cast<double>(mem.ullAvailPhys) / mb;
            g_sensors.mem_used_pct    = static_cast<double>(mem.dwMemoryLoad);
            g_sensors.commit_total_mb = commit_total;
            g_sensors.commit_used_mb  = commit_used;
            g_sensors.disk_total_gb   = disk_total;
            g_sensors.disk_free_gb    = disk_free;
            g_sensors.disk_used_pct   = disk_used_pct;
            g_sensors.uptime_seconds  = uptime_sec;
        }
    }
}

// ============================================================================
// Background thread: WMI thermal zones  (every 5 s)
// Fixed for MinGW compatibility (No <comdef.h> or _bstr_t used)
// ============================================================================

void temperature_monitor() {
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return;

    CoInitializeSecurity(
        nullptr, -1, nullptr, nullptr,
        RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr, EOAC_NONE, nullptr);

    IWbemLocator* pLoc = nullptr;
    if (FAILED(CoCreateInstance(
            CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
            IID_IWbemLocator, reinterpret_cast<LPVOID*>(&pLoc)))) {
        CoUninitialize();
        return;
    }

    IWbemServices* pSvc = nullptr;

    // Allocate BSTR for MinGW compatibility
    BSTR wmiPath = SysAllocString(L"ROOT\\WMI");
    HRESULT hrConnect = pLoc->ConnectServer(
            wmiPath, nullptr, nullptr, nullptr,
            0, nullptr, nullptr, &pSvc);
    SysFreeString(wmiPath); // Free immediately after use

    if (FAILED(hrConnect)) {
        pLoc->Release();
        CoUninitialize();
        return;
    }

    CoSetProxyBlanket(
        pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr, EOAC_NONE);

    while (true) {
        std::vector<double> temps;

        IEnumWbemClassObject* pEnum = nullptr;

        // Allocate query strings
        BSTR queryLang = SysAllocString(L"WQL");
        BSTR query = SysAllocString(L"SELECT CurrentTemperature FROM MSAcpi_ThermalZoneTemperature");

        HRESULT hr = pSvc->ExecQuery(
            queryLang,
            query,
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr, &pEnum);

        // Free query strings
        SysFreeString(queryLang);
        SysFreeString(query);

        if (SUCCEEDED(hr) && pEnum) {
            IWbemClassObject* pObj = nullptr;
            ULONG returned = 0;

            while (pEnum->Next(WBEM_INFINITE, 1, &pObj, &returned) == S_OK) {
                VARIANT vt{};
                if (SUCCEEDED(pObj->Get(L"CurrentTemperature", 0, &vt, nullptr, nullptr))
                    && vt.vt == VT_I4) {
                    // tenths of Kelvin → Celsius
                    double celsius = (static_cast<double>(vt.lVal) / 10.0) - 273.15;
                    temps.push_back(celsius);
                }
                VariantClear(&vt);
                pObj->Release();
            }
            pEnum->Release();
        }

        {
            std::lock_guard<std::mutex> lock(g_sensors_mtx);
            g_sensors.temperatures_c = std::move(temps);
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    pSvc->Release();
    pLoc->Release();
    CoUninitialize();
}

// ============================================================================
// Build JSON response from cache
// ============================================================================

crow::json::wvalue sensor_snapshot() {
    SensorData snap;
    {
        std::lock_guard<std::mutex> lock(g_sensors_mtx);
        snap = g_sensors;
    }

    crow::json::wvalue out;

    // CPU
    out["cpu"]["usage_percent"] = snap.cpu_usage_pct;
    out["cpu"]["logical_cores"] = static_cast<int>(snap.cpu_core_count);
    out["cpu"]["base_freq_mhz"] = static_cast<int>(snap.cpu_freq_mhz);

    // Memory
    out["memory"]["total_mb"]        = snap.mem_total_mb;
    out["memory"]["available_mb"]    = snap.mem_avail_mb;
    out["memory"]["used_percent"]    = snap.mem_used_pct;
    out["memory"]["commit_total_mb"] = snap.commit_total_mb;
    out["memory"]["commit_used_mb"]  = snap.commit_used_mb;

    // Disk
    out["disk"]["drive"]        = "C:";
    out["disk"]["total_gb"]     = snap.disk_total_gb;
    out["disk"]["free_gb"]      = snap.disk_free_gb;
    out["disk"]["used_percent"] = snap.disk_used_pct;

    // Temperatures
    if (snap.temperatures_c.empty()) {
        out["temperatures"] = crow::json::wvalue(crow::json::type::List);
    } else {
        crow::json::wvalue::list temps;
        for (size_t i = 0; i < snap.temperatures_c.size(); ++i) {
            crow::json::wvalue entry;
            entry["zone"]       = "zone_" + std::to_string(i);
            entry["celsius"]    = snap.temperatures_c[i];
            temps.push_back(std::move(entry));
        }
        out["temperatures"] = std::move(temps);
    }

    // System
    out["system"]["uptime_seconds"] = snap.uptime_seconds;

    return out;
}

// ============================================================================
// OpenAPI document
// ============================================================================

std::string openapi_document() {
    return R"({
  "openapi": "3.0.3",
  "info": {
    "title": "portfolio_cpp API",
    "version": "1.0.0",
    "description": "Crow service exposing detailed system sensor data."
  },
  "servers": [{"url": "http://localhost:18080"}],
  "paths": {
    "/": {
      "get": {
        "summary": "Service metadata",
        "responses": {"200": {"description": "Basic service information"}}
      }
    },
    "/api/sensors": {
      "get": {
        "summary": "Live sensor readings",
        "description": "CPU usage/frequency/cores, RAM, disk, thermal zones, uptime.",
        "responses": {
          "200": {
            "description": "Sensor data",
            "content": {
              "application/json": {
                "schema": {
                  "type": "object",
                  "properties": {
                    "cpu":          {"type": "object"},
                    "memory":       {"type": "object"},
                    "disk":         {"type": "object"},
                    "temperatures": {"type": "array"},
                    "system":       {"type": "object"}
                  }
                }
              }
            }
          }
        }
      }
    },
    "/api/hello/{name}": {
      "get": {
        "summary": "Returns a greeting",
        "parameters": [{
          "name": "name", "in": "path", "required": true,
          "schema": {"type": "string"}
        }],
        "responses": {"200": {"description": "Greeting payload"}}
      }
    },
    "/openapi.json": {
      "get": {
        "summary": "OpenAPI document",
        "responses": {"200": {"description": "The OpenAPI 3.0 spec for this service"}}
      }
    }
  }
})";
}

}  // namespace

// ============================================================================
// main
// ============================================================================

int main() {
    std::thread(system_monitor).detach();
    std::thread(temperature_monitor).detach();

    crow::SimpleApp app;

    // Define routes inside main directly without capturing app variables
    CROW_ROUTE(app, "/")([] {
        crow::json::wvalue r;
        r["app"]       = "portfolio_cpp";
        r["framework"] = "Crow";
        r["docs"]      = "/openapi.json";
        r["sensors"]   = "/api/sensors";
        return r;
    });

    CROW_ROUTE(app, "/api/sensors")([] {
        return sensor_snapshot();
    });

    CROW_ROUTE(app, "/api/hello/<string>")([](const std::string& name) {
        crow::json::wvalue r;
        r["message"]   = "Hello, " + name + "!";
        r["framework"] = "Crow";
        return r;
    });

    CROW_ROUTE(app, "/openapi.json")([] {
        crow::response r(openapi_document());
        r.code = 200;
        r.set_header("Content-Type", "application/json");
        return r;
    });

    const int port = read_port();
    app.port(static_cast<std::uint16_t>(port)).multithreaded().run();
}