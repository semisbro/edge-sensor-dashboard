#include "portfolio/web/app_routes.hpp"

#include "portfolio/web/api_payloads.hpp"
#include "portfolio/web/frontend_assets.hpp"

namespace portfolio::web {

void configure_routes(crow::SimpleApp& app, platform::TelemetryCollector& collector) {
    CROW_ROUTE(app, "/")([](crow::response& response) {
        if (frontend_build_available()) {
            serve_file(response, frontend_dist_dir() / "index.html");
            return;
        }

        response.code = 200;
        response.set_header("Content-Type", "application/json");
        response.write(service_metadata().dump());
        response.end();
    });

    CROW_ROUTE(app, "/api/meta")([] {
        return service_metadata();
    });

    CROW_ROUTE(app, "/api/sensors")([&collector] {
        return sensor_payload(collector.sensor_snapshot());
    });

    CROW_ROUTE(app, "/api/disks")([&collector] {
        return disk_inventory_payload(collector.disk_inventory_snapshot());
    });

    CROW_ROUTE(app, "/api/hello/<string>")([](const std::string& name) {
        crow::json::wvalue payload;
        payload["message"] = "Hello, " + name + "!";
        payload["framework"] = "Crow";
        return payload;
    });

    CROW_ROUTE(app, "/openapi.json")([] {
        crow::response response(openapi_document());
        response.code = 200;
        response.set_header("Content-Type", "application/json");
        return response;
    });

    CROW_ROUTE(app, "/assets/<path>")
    ([](crow::response& response, const std::string& asset_path) {
        if (auto file_path = frontend_file_for("assets/" + asset_path)) {
            serve_file(response, *file_path);
            return;
        }

        response.code = 404;
        response.end();
    });

    CROW_ROUTE(app, "/<path>")
    ([](crow::response& response, const std::string& route_path) {
        if (!frontend_build_available()) {
            response.code = 404;
            response.end();
            return;
        }

        if (auto file_path = frontend_file_for(route_path)) {
            serve_file(response, *file_path);
            return;
        }

        if (looks_like_asset_request(route_path)) {
            response.code = 404;
            response.end();
            return;
        }

        serve_file(response, frontend_dist_dir() / "index.html");
    });
}

}  // namespace portfolio::web
