#include "sip/modules/process_dep_mapper.h"
#include "sip/os/os_abstraction.h"
#include <spdlog/spdlog.h>
#include <queue>
#include <sstream>

namespace sip::modules {

void ProcessDepMapper::update(const std::vector<sip::os::ProcessInfo>& snapshot) {
    graph_.nodes.clear();
    graph_.edges.clear();

    for (const auto& pi : snapshot) {
        graph_.nodes[pi.pid] = pi.name;

        // Parent→child edges from ProcessInfo.children
        for (uint32_t child : pi.children)
            graph_.edges.push_back(Edge{pi.pid, child, "parent"});

        // IPC edges
        for (uint32_t peer : sip::os::ipc_peers(pi.pid))
            graph_.edges.push_back(Edge{pi.pid, peer, "ipc"});
    }

    spdlog::debug("[ProcessMapper] {} nodes, {} edges",
                  graph_.nodes.size(), graph_.edges.size());
}

std::vector<uint32_t> ProcessDepMapper::isolated() const {
    std::unordered_set<uint32_t> connected;
    for (const auto& e : graph_.edges) {
        connected.insert(e.from_pid);
        connected.insert(e.to_pid);
    }
    std::vector<uint32_t> out;
    for (const auto& [pid, _] : graph_.nodes)
        if (!connected.count(pid)) out.push_back(pid);
    return out;
}

std::unordered_set<uint32_t> ProcessDepMapper::reachable_from(uint32_t pid) const {
    std::unordered_set<uint32_t> visited;
    std::queue<uint32_t> q;
    q.push(pid);
    while (!q.empty()) {
        uint32_t cur = q.front(); q.pop();
        if (!visited.insert(cur).second) continue;
        for (const auto& e : graph_.edges)
            if (e.from_pid == cur && !visited.count(e.to_pid))
                q.push(e.to_pid);
    }
    return visited;
}

std::string ProcessDepMapper::to_dot() const {
    std::ostringstream ss;
    ss << "digraph IPC {\n  rankdir=LR;\n  node [shape=box fontname=\"sans\"];\n";
    for (const auto& [pid, name] : graph_.nodes)
        ss << "  " << pid << " [label=\"" << name << "\\n" << pid << "\"];\n";
    for (const auto& e : graph_.edges)
        ss << "  " << e.from_pid << " -> " << e.to_pid
           << " [label=\"" << e.type << "\"];\n";
    ss << "}\n";
    return ss.str();
}

} // namespace sip::modules
