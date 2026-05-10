#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace sip::exports {

/// Write a JSON snapshot to disk (pretty-printed).
void write_snapshot(const std::string& path, const nlohmann::json& doc);

/// Load a previously written snapshot.
nlohmann::json read_snapshot(const std::string& path);

} // namespace sip::exports
