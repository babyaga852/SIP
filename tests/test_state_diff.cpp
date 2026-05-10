
#include <gtest/gtest.h>
#include "modules/state_diff.hpp"
#include "core/engine.hpp"
#include <nlohmann/json.hpp>

using namespace sip;

TEST(StateDiff, InitAndName) {
    Engine eng;
    auto sd = std::make_shared<StateDiff>();
    eng.register_module(sd);
    EXPECT_EQ(sd->name(), "StateDiff");
}

TEST(StateDiff, SnapshotIsObject) {
    Engine eng;
    auto sd = std::make_shared<StateDiff>();
    eng.register_module(sd);
    // snapshot returns array of diffs (empty on init)
    EXPECT_TRUE(sd->snapshot().is_array());
}

TEST(StateDiff, CsvExportDoesNotThrow) {
    Engine eng;
    auto sd = std::make_shared<StateDiff>();
    eng.register_module(sd);
    sd->tick();
    EXPECT_NO_THROW(sd->export_csv("/tmp/sip_test_diff.csv"));
}
