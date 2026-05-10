#pragma once
#include <string>
#include <nlohmann/json.hpp>
struct sqlite3;

namespace sip {
class Storage {
public:
    explicit Storage(std::string db_path = ":memory:");
    ~Storage();
    void           set(const std::string& key, const nlohmann::json& value);
    nlohmann::json get(const std::string& key) const;
    bool           has(const std::string& key) const;
    void           del(const std::string& key);
    void           snapshot(const std::string& path) const;
    void           append_metric(const std::string& series, double value,
                                 const std::string& meta = "");
private:
    sqlite3* db_{nullptr};
    void exec(const std::string& sql);
};
} // namespace sip
