#include "portfolio/web/api_payloads.hpp"

#include <utility>

namespace portfolio::web {

crow::json::wvalue sensor_payload(const SensorData& s) {
    crow::json::wvalue p;

    // ── CPU (dynamic + static) ────────────────────────────────────────────────
    p["cpu"]["usage_percent"]  = s.cpu_usage_pct;
    p["cpu"]["logical_cores"]  = static_cast<int>(s.cpu_info.logical_cores);
    p["cpu"]["physical_cores"] = static_cast<int>(s.cpu_info.physical_cores);
    p["cpu"]["base_freq_mhz"]  = static_cast<int>(s.cpu_info.base_freq_mhz);
    p["cpu"]["max_freq_mhz"]   = static_cast<int>(s.cpu_info.max_freq_mhz);
    p["cpu"]["vendor"]         = s.cpu_info.vendor;
    p["cpu"]["brand"]          = s.cpu_info.brand;
    p["cpu"]["architecture"]   = s.cpu_info.architecture;
    p["cpu"]["l2_cache_kb"]    = static_cast<int>(s.cpu_info.l2_cache_kb);
    p["cpu"]["l3_cache_kb"]    = static_cast<int>(s.cpu_info.l3_cache_kb);

    // ── Memory ────────────────────────────────────────────────────────────────
    p["memory"]["total_mb"]        = s.mem_total_mb;
    p["memory"]["available_mb"]    = s.mem_avail_mb;
    p["memory"]["used_percent"]    = s.mem_used_pct;
    p["memory"]["commit_total_mb"] = s.commit_total_mb;
    p["memory"]["commit_used_mb"]  = s.commit_used_mb;

    // ── Primary disk ──────────────────────────────────────────────────────────
    p["disk"]["drive"]        = s.disk_mount_point;
    p["disk"]["total_gb"]     = s.disk_total_gb;
    p["disk"]["free_gb"]      = s.disk_free_gb;
    p["disk"]["used_percent"] = s.disk_used_pct;

    // ── Temperatures ──────────────────────────────────────────────────────────
    crow::json::wvalue::list temps;
    for (const auto& t : s.temperatures) {
        crow::json::wvalue entry;
        entry["zone"]    = t.zone;
        entry["celsius"] = t.celsius;
        temps.push_back(std::move(entry));
    }
    p["temperatures"] = std::move(temps);

    // ── System ────────────────────────────────────────────────────────────────
    p["system"]["uptime_seconds"] = s.uptime_seconds;

    // ── OS ────────────────────────────────────────────────────────────────────
    p["os"]["name"]            = s.os_info.name;
    p["os"]["display_version"] = s.os_info.display_version;
    p["os"]["build"]           = s.os_info.build;
    p["os"]["hostname"]        = s.os_info.hostname;
    p["os"]["architecture"]    = s.os_info.architecture;
    p["os"]["is_elevated"]     = s.os_info.is_elevated;

    return p;
}

crow::json::wvalue disk_inventory_payload(const std::vector<DiskInfo>& disks) {
    crow::json::wvalue p;
    crow::json::wvalue::list entries;

    for (const auto& d : disks) {
        crow::json::wvalue entry;
        entry["mount_point"]  = d.mount_point;
        entry["drive_type"]   = d.drive_type;
        entry["volume_label"] = d.volume_label;
        entry["filesystem"]   = d.filesystem;
        entry["ready"]        = d.ready;
        entry["total_gb"]     = d.total_gb;
        entry["free_gb"]      = d.free_gb;
        entry["used_gb"]      = d.used_gb;
        entry["used_percent"] = d.used_pct;
        entries.push_back(std::move(entry));
    }

    p["count"] = static_cast<int>(entries.size());
    p["disks"] = std::move(entries);
    return p;
}

crow::json::wvalue service_metadata() {
    crow::json::wvalue p;
    p["app"]       = "portfolio_cpp";
    p["framework"] = "Crow";
    p["docs"]      = "/openapi.json";
    p["meta"]      = "/api/meta";
    p["sensors"]   = "/api/sensors";
    p["disks"]     = "/api/disks";
    return p;
}

std::string openapi_document() {
    return R"({
  "openapi": "3.0.3",
  "info": {
    "title": "portfolio_cpp API",
    "version": "1.0.0",
    "description": "Crow service exposing live telemetry snapshots."
  },
  "servers": [{"url": "http://localhost:18080"}],
  "paths": {
    "/": {
      "get": {
        "summary": "Frontend entrypoint or service metadata",
        "responses": {"200": {"description": "Frontend app or metadata payload"}}
      }
    },
    "/api/meta": {
      "get": {
        "summary": "Service metadata",
        "responses": {"200": {"description": "Basic service information"}}
      }
    },
    "/api/sensors": {
      "get": {
        "summary": "Live sensor readings",
        "description": "CPU (usage + static info), memory, primary disk, temperatures, uptime, OS info.",
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
                    "system":       {"type": "object"},
                    "os":           {"type": "object"}
                  }
                }
              }
            }
          }
        }
      }
    },
    "/api/disks": {
      "get": {
        "summary": "Logical disk inventory",
        "description": "Lists all visible disks with capacity and filesystem info.",
        "responses": {
          "200": {
            "description": "Disk inventory",
            "content": {
              "application/json": {
                "schema": {
                  "type": "object",
                  "properties": {
                    "count": {"type": "integer"},
                    "disks": {"type": "array"}
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
        "parameters": [{"name": "name", "in": "path", "required": true, "schema": {"type": "string"}}],
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

}  // namespace portfolio::web
