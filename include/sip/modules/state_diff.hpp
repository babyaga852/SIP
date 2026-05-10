#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <nlohmann/json.hpp>

namespace sip {
class Engine;

struct Snapshot {
    std::string  tag;
    std::int64_t timestamp_ms{};
    nlohmann::json data;
};

struct DiffEntry {
    std::string path;      ///< JSON pointer, e.g. "/processes/1234/cpu"
    std::string op;        ///< "add" | "remove" | "replace"
    nlohmann::json old_value;
    nlohmann::json new_value;
};

/// Periodically snapshots system state and produces structured diffs
/// between consecutive (or named) snapshots.
class StateDiff {
public:
    explicit StateDiff(Engine& engine);
    ~StateDiff();

    void start();
    void stop();

    /// Take an immediate named snapshot.
    void take_snapshot(const std::string& tag);

    /// Diff two named snapshots. Returns JSON Patch (RFC 6902) entries.
    std::vector<DiffEntry> diff(const std::string& tag_a, const std::string& tag_b) const;

    std::vector<Snapshot> list_snapshots() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace sip
