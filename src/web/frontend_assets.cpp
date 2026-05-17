#include "portfolio/web/frontend_assets.hpp"

#include <cstdlib>
#include <system_error>

namespace portfolio::web {
namespace fs = std::filesystem;

int read_port() {
    if (const char* raw_port = std::getenv("PORT")) {
        return std::stoi(raw_port);
    }
    return 18080;
}

fs::path frontend_dist_dir() {
    return fs::path("frontend") / "dist";
}

bool frontend_build_available() {
    std::error_code error;
    return fs::is_regular_file(frontend_dist_dir() / "index.html", error);
}

std::optional<fs::path> frontend_file_for(const std::string& raw_path) {
    fs::path relative_path(raw_path);
    if (relative_path.empty() || relative_path.is_absolute()) {
        return std::nullopt;
    }

    relative_path = relative_path.lexically_normal();
    const std::string normalized = relative_path.generic_string();
    if (normalized.empty() || normalized == "." || normalized == ".." ||
        normalized.rfind("../", 0) == 0) {
        return std::nullopt;
    }

    std::error_code error;
    const fs::path candidate = (frontend_dist_dir() / relative_path).lexically_normal();
    if (fs::is_regular_file(candidate, error)) {
        return candidate;
    }

    return std::nullopt;
}

void serve_file(crow::response& response, const fs::path& file_path) {
    response.set_static_file_info(file_path.string());
    response.end();
}

bool looks_like_asset_request(const std::string& route_path) {
    return fs::path(route_path).has_extension();
}

}  // namespace portfolio::web
