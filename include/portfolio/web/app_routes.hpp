#pragma once

#include "crow.h"
#include "portfolio/platform/telemetry_collector.hpp"

namespace portfolio::web {

void configure_routes(crow::SimpleApp& app, platform::TelemetryCollector& collector);

}  // namespace portfolio::web
