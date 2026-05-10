#pragma once
#include "sip/os/os_abstraction.h"

namespace sip::modules {

struct ThrottlePolicy {
    float  warn_temp_c   = 75.0f;  // log warning above this
    float  throttle_temp_c = 85.0f; // begin CPU frequency scaling
    float  critical_temp_c = 95.0f; // aggressive throttle + alert
    int    min_cores     = 1;       // never offline more than (total - min_cores)
};

enum class ThermalState { Normal, Warm, Hot, Critical };

/// Monitors CPU temperature and proactively adjusts CPU governor / core affinity
/// to keep the system within the policy's thermal envelope.
class ThermalScheduler {
public:
    explicit ThermalScheduler(ThrottlePolicy policy = {});

    /// Feed the latest ThermalInfo reading. Returns new state.
    ThermalState update(const sip::os::ThermalInfo& ti);

    ThermalState      current_state()   const { return state_; }
    const ThrottlePolicy& policy()      const { return policy_; }
    float             last_temp()       const { return last_temp_; }

private:
    ThermalState classify(float temp) const;
    void         apply_throttle(ThermalState state, int cores);

    ThrottlePolicy policy_;
    ThermalState   state_{ThermalState::Normal};
    float          last_temp_{0.0f};
};

} // namespace sip::modules
