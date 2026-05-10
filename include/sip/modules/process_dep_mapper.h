#pragma once
#include "sip/os/os_abstraction.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace sip::modules {

struct Edge {
    uint32_t from_pid;
    uint32_t to_pid;
    std::string type;  // "parent", "ipc", "socket"
};

struct IpcGraph {
    std::unordered_map<uint32_t, std::string> nodes;  // pid → name
    std::vector<Edge>                          edges;
};

/// Builds an IPC dependency graph from the live process list.
/// Edges are derived from:
///   - Parent/child relationships (/proc/<pid>/status: PPid)
///   - IPC peers from the OS abstraction layer
class ProcessDepMapper {
public:
    /// Rebuild the graph from a fresh process snapshot.
    void update(const std::vector<sip::os::ProcessInfo>& snapshot);

    const IpcGraph& graph() const { return graph_; }

    /// Return processes with no parents and no children (isolated).
    std::vector<uint32_t> isolated() const;

    /// BFS reachable set from a given PID.
    std::unordered_set<uint32_t> reachable_from(uint32_t pid) const;

    /// Export graph as DOT format for Graphviz.
    std::string to_dot() const;

private:
    IpcGraph graph_;
};

} // namespace sip::modules
