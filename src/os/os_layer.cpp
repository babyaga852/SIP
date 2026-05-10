#include "os/os_layer.hpp"
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <spdlog/spdlog.h>

namespace sip::os {

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void throttle_cpu([[maybe_unused]] double fraction) {
    spdlog::debug("[OS] throttle_cpu({:.2f})", fraction);
}

std::vector<ProcessInfo> list_processes() {
    std::vector<ProcessInfo> result;
    namespace fs = std::filesystem;
    std::error_code ec;
    for (auto& entry : fs::directory_iterator("/proc", ec)) {
        auto fname = entry.path().filename().string();
        if (!std::all_of(fname.begin(), fname.end(), ::isdigit)) continue;
        uint32_t pid = std::stoul(fname);
        std::ifstream stat(entry.path() / "stat");
        if (!stat) continue;
        ProcessInfo pi{}; pi.pid = pid;
        std::string line; std::getline(stat, line);
        auto lp = line.find('('), rp = line.rfind(')');
        if (lp == std::string::npos) continue;
        pi.name = line.substr(lp + 1, rp - lp - 1);
        std::istringstream ss(line.substr(rp + 2));
        char state; ss >> state >> pi.ppid;
        std::ifstream statm(entry.path() / "statm");
        uint64_t size = 0, rss = 0; statm >> size >> rss;
        pi.rss_bytes = rss * 4096;
        pi.open_fds = static_cast<uint32_t>(
            std::distance(fs::directory_iterator(entry.path() / "fd", ec),
                          fs::directory_iterator{}));
        result.push_back(pi);
    }
    return result;
}

std::vector<ThermalReading> thermal_readings() {
    std::vector<ThermalReading> result;
    namespace fs = std::filesystem;
    std::error_code ec;
    for (auto& entry : fs::directory_iterator("/sys/class/thermal", ec)) {
        auto name = entry.path().filename().string();
        if (name.find("thermal_zone") == std::string::npos) continue;
        std::ifstream temp_f(entry.path() / "temp");
        std::ifstream type_f(entry.path() / "type");
        if (!temp_f) continue;
        ThermalReading r;
        r.id = name;
        int raw = 0; temp_f >> raw;
        r.temp_c = raw / 1000.0;
        if (type_f) std::getline(type_f, r.type);
        result.push_back(r);
    }
    return result;
}

std::vector<NetStats> net_stats() {
    std::vector<NetStats> result;
    std::ifstream f("/proc/net/dev");
    if (!f) return result;
    std::string line;
    std::getline(f, line); std::getline(f, line);
    while (std::getline(f, line)) {
        std::istringstream ss(line);
        std::string iface; ss >> iface;
        if (iface.empty()) continue;
        if (iface.back() == ':') iface.pop_back();
        NetStats n; n.iface = iface;
        uint64_t dummy = 0;
        ss >> n.rx_bytes >> dummy >> dummy >> dummy
           >> dummy >> dummy >> dummy >> dummy
           >> n.tx_bytes;
        result.push_back(n);
    }
    return result;
}

} // namespace sip::os
