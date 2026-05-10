#pragma once
#include "core/engine.hpp"
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace sip {

struct ThermalZone {
    std::string id;
    std::string type;   // cpu | gpu | battery
    double      temp_c;
    double      throttle_pct;  // 0 = none, 100 = max throttle
};

class ThermalScheduler : public IModule {
public:
    std::string name() const override { return "ThermalScheduler"; }
    void init(Engine& eng) override;
    void tick() override;
    void shutdown() override;

    std::vector<ThermalZone> zones()  const;
    nlohmann::json           report() const;

    void set_threshold(double warn_c, double critical_c);

private:
    Engine*                  engine_{nullptr};
    double                   warn_c_{75.0};
    double                   critical_c_{90.0};
    std::vector<ThermalZone> zones_;

    void read_zones();
    void apply_throttle(ThermalZone& z);
};

} // namespace sip
