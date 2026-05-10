#pragma once
#include <string>
#include <vector>

namespace sip {
class Engine;

struct ThermalReading {
    std::string zone;        ///< e.g. "cpu-thermal", "acpitz"
    double      celsius{};
};

struct ThrottleAction {
    std::uint32_t pid{};
    std::string   process_name;
    int           cpu_affinity_limit{};  ///< New core count limit (0 = unrestricted)
    std::string   reason;
};

/// Monitors CPU thermal zones and proactively throttles hot processes
/// before the kernel's own thermal management kicks in.
class ThermalScheduler {
public:
    explicit ThermalScheduler(Engine& engine);
    ~ThermalScheduler();

    void set_throttle_threshold(double celsius);  ///< Default: 80°C
    void start();
    void stop();

    std::vector<ThermalReading>  current_readings() const;
    std::vector<ThrottleAction>  active_throttles() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace sip
