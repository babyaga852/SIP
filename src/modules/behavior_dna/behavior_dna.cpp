#include "sip/modules/behavior_dna.h"
#include <spdlog/spdlog.h>
#include <cmath>
#include <numeric>
#include <algorithm>

namespace sip::modules {

void BehaviorDNA::update(const std::vector<sip::os::ProcessInfo>& snapshot) {
    for (const auto& pi : snapshot) {
        auto& fp = fps_[pi.pid];
        fp.pid  = pi.pid;
        fp.name = pi.name;
        update_fingerprint(fp, pi);
        compute_stats(fp);
    }
}

void BehaviorDNA::update_fingerprint(Fingerprint& fp, const sip::os::ProcessInfo& pi) {
    fp.cpu_history.push_back(pi.cpu_percent);
    fp.mem_history.push_back(pi.mem_rss_bytes);
    if ((int)fp.cpu_history.size() > WINDOW_SIZE) fp.cpu_history.pop_front();
    if ((int)fp.mem_history.size() > WINDOW_SIZE) fp.mem_history.pop_front();
}

void BehaviorDNA::compute_stats(Fingerprint& fp) {
    if (fp.cpu_history.size() < 5) {
        fp.anomaly_score = 0.0f;
        return;
    }
    // Mean
    float sum = std::accumulate(fp.cpu_history.begin(), fp.cpu_history.end(), 0.0f);
    fp.cpu_mean = sum / (float)fp.cpu_history.size();

    // Stddev
    float sq_sum = 0;
    for (float v : fp.cpu_history) sq_sum += (v - fp.cpu_mean) * (v - fp.cpu_mean);
    fp.cpu_stddev = std::sqrt(sq_sum / (float)fp.cpu_history.size());

    // Z-score of latest sample
    float latest = fp.cpu_history.back();
    fp.anomaly_score = (fp.cpu_stddev > 0.01f)
        ? std::abs(latest - fp.cpu_mean) / fp.cpu_stddev
        : 0.0f;

    if (fp.anomaly_score > ANOMALY_THRESH)
        spdlog::warn("[BehaviorDNA] anomaly pid={} name='{}' score={:.2f}",
                     fp.pid, fp.name, fp.anomaly_score);
}

std::vector<Fingerprint> BehaviorDNA::anomalies() const {
    std::vector<Fingerprint> out;
    for (const auto& [pid, fp] : fps_)
        if (fp.anomaly_score > ANOMALY_THRESH)
            out.push_back(fp);
    std::sort(out.begin(), out.end(), [](const Fingerprint& a, const Fingerprint& b){
        return a.anomaly_score > b.anomaly_score;
    });
    return out;
}

} // namespace sip::modules
