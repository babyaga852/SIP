
#include <gtest/gtest.h>
#include "os/os_layer.hpp"

TEST(OsLayer, ListProcessesNotEmpty) {
    auto procs = sip::os::list_processes();
#if defined(SIP_OS_LINUX)
    EXPECT_GT(procs.size(), 0u);
#else
    // Stubs may return empty on macOS/Windows without privileges
    SUCCEED();
#endif
}

TEST(OsLayer, NowMsIsReasonable) {
    auto t = sip::os::now_ms();
    // 2024-01-01 in ms = 1704067200000
    EXPECT_GT(t, 1704067200000LL);
}

TEST(OsLayer, ThrottleDoesNotThrow) {
    EXPECT_NO_THROW(sip::os::throttle_cpu(0.5));
}

TEST(OsLayer, NetStatsReturnsVector) {
    EXPECT_NO_THROW(sip::os::net_stats());
}
