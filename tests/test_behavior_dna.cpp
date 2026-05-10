
#include <gtest/gtest.h>
#include "modules/behavior_dna.hpp"
#include "core/engine.hpp"

using namespace sip;

TEST(BehaviorDNA, InitWithEngine) {
    Engine eng;
    auto dna = std::make_shared<BehaviorDNA>();
    EXPECT_NO_THROW(eng.register_module(dna));
    EXPECT_EQ(dna->name(), "BehaviorDNA");
}

TEST(BehaviorDNA, NoAnomaliesOnFirstTick) {
    Engine eng;
    auto dna = std::make_shared<BehaviorDNA>();
    eng.register_module(dna);
    dna->tick();
    // First tick builds baseline; no anomalies expected
    EXPECT_EQ(dna->anomalies().size(), 0u);
}

TEST(BehaviorDNA, ReportIsArray) {
    Engine eng;
    auto dna = std::make_shared<BehaviorDNA>();
    eng.register_module(dna);
    EXPECT_TRUE(dna->report().is_array());
}
