#include "modules/behavior_dna.hpp"
#include "os/os_layer.hpp"
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>
#include <chrono>

namespace sip {

void BehaviorDNA::init(Engine& eng) {
    engine_ = &eng;
    eng.scheduler().every(std::chrono::seconds(5), [this]{ tick(); }, "BehaviorDNA");
}

void BehaviorDNA::tick() { sample(); detect(); }
void BehaviorDNA::shutdown() {}

void BehaviorDNA::sample() {
    auto procs = sip::os::list_processes();
    for (auto& p : procs) {
        std::string key = std::to_string(p.pid);
        Fingerprint fp = {p.cpu_pct, p.cpu_pct * 1.2,
                          static_cast<double>(p.rss_bytes) / (1024*1024),
                          static_cast<double>(p.open_fds), 0.0};
        current_[key] = fp;
        if (!baselines_.count(key)) baselines_[key] = fp;
        else {
            // Exponential moving average update
            auto& base = baselines_[key];
            for (size_t i = 0; i < fp.size(); ++i)
                base[i] = 0.9 * base[i] + 0.1 * fp[i];
        }
    }
}

double BehaviorDNA::mahalanobis(const Fingerprint& a, const Fingerprint& b) const {
    double sum = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        double d = a[i] - b[i];
        sum += d * d;
    }
    return std::sqrt(sum);
}

void BehaviorDNA::detect() {
    anomalies_.clear();
    auto now_ms = sip::os::now_ms();
    for (auto& [pid, cur] : current_) {
        if (!baselines_.count(pid)) continue;
        double score = mahalanobis(cur, baselines_.at(pid));
        if (score > 3.0) {
            anomalies_.push_back({pid, "unknown", score,
                "Deviation score " + std::to_string(score), now_ms});
        }
    }
    if (!anomalies_.empty()) {
        engine_->storage().set("behavior_dna.anomalies", report());
        spdlog::warn("[BehaviorDNA] {} anomalies detected", anomalies_.size());
    }
}

std::vector<Anomaly> BehaviorDNA::anomalies() const { return anomalies_; }

nlohmann::json BehaviorDNA::report() const {
    nlohmann::json out = nlohmann::json::array();
    for (auto& a : anomalies_)
        out.push_back({{"pid", a.pid}, {"score", a.deviation_score},
                       {"desc", a.description}, {"ts", a.timestamp_ms}});
    return out;
}

} // namespace sip
