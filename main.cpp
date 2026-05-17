#include <cstdint>
#include <memory>

#include "crow.h"
#include "portfolio/platform/telemetry_collector.hpp"
#include "portfolio/web/app_routes.hpp"
#include "portfolio/web/frontend_assets.hpp"

int main() {
    std::unique_ptr<portfolio::platform::TelemetryCollector> collector =
        portfolio::platform::make_default_telemetry_collector();
    collector->start();

    crow::SimpleApp app;
    portfolio::web::configure_routes(app, *collector);

    const int port = portfolio::web::read_port();
    app.port(static_cast<std::uint16_t>(port)).multithreaded().run();
}
