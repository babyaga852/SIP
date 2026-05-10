#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>
#include <mutex>
#include <optional>

namespace sip::core {

using json = nlohmann::json;

/// Persistent JSON-backed key-value store.
/// All snapshots, module state, and diffs are written here.
class Storage {
public:
    explicit Storage(std::filesystem::path db_path);

    /// Set a value (auto-serialises to JSON).
    template<typename T>
    void set(const std::string& key, const T& value) {
        std::lock_guard lock(mu_);
        data_[key] = value;
        flush_unlocked();
    }

    /// Get a value, returns nullopt if key absent.
    template<typename T>
    std::optional<T> get(const std::string& key) const {
        std::lock_guard lock(mu_);
        if (!data_.contains(key)) return std::nullopt;
        return data_[key].get<T>();
    }

    /// Return the raw json at a key (for diffs / export).
    std::optional<json> get_raw(const std::string& key) const;

    /// Delete a key.
    void remove(const std::string& key);

    /// Dump full store to json (for snapshots).
    json snapshot() const;

private:
    void load();
    void flush_unlocked();

    std::filesystem::path  path_;
    json                   data_;
    mutable std::mutex     mu_;
};

} // namespace sip::core
