#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace sip::os {

// ── Process info ───────────────────────────────────────────────────────────────
struct ProcessInfo {
    uint32_t    pid;
    std::string name;
    float       cpu_percent;   // 0–100 per core
    uint64_t    mem_rss_bytes;
    std::vector<uint32_t> children;
};

// ── Thermal / CPU info ─────────────────────────────────────────────────────────
struct ThermalInfo {
    float cpu_temp_celsius;    // package temperature
    float cpu_usage_percent;   // aggregate
    int   active_cores;
};

// ── File stat ──────────────────────────────────────────────────────────────────
struct FileStat {
    std::string path;
    uint64_t    size_bytes;
    int64_t     last_access_epoch;  // seconds since epoch
    int64_t     last_modify_epoch;
};

// ── Interface ──────────────────────────────────────────────────────────────────

/// List all running processes.
std::vector<ProcessInfo> list_processes();

/// Read current thermal and CPU info.
ThermalInfo thermal_info();

/// Stat a file (cross-platform).
FileStat stat_file(const std::string& path);

/// Return the IPC connections for a given PID (sockets, pipes).
/// Returns a list of connected PIDs where detectable.
std::vector<uint32_t> ipc_peers(uint32_t pid);

} // namespace sip::os
