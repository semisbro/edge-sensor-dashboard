#pragma once

#include <memory>
#include <vector>

#include "portfolio/telemetry_models.hpp"

namespace portfolio::platform {

class TelemetryCollector {
public:
    virtual ~TelemetryCollector() = default;

    virtual void start() = 0;
    virtual SensorData              sensor_snapshot()          const = 0;
    virtual std::vector<DiskInfo>   disk_inventory_snapshot()  const = 0;
};

std::unique_ptr<TelemetryCollector> make_default_telemetry_collector();

}  // namespace portfolio::platform
