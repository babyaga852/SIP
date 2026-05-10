#pragma once
#include "sip/os/os_abstraction.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <deque>
#include <cstdint>

namespace sip::modules {

/// A behavioral fingerprint for a single process.
struct Fingerprint {
    uint32_t pid;
    std::string name;
    // Rolling window of CPU samples
    std::deque<float> cpu_history;   // last N samples
    // Rolling window of RSS samples
    std::deque<uint64_t> mem_history;
    float cpu_mean{0};
    float cpu_stddev{0};
    // Anomaly score: 0 = normal, >1 = suspicious
    float anomaly_score{0};
};

/// Detects process behavioural anomalies by comparing live CPU/memory
/// readings against a rolling baseline fingerprint.
class BehaviorDNA {
public:
    static constexpr int WINDOW_SIZE      = 60;  // samples
    static constexpr float ANOMALY_THRESH = 3.0f; // sigma

    /// Feed a fresh process snapshot. Updates fingerprints and scores.
    void update(const std::vector<sip::os::ProcessInfo>& snapshot);

    /// Return processes whose anomaly_score exceeds ANOMALY_THRESH.
    std::vector<Fingerprint> anomalies() const;

    /// Full fingerprint map (pid → Fingerprint).
    const std::unordered_map<uint32_t, Fingerprint>& fingerprints() const {
        return fps_;
    }

private:
    void update_fingerprint(Fingerprint& fp, const sip::os::ProcessInfo& pi);
    void compute_stats(Fingerprint& fp);

    std::unordered_map<uint32_t, Fingerprint> fps_;
};

} // namespace sip::modules
