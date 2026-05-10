#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace sip::os {

struct ProcessStat {
    std::uint32_t pid{};
    std::uint32_t ppid{};
    std::string   name;
    double        cpu_percent{};
    std::uint64_t rss_bytes{};
    std::uint64_t vms_bytes{};
    std::string   state;
    std::uint32_t num_threads{};
};

struct ThermalZone {
    std::string name;
    double      celsius{};
};

struct FileStatEntry {
    std::string    path;
    std::int64_t   atime_sec{};
    std::int64_t   mtime_sec{};
    std::uintmax_t size_bytes{};
};

class OSLayer {
public:
    static std::unique_ptr<OSLayer> create();

    virtual ~OSLayer() = default;

    virtual std::vector<ProcessStat>   list_processes()    const = 0;
    virtual std::vector<ThermalZone>   thermal_zones()     const = 0;
    virtual std::vector<FileStatEntry> stat_directory(const std::string& path,
                                                      bool recursive = true) const = 0;
    virtual std::vector<std::pair<std::uint32_t,std::uint32_t>> ipc_connections() const = 0;
};

} // namespace sip::os
