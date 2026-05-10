
#include <gtest/gtest.h>
#include "modules/thermal_scheduler.hpp"
#include "core/engine.hpp"

using namespace sip;

TEST(ThermalScheduler, InitAndName) {
    Engine eng;
    auto ts = std::make_shared<ThermalScheduler>();
    eng.register_module(ts);
    EXPECT_EQ(ts->name(), "ThermalScheduler");
}

TEST(ThermalScheduler, ThresholdSetting) {
    Engine eng;
    auto ts = std::make_shared<ThermalScheduler>();
    eng.register_module(ts);
    EXPECT_NO_THROW(ts->set_threshold(70.0, 85.0));
}

TEST(ThermalScheduler, ReportIsArray) {
    Engine eng;
    auto ts = std::make_shared<ThermalScheduler>();
    eng.register_module(ts);
    ts->tick();
    EXPECT_TRUE(ts->report().is_array());
}
