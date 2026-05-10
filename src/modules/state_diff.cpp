#include "modules/state_diff.hpp"
#include "os/os_layer.hpp"
#include <spdlog/spdlog.h>
#include <fstream>

namespace sip {

void StateDiff::init(Engine& eng) {
    engine_ = &eng;
    prev_state_ = capture_state();
    last_snap_ms_ = sip::os::now_ms();
    eng.scheduler().every(std::chrono::seconds(60), [this]{ tick(); }, "StateDiff");
}

void StateDiff::tick() {
    auto cur = capture_state();
    auto diffs = compute_diff(prev_state_, cur);
    for (auto& d : diffs) history_.push_back(d);
    if (!diffs.empty()) {
        engine_->storage().set("state_diff.latest", snapshot());
        spdlog::info("[StateDiff] {} changes detected", diffs.size());
    }
    prev_state_ = cur;
    last_snap_ms_ = sip::os::now_ms();
}

void StateDiff::shutdown() {}

nlohmann::json StateDiff::capture_state() const {
    nlohmann::json s;
    for (auto& p : sip::os::list_processes())
        s["processes"][std::to_string(p.pid)] = {{"name", p.name},
            {"cpu", p.cpu_pct}, {"rss", p.rss_bytes}};
    for (auto& n : sip::os::net_stats())
        s["net"][n.iface] = {{"rx", n.rx_bytes}, {"tx", n.tx_bytes}};
    return s;
}

std::vector<DiffEntry> StateDiff::compute_diff(const nlohmann::json& a,
                                               const nlohmann::json& b) const {
    std::vector<DiffEntry> out;
    for (auto& [k, v] : b.items()) {
        if (!a.contains(k)) out.push_back({k, "added",  nullptr, v});
        else if (a[k] != v) out.push_back({k, "changed", a[k],  v});
    }
    for (auto& [k, v] : a.items())
        if (!b.contains(k)) out.push_back({k, "removed", v, nullptr});
    return out;
}

std::vector<DiffEntry> StateDiff::diff_since(int64_t epoch_ms) const {
    return history_; // TODO: filter by timestamp
}

nlohmann::json StateDiff::snapshot() const {
    nlohmann::json out = nlohmann::json::array();
    for (auto& d : history_)
        out.push_back({{"key",d.key},{"op",d.op},
                       {"before",d.before},{"after",d.after}});
    return out;
}

void StateDiff::export_csv(const std::string& path) const {
    std::ofstream f(path);
    f << "key,op,before,after\n";
    for (auto& d : history_)
        f << d.key << "," << d.op << ","
          << d.before.dump() << "," << d.after.dump() << "\n";
}

} // namespace sip
