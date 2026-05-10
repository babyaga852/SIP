#include <gtest/gtest.h>
#include "core/scheduler.hpp"
#include <thread>

using namespace sip;

TEST(Scheduler, RunsRepeatingTask) {
    Scheduler s;
    int count = 0;
    s.every(Duration(10), [&]{ count++; }, "test");
    std::this_thread::sleep_for(std::chrono::milliseconds(35));
    s.tick(); s.tick(); s.tick();
    EXPECT_GE(count, 1);
}

TEST(Scheduler, CancelStopsTask) {
    Scheduler s;
    int count = 0;
    // Use 1ms interval so it fires on first tick
    auto id = s.every(Duration(1), [&]{ count++; }, "cancel_test");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    s.tick();               // fires once
    int after_first = count;
    s.cancel(id);
    s.tick(); s.tick();     // should not fire again
    EXPECT_GE(after_first, 1);
    EXPECT_EQ(count, after_first); // no more increments after cancel
}
