#include "sip/modules/thermal_scheduler.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <string>
#include <filesystem>

namespace sip::modules {

ThermalScheduler::ThermalScheduler(ThrottlePolicy policy)
    : policy_(policy) {}

ThermalState ThermalScheduler::update(const sip::os::ThermalInfo& ti) {
    last_temp_ = ti.cpu_temp_celsius;
    auto new_state = classify(last_temp_);

    if (new_state != state_) {
        const char* names[] = {"Normal","Warm","Hot","Critical"};
        spdlog::warn("[ThermalScheduler] state {} → {} @ {:.1f}°C",
                     names[(int)state_], names[(int)new_state], last_temp_);
        state_ = new_state;
        apply_throttle(state_, ti.active_cores);
    }
    return state_;
}

ThermalState ThermalScheduler::classify(float temp) const {
    if (temp >= policy_.critical_temp_c) return ThermalState::Critical;
    if (temp >= policy_.throttle_temp_c) return ThermalState::Hot;
    if (temp >= policy_.warn_temp_c)     return ThermalState::Warm;
    return ThermalState::Normal;
}

void ThermalScheduler::apply_throttle(ThermalState state, int /*cores*/) {
#ifdef SIP_OS_LINUX
    // Set CPU governor via sysfs
    auto set_governor = [](const std::string& gov) {
        namespace fs = std::filesystem;
        for (int i = 0; i < 32; ++i) {
            std::string path = "/sys/devices/system/cpu/cpu" + std::to_string(i)
                             + "/cpufreq/scaling_governor";
            if (!fs::exists(path)) break;
            std::ofstream f(path);
            if (f) { f << gov; }
        }
        spdlog::info("[ThermalScheduler] set governor → {}", gov);
    };

    switch (state) {
        case ThermalState::Normal:   set_governor("performance"); break;
        case ThermalState::Warm:     set_governor("schedutil");   break;
        case ThermalState::Hot:      set_governor("powersave");   break;
        case ThermalState::Critical: set_governor("powersave");
            spdlog::error("[ThermalScheduler] CRITICAL TEMP — consider shutdown!"); break;
    }
#else
    (void)state;
    spdlog::debug("[ThermalScheduler] throttle applied (stub on non-Linux)");
#endif
}

} // namespace sip::modules
