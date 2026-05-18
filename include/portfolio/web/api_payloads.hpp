#pragma once

#include <string>
#include <vector>

#include "crow.h"
#include "portfolio/telemetry_models.hpp"

namespace portfolio::web {

// Pull platform types into web namespace so callers need not qualify them.
using platform::SensorData;
using platform::DiskInfo;

crow::json::wvalue sensor_payload(const SensorData& s);
crow::json::wvalue disk_inventory_payload(const std::vector<DiskInfo>& disks);
crow::json::wvalue service_metadata();
std::string        openapi_document();

}  // namespace portfolio::web
