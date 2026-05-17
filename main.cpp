#include <cstdint>
#include <cstdio>
#include <memory>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <shellapi.h>
#endif

#include "crow.h"
#include "portfolio/platform/telemetry_collector.hpp"
#include "portfolio/web/app_routes.hpp"
#include "portfolio/web/frontend_assets.hpp"

// ─── Windows elevation helpers ───────────────────────────────────────────────
#ifdef _WIN32

static bool is_process_elevated() {
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

// Attempts to relaunch the current executable with Administrator privileges.
// Returns true  → caller should continue running (already elevated, or user declined).
// Returns false → an elevated child process was launched; caller should exit.
static bool request_elevation_or_continue() {
    if (is_process_elevated()) {
        std::puts("[SYS-LOG] [INFO] Running as Administrator — all sensors active");
        return true;
    }

    std::puts("[SYS-LOG] [INFO] Not elevated — requesting Administrator privileges");
    std::puts("[SYS-LOG] [INFO] Administrator access enables WMI thermal zone sensors");

    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);

    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask  = SEE_MASK_NOASYNC | SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = exe;
    sei.nShow  = SW_SHOWNORMAL;

    if (ShellExecuteExW(&sei)) {
        std::puts("[SYS-LOG] [INFO] Elevation granted — elevated instance started, exiting");
        if (sei.hProcess) CloseHandle(sei.hProcess);
        return false;
    }

    // User clicked "No" on the UAC prompt — run with limited sensors
    std::puts("[SYS-LOG] [WARN] Elevation declined — starting in limited mode");
    std::puts("[SYS-LOG] [WARN] Temperature fields will be empty (require Administrator)");
    return true;
}

#endif  // _WIN32

// ─── Entry point ─────────────────────────────────────────────────────────────

int main() {
#ifdef _WIN32
    if (!request_elevation_or_continue()) {
        return 0;  // Elevated child is now running; exit the non-elevated parent
    }
#endif

    std::unique_ptr<portfolio::platform::TelemetryCollector> collector =
        portfolio::platform::make_default_telemetry_collector();
    collector->start();

    crow::SimpleApp app;
    portfolio::web::configure_routes(app, *collector);

    const int port = portfolio::web::read_port();
    app.port(static_cast<std::uint16_t>(port)).multithreaded().run();
}
