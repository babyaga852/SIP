#include "sip/os/os_abstraction.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <sys/stat.h>
#include <ctime>

namespace sip::os {

// ── Helpers ────────────────────────────────────────────────────────────────────
static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// ── Process list ───────────────────────────────────────────────────────────────
std::vector<ProcessInfo> list_processes() {
    std::vector<ProcessInfo> procs;
    for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
        const auto& p = entry.path().filename().string();
        // Only numeric directories are PIDs
        if (p.empty() || !std::isdigit(p[0])) continue;
        uint32_t pid = std::stoul(p);

        std::string comm = read_file("/proc/" + p + "/comm");
        if (comm.empty()) continue;
        if (!comm.empty() && comm.back() == '\n') comm.pop_back();

        ProcessInfo pi;
        pi.pid  = pid;
        pi.name = comm;
        pi.cpu_percent  = 0.0f; // full calculation needs two samples
        pi.mem_rss_bytes = 0;

        // Parse /proc/<pid>/status for VmRSS
        std::string status = read_file("/proc/" + p + "/status");
        std::istringstream ss(status);
        std::string line;
        while (std::getline(ss, line)) {
            if (line.rfind("VmRSS:", 0) == 0) {
                uint64_t kb = std::stoull(line.substr(6));
                pi.mem_rss_bytes = kb * 1024;
                break;
            }
        }
        procs.push_back(pi);
    }
    return procs;
}

// ── Thermal info ───────────────────────────────────────────────────────────────
ThermalInfo thermal_info() {
    ThermalInfo ti{};
    // Read first available thermal zone
    const std::string base = "/sys/class/thermal/";
    for (int i = 0; i < 8; ++i) {
        std::string zone = base + "thermal_zone" + std::to_string(i) + "/temp";
        std::string val  = read_file(zone);
        if (!val.empty()) {
            ti.cpu_temp_celsius = std::stof(val) / 1000.0f;
            break;
        }
    }
    // /proc/stat for usage (single snapshot — caller should diff)
    std::string stat = read_file("/proc/stat");
    if (!stat.empty()) {
        std::istringstream ss(stat);
        std::string cpu;
        uint64_t user, nice, system, idle, iowait, irq, softirq;
        ss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq;
        uint64_t total = user + nice + system + idle + iowait + irq + softirq;
        uint64_t busy  = total - idle - iowait;
        ti.cpu_usage_percent = total ? (float)busy / (float)total * 100.0f : 0.0f;
    }
    ti.active_cores = (int)std::thread::hardware_concurrency();
    return ti;
}

// ── File stat ──────────────────────────────────────────────────────────────────
FileStat stat_file(const std::string& path) {
    struct stat s{};
    FileStat fs;
    fs.path = path;
    if (::stat(path.c_str(), &s) == 0) {
        fs.size_bytes        = (uint64_t)s.st_size;
        fs.last_access_epoch = (int64_t)s.st_atime;
        fs.last_modify_epoch = (int64_t)s.st_mtime;
    }
    return fs;
}

// ── IPC peers ──────────────────────────────────────────────────────────────────
std::vector<uint32_t> ipc_peers(uint32_t /*pid*/) {
    // Full netlink-based IPC graph is complex; return empty for now.
    // TODO: parse /proc/<pid>/net/unix and cross-reference inodes.
    return {};
}

} // namespace sip::os
