#include "modules/process_mapper.hpp"
#include "os/os_layer.hpp"
#include <spdlog/spdlog.h>
#include <sstream>

namespace sip {

void ProcessMapper::init(Engine& eng) {
    engine_ = &eng;
    eng.scheduler().every(std::chrono::seconds(10), [this]{ tick(); }, "ProcessMapper");
}

void ProcessMapper::tick() { refresh_processes(); refresh_ipc(); }
void ProcessMapper::shutdown() {}

void ProcessMapper::refresh_processes() {
    nodes_.clear();
    for (auto& p : sip::os::list_processes())
        nodes_[p.pid] = {p.pid, p.ppid, p.name, p.cmdline};
}

void ProcessMapper::refresh_ipc() {
    edges_.clear();
    // On Linux, parse /proc/<pid>/net/unix + /proc/<pid>/fd for socket pairs.
    // Stub: parent→child edges from PPID relationships.
    for (auto& [pid, node] : nodes_) {
        if (nodes_.count(node.ppid) && node.ppid != 0)
            edges_.push_back({node.ppid, pid, "parent", 0});
    }
    engine_->storage().set("process_mapper.graph", graph());
}

nlohmann::json ProcessMapper::graph() const {
    nlohmann::json nodes = nlohmann::json::array();
    nlohmann::json edges = nlohmann::json::array();
    for (auto& [pid, n] : nodes_)
        nodes.push_back({{"pid", n.pid}, {"name", n.name}});
    for (auto& e : edges_)
        edges.push_back({{"src", e.src_pid}, {"dst", e.dst_pid}, {"type", e.type}});
    return {{"nodes", nodes}, {"edges", edges}};
}

std::string ProcessMapper::dot() const {
    std::ostringstream ss;
    ss << "digraph sip_processes {\n";
    for (auto& [pid, n] : nodes_)
        ss << "  " << pid << " [label=\"" << n.name << "\"];\n";
    for (auto& e : edges_)
        ss << "  " << e.src_pid << " -> " << e.dst_pid
           << " [label=\"" << e.type << "\"];\n";
    ss << "}\n";
    return ss.str();
}

} // namespace sip
