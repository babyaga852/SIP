#include "sip/modules/file_decay_tracker.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <sys/stat.h>

namespace sip::modules {

static const std::unordered_set<std::string> LOW_VALUE_EXTS = {
    ".log", ".tmp", ".bak", ".swp", ".DS_Store", ".pyc"
};

FileDecayTracker::FileDecayTracker(std::vector<std::string> watch_dirs)
    : dirs_(std::move(watch_dirs)) {}

void FileDecayTracker::scan() {
    namespace fs = std::filesystem;
    int64_t now = (int64_t)std::time(nullptr);

    for (const auto& dir : dirs_) {
        if (!fs::exists(dir)) {
            spdlog::warn("[FileDecay] directory '{}' does not exist", dir);
            continue;
        }
        for (auto& entry : fs::recursive_directory_iterator(
                dir, fs::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;
            const auto& path = entry.path();

            struct stat s{};
            if (::stat(path.c_str(), &s) != 0) continue;

            int64_t days_access  = (now - (int64_t)s.st_atime)  / 86400;
            int64_t days_modify  = (now - (int64_t)s.st_mtime)  / 86400;
            uint64_t size        = (uint64_t)s.st_size;

            float u = compute_usefulness(path, days_access, days_modify, size);
            scores_[path.string()] = FileScore{
                path.string(), u, days_access, days_modify, size
            };
        }
    }
    spdlog::info("[FileDecay] scanned {} files", scores_.size());
}

float FileDecayTracker::compute_usefulness(const std::filesystem::path& p,
                                           int64_t days_access, int64_t days_modify,
                                           uint64_t size) const {
    // Access score: decays exponentially; half-life ~30 days
    float access_score = std::exp(-0.023f * (float)days_access);
    // Modify score: slower decay; half-life ~90 days
    float modify_score = std::exp(-0.008f * (float)days_modify);
    // Combine with weights
    float score = 0.7f * access_score + 0.3f * modify_score;

    // Extension penalty
    std::string ext = p.extension().string();
    if (LOW_VALUE_EXTS.count(ext)) score *= 0.5f;

    // Size penalty for very tiny files (<64 bytes) or huge logs (>500 MB)
    if (size < 64)               score *= 0.8f;
    if (size > 500ULL * 1024 * 1024) score *= 0.7f;

    return std::clamp(score, 0.0f, 1.0f);
}

std::vector<FileScore> FileDecayTracker::ranked() const {
    std::vector<FileScore> out;
    out.reserve(scores_.size());
    for (const auto& [_, fs] : scores_) out.push_back(fs);
    std::sort(out.begin(), out.end(), [](const FileScore& a, const FileScore& b){
        return a.usefulness < b.usefulness;
    });
    return out;
}

std::vector<FileScore> FileDecayTracker::stale(float threshold) const {
    std::vector<FileScore> out;
    for (const auto& [_, fs] : scores_)
        if (fs.usefulness < threshold) out.push_back(fs);
    return out;
}

} // namespace sip::modules
