#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace sip::os {

struct ProcessInfo {
    uint32_t    pid;
    uint32_t    ppid;
    std::string name;
    std::string cmdline;
    double      cpu_pct;
    uint64_t    rss_bytes;
    uint32_t    open_fds;
};

struct ThermalReading {
    std::string id;
    std::string type;
    double      temp_c;
};

struct NetStats {
    std::string iface;
    uint64_t    rx_bytes;
    uint64_t    tx_bytes;
};

// Returns all running processes with stats.
std::vector<ProcessInfo>   list_processes();

// Returns thermal sensor readings.
std::vector<ThermalReading> thermal_readings();

// Returns per-interface network counters.
std::vector<NetStats>       net_stats();

// Apply CPU frequency limit (0.0–1.0). Best-effort.
void throttle_cpu(double fraction);

// Milliseconds since epoch.
int64_t now_ms();

} // namespace sip::os
