#include <cstdint>
#include <cstdio>
#include <memory>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#include "crow.h"
#include "portfolio/platform/telemetry_collector.hpp"
#include "portfolio/web/app_routes.hpp"
#include "portfolio/web/frontend_assets.hpp"

// ─── Entry point ─────────────────────────────────────────────────────────────

int main() {
#ifdef _WIN32
    // Log elevation status — WMI thermal zones need admin, everything else works without it.
    // To run elevated: right-click the .exe → "Run as administrator".
    BOOL elevated = FALSE;
    HANDLE token  = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION elev{};
        DWORD size = sizeof(elev);
        if (GetTokenInformation(token, TokenElevation, &elev, size, &size))
            elevated = elev.TokenIsElevated;
        CloseHandle(token);
    }
    if (elevated)
        std::puts("[SYS-LOG] [INFO] Running as Administrator — all sensors active");
    else
        std::puts("[SYS-LOG] [WARN] Not elevated — thermal zone data unavailable");
#endif

    std::unique_ptr<portfolio::platform::TelemetryCollector> collector =
        portfolio::platform::make_default_telemetry_collector();
    collector->start();

    crow::SimpleApp app;
    portfolio::web::configure_routes(app, *collector);

    const int port = portfolio::web::read_port();
    app.port(static_cast<std::uint16_t>(port)).multithreaded().run();
}
