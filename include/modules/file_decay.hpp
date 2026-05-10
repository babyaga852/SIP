#pragma once
#include "core/engine.hpp"
#include <filesystem>
#include <unordered_map>
#include <string>
#include <nlohmann/json.hpp>
namespace fs = std::filesystem;

namespace sip {

struct FileRecord {
    fs::path    path;
    double      usefulness_score; // 0.0 (dead) – 1.0 (hot)
    int64_t     last_access_ms;
    int64_t     last_modify_ms;
    uint64_t    access_count;
};

class FileDecayTracker : public IModule {
public:
    std::string name() const override { return "FileDecayTracker"; }
    void init(Engine& eng) override;
    void tick() override;
    void shutdown() override;

    void            watch(const fs::path& root);
    nlohmann::json  report() const;
    std::vector<FileRecord> dead_files(double threshold = 0.1) const;

private:
    Engine*    engine_{nullptr};
    fs::path   root_;
    std::unordered_map<std::string, FileRecord> records_;

    void        scan_directory();
    static double decay_score(int64_t last_access_ms, uint64_t count);
};

} // namespace sip
