#pragma once
#include "core/engine.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace sip {

struct ProcessNode {
    uint32_t    pid;
    uint32_t    ppid;
    std::string name;
    std::string cmdline;
};

struct IPCEdge {
    uint32_t    src_pid;
    uint32_t    dst_pid;
    std::string type;   // pipe | socket | shm | mmap
    uint64_t    bytes;
};

class ProcessMapper : public IModule {
public:
    std::string name() const override { return "ProcessMapper"; }
    void init(Engine& eng) override;
    void tick() override;
    void shutdown() override;

    nlohmann::json graph() const;   // DOT + JSON
    std::string    dot()   const;   // Graphviz DOT

private:
    Engine*                                  engine_{nullptr};
    std::unordered_map<uint32_t,ProcessNode> nodes_;
    std::vector<IPCEdge>                     edges_;

    void refresh_processes();
    void refresh_ipc();
};

} // namespace sip
