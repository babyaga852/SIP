#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace sip {
class Engine;

struct ProcessFingerprint {
    std::string name;
    double      cpu_mean{};        ///< Rolling mean CPU %
    double      cpu_stddev{};      ///< Rolling stddev
    double      mem_mean{};        ///< Rolling mean RSS bytes
    std::size_t syscall_count{};   ///< Distinct syscalls observed
    nlohmann::json raw;            ///< Full sample for export
};

struct AnomalyEvent {
    std::string   process_name;
    std::string   reason;
    double        deviation_sigma{};  ///< How many σ from baseline
    std::int64_t  timestamp_ms{};
};

/// Builds per-process behavioral fingerprints and raises anomaly events
/// when a process deviates significantly from its baseline.
class BehaviorDNA {
public:
    explicit BehaviorDNA(Engine& engine);
    ~BehaviorDNA();

    void start();
    void stop();

    std::vector<AnomalyEvent>     recent_anomalies(std::size_t limit = 50) const;
    std::vector<ProcessFingerprint> fingerprints() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace sip
