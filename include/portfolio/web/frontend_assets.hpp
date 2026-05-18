#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "crow.h"

namespace portfolio::web {

int                             read_port();
std::filesystem::path           frontend_dist_dir();
bool                            frontend_build_available();
std::optional<std::filesystem::path> frontend_file_for(const std::string& raw_path);
void serve_file(crow::response& response, const std::filesystem::path& file_path);
bool looks_like_asset_request(const std::string& route_path);

}  // namespace portfolio::web
