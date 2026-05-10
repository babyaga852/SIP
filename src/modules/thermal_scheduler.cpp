#include "modules/thermal_scheduler.hpp"
#include "os/os_layer.hpp"
#include <spdlog/spdlog.h>

namespace sip {

void ThermalScheduler::init(Engine& eng) {
    engine_ = &eng;
    eng.scheduler().every(std::chrono::seconds(2), [this]{ tick(); }, "ThermalSched");
}

void ThermalScheduler::tick() { read_zones(); }
void ThermalScheduler::shutdown() {}

void ThermalScheduler::set_threshold(double warn_c, double critical_c) {
    warn_c_     = warn_c;
    critical_c_ = critical_c;
}

void ThermalScheduler::read_zones() {
    zones_.clear();
    for (auto& r : sip::os::thermal_readings()) {
        ThermalZone z{r.id, r.type, r.temp_c, 0.0};
        apply_throttle(z);
        zones_.push_back(z);
    }
    engine_->storage().set("thermal.zones", report());
    engine_->storage().append_metric("cpu_temp",
        zones_.empty() ? 0.0 : zones_[0].temp_c);
}

void ThermalScheduler::apply_throttle(ThermalZone& z) {
    if (z.temp_c < warn_c_) {
        z.throttle_pct = 0.0;
    } else if (z.temp_c >= critical_c_) {
        z.throttle_pct = 80.0;
        sip::os::throttle_cpu(0.2);
        spdlog::warn("[Thermal] critical temp {:.1f}°C – throttling CPU", z.temp_c);
    } else {
        double ratio = (z.temp_c - warn_c_) / (critical_c_ - warn_c_);
        z.throttle_pct = ratio * 60.0;
        sip::os::throttle_cpu(1.0 - ratio * 0.6);
    }
}

std::vector<ThermalZone> ThermalScheduler::zones() const { return zones_; }

nlohmann::json ThermalScheduler::report() const {
    nlohmann::json out = nlohmann::json::array();
    for (auto& z : zones_)
        out.push_back({{"id", z.id}, {"temp_c", z.temp_c},
                       {"throttle_pct", z.throttle_pct}});
    return out;
}

} // namespace sip
