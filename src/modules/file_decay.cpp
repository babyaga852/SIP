#include "modules/file_decay.hpp"
#include "os/os_layer.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include <cmath>
#include <fstream>

namespace sip {

void FileDecayTracker::init(Engine& eng) {
    engine_ = &eng;
    eng.scheduler().every(std::chrono::seconds(30), [this]{ tick(); }, "FileDecay");
}

void FileDecayTracker::watch(const fs::path& root) {
    root_ = root;
    engine_->event_loop().watch_path(root.string(),
        [this](const std::string& path, int) {
            auto& rec = records_[path];
            rec.path = path;
            rec.access_count++;
            rec.last_access_ms = sip::os::now_ms();
        });
    scan_directory();
}

void FileDecayTracker::tick() { scan_directory(); }
void FileDecayTracker::shutdown() {}

void FileDecayTracker::scan_directory() {
    if (root_.empty()) return;
    auto now = sip::os::now_ms();
    std::error_code ec;
    for (auto& entry : fs::recursive_directory_iterator(root_, ec)) {
        if (!entry.is_regular_file()) continue;
        auto key = entry.path().string();
        auto& rec = records_[key];
        rec.path = entry.path();
        auto lwt = fs::last_write_time(entry.path(), ec);
        if (!ec) {
            auto lwt_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                lwt.time_since_epoch()).count();
            rec.last_modify_ms = lwt_ms;
            if (!rec.last_access_ms) rec.last_access_ms = lwt_ms;
        }
        rec.usefulness_score = decay_score(rec.last_access_ms, rec.access_count);
    }
    engine_->storage().set("file_decay.records", report());
}

double FileDecayTracker::decay_score(int64_t last_access_ms, uint64_t count) {
    auto now = sip::os::now_ms();
    double age_days = static_cast<double>(now - last_access_ms) / 86400000.0;
    double recency  = std::exp(-0.05 * age_days);
    double freq     = std::min(1.0, std::log1p(count) / 5.0);
    return 0.6 * recency + 0.4 * freq;
}

nlohmann::json FileDecayTracker::report() const {
    nlohmann::json out = nlohmann::json::array();
    for (auto& [key, rec] : records_)
        out.push_back({{"path", rec.path.string()},
                       {"score", rec.usefulness_score},
                       {"accesses", rec.access_count}});
    return out;
}

std::vector<FileRecord> FileDecayTracker::dead_files(double threshold) const {
    std::vector<FileRecord> out;
    for (auto& [k, r] : records_)
        if (r.usefulness_score < threshold) out.push_back(r);
    return out;
}

} // namespace sip
