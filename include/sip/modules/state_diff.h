#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <chrono>

namespace sip::modules {

using json = nlohmann::json;

struct DiffEntry {
    std::string key;
    enum class Kind { Added, Removed, Changed } kind;
    json        old_value;
    json        new_value;
};

/// Takes periodic snapshots of the full system state (as JSON) and computes
/// a structural diff between consecutive snapshots.
class StateDiff {
public:
    /// Capture a new snapshot labelled with current time.
    void capture(const json& state);

    /// Compute diff between the last two snapshots.
    /// Returns empty if fewer than two snapshots have been taken.
    std::vector<DiffEntry> diff_last() const;

    /// Diff between any two snapshots by index (0 = oldest).
    std::vector<DiffEntry> diff(size_t i, size_t j) const;

    /// Number of stored snapshots.
    size_t size() const { return snapshots_.size(); }

    /// Export all snapshots as a JSON array.
    json export_all() const;

private:
    struct Snapshot {
        std::chrono::system_clock::time_point ts;
        json data;
    };

    static std::vector<DiffEntry> compute_diff(const json& a, const json& b,
                                               const std::string& prefix = "");

    std::vector<Snapshot> snapshots_;
};

} // namespace sip::modules
