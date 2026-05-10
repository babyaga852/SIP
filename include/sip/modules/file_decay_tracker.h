#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <unordered_map>

namespace sip::modules {

struct FileScore {
    std::string path;
    float       usefulness;   // 0.0 (stale/dead) → 1.0 (actively used)
    int64_t     days_since_access;
    int64_t     days_since_modify;
    uint64_t    size_bytes;
};

/// Scores files in watched directories by a "usefulness" heuristic:
///   - Last access time (weighted heavily)
///   - Last modification time
///   - File size (very large or very tiny files penalised slightly)
///   - Extension hints (e.g. .log, .tmp → lower base score)
class FileDecayTracker {
public:
    explicit FileDecayTracker(std::vector<std::string> watch_dirs);

    /// Scan all watched directories and recompute scores.
    void scan();

    /// Return all files sorted by usefulness ascending (stalest first).
    std::vector<FileScore> ranked() const;

    /// Return files with usefulness below threshold.
    std::vector<FileScore> stale(float threshold = 0.2f) const;

private:
    float compute_usefulness(const std::filesystem::path& p,
                             int64_t days_access, int64_t days_modify,
                             uint64_t size) const;

    std::vector<std::string>              dirs_;
    std::unordered_map<std::string, FileScore> scores_;
};

} // namespace sip::modules
