#include "sip/modules/state_diff.h"
#include <spdlog/spdlog.h>

namespace sip::modules {

void StateDiff::capture(const json& state) {
    snapshots_.push_back({std::chrono::system_clock::now(), state});
    spdlog::debug("[StateDiff] snapshot #{}", snapshots_.size());
}

std::vector<DiffEntry> StateDiff::diff_last() const {
    if (snapshots_.size() < 2) return {};
    return diff(snapshots_.size() - 2, snapshots_.size() - 1);
}

std::vector<DiffEntry> StateDiff::diff(size_t i, size_t j) const {
    if (i >= snapshots_.size() || j >= snapshots_.size()) return {};
    return compute_diff(snapshots_[i].data, snapshots_[j].data);
}

json StateDiff::export_all() const {
    json arr = json::array();
    for (const auto& s : snapshots_) {
        auto t = std::chrono::system_clock::to_time_t(s.ts);
        arr.push_back({{"ts", (int64_t)t}, {"data", s.data}});
    }
    return arr;
}

// ── Recursive flat diff ────────────────────────────────────────────────────────
std::vector<DiffEntry> StateDiff::compute_diff(const json& a, const json& b,
                                               const std::string& prefix) {
    std::vector<DiffEntry> out;

    if (a.is_object() && b.is_object()) {
        // Keys in a but not b → Removed
        for (const auto& [k, v] : a.items()) {
            std::string full = prefix.empty() ? k : prefix + "." + k;
            if (!b.contains(k))
                out.push_back({full, DiffEntry::Kind::Removed, v, {}});
            else {
                auto sub = compute_diff(v, b[k], full);
                out.insert(out.end(), sub.begin(), sub.end());
            }
        }
        // Keys in b but not a → Added
        for (const auto& [k, v] : b.items()) {
            std::string full = prefix.empty() ? k : prefix + "." + k;
            if (!a.contains(k))
                out.push_back({full, DiffEntry::Kind::Added, {}, v});
        }
    } else if (a != b) {
        out.push_back({prefix, DiffEntry::Kind::Changed, a, b});
    }
    return out;
}

} // namespace sip::modules
