#if !defined(_WIN32) && !defined(__APPLE__) && !defined(__linux__)

#include <memory>
#include <vector>

#include "portfolio/platform/telemetry_collector.hpp"

namespace portfolio::platform {
namespace {

class NullTelemetryCollector final : public TelemetryCollector {
public:
    void start() override {}

    SensorData sensor_snapshot() const override {
        return {};
    }

    std::vector<DiskInfo> disk_inventory_snapshot() const override {
        return {};
    }
};

}  // namespace

std::unique_ptr<TelemetryCollector> make_default_telemetry_collector() {
    return std::make_unique<NullTelemetryCollector>();
}

}  // namespace portfolio::platform

#endif  // !_WIN32 && !__APPLE__ && !__linux__
