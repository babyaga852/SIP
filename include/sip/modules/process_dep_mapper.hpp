#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace sip {
class Engine;

struct ProcessNode {
    std::uint32_t pid{};
    std::string   name;
    std::uint32_t ppid{};
};

struct IPCEdge {
    std::uint32_t src_pid{};
    std::uint32_t dst_pid{};
    std::string   ipc_type;   ///< "pipe", "socket", "shm", "signal"
    std::uint64_t byte_count{};
};

struct DependencyGraph {
    std::vector<ProcessNode> nodes;
    std::vector<IPCEdge>     edges;

    /// Serialise to DOT format for Graphviz.
    std::string to_dot() const;

    /// Serialise to JSON for the TUI / export layer.
    std::string to_json() const;
};

/// Discovers IPC relationships between running processes and builds
/// a live dependency graph.
class ProcessDepMapper {
public:
    explicit ProcessDepMapper(Engine& engine);
    ~ProcessDepMapper();

    void start();
    void stop();

    DependencyGraph snapshot() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace sip
