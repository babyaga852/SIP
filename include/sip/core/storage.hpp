#pragma once
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace sip {

/// Key-value + JSON document store backed by SQLite.
class Storage {
public:
    explicit Storage(const std::string& db_path);
    ~Storage();

    // ── KV store ─────────────────────────────────────────────────────────────
    void        set(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key) const;
    void        remove(const std::string& key);

    // ── JSON snapshots ────────────────────────────────────────────────────────
    void                 save_snapshot(const std::string& tag, const nlohmann::json& doc);
    nlohmann::json       load_snapshot(const std::string& tag) const;

    // ── Raw SQL (for modules that need custom tables) ─────────────────────────
    void exec(const std::string& sql);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace sip
