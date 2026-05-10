#pragma once
#include <string>
#include <vector>
#include <chrono>

namespace sip {
class Engine;

struct FileRecord {
    std::string path;
    double      usefulness_score{};  ///< 0.0 (dead) … 1.0 (very active)
    std::chrono::system_clock::time_point last_access;
    std::chrono::system_clock::time_point last_modify;
    std::uintmax_t size_bytes{};
};

/// Monitors a set of directories and assigns a "usefulness score" to every
/// file based on access/modify recency, open frequency, and size.
class FileDecayTracker {
public:
    explicit FileDecayTracker(Engine& engine);
    ~FileDecayTracker();

    void add_watch_dir(const std::string& dir);
    void start();
    void stop();

    /// Returns files sorted by ascending usefulness (most decayed first).
    std::vector<FileRecord> decayed_files(double threshold = 0.2) const;
    std::vector<FileRecord> all_files() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace sip
