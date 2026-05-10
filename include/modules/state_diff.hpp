#pragma once
#include "core/engine.hpp"
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace sip {

struct DiffEntry {
    std::string key;
    std::string op;        // added | removed | changed
    nlohmann::json before;
    nlohmann::json after;
};

class StateDiff : public IModule {
public:
    std::string name() const override { return "StateDiff"; }
    void init(Engine& eng) override;
    void tick() override;
    void shutdown() override;

    std::vector<DiffEntry> diff_since(int64_t epoch_ms) const;
    nlohmann::json         snapshot()                    const;
    void                   export_csv(const std::string& path) const;

private:
    Engine*                         engine_{nullptr};
    nlohmann::json                  prev_state_;
    std::vector<DiffEntry>          history_;
    int64_t                         last_snap_ms_{0};

    nlohmann::json  capture_state()         const;
    std::vector<DiffEntry> compute_diff(const nlohmann::json& a,
                                        const nlohmann::json& b) const;
};

} // namespace sip
