#pragma once
#include "core/engine.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>

namespace sip {

// Fingerprint = { cpu_p50, cpu_p99, rss_mb, open_fds, net_bytes_ps }
using Fingerprint = std::array<double, 5>;

struct Anomaly {
    std::string pid;
    std::string process_name;
    double      deviation_score; // z-score vs baseline
    std::string description;
    int64_t     timestamp_ms;
};

class BehaviorDNA : public IModule {
public:
    std::string name() const override { return "BehaviorDNA"; }
    void init(Engine& eng) override;
    void tick() override;
    void shutdown() override;

    std::vector<Anomaly> anomalies() const;
    nlohmann::json       report()    const;

private:
    Engine*                                    engine_{nullptr};
    std::unordered_map<std::string,Fingerprint> baselines_;
    std::unordered_map<std::string,Fingerprint> current_;
    std::vector<Anomaly>                        anomalies_;

    void sample();
    void detect();
    double mahalanobis(const Fingerprint& a, const Fingerprint& b) const;
};

} // namespace sip
