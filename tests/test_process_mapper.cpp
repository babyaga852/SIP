
#include <gtest/gtest.h>
#include "modules/process_mapper.hpp"
#include "core/engine.hpp"

using namespace sip;

TEST(ProcessMapper, InitAndName) {
    Engine eng;
    auto pm = std::make_shared<ProcessMapper>();
    eng.register_module(pm);
    EXPECT_EQ(pm->name(), "ProcessMapper");
}

TEST(ProcessMapper, GraphHasNodesAndEdges) {
    Engine eng;
    auto pm = std::make_shared<ProcessMapper>();
    eng.register_module(pm);
    pm->tick();
    auto g = pm->graph();
    EXPECT_TRUE(g.contains("nodes"));
    EXPECT_TRUE(g.contains("edges"));
}

TEST(ProcessMapper, DotContainsDigraph) {
    Engine eng;
    auto pm = std::make_shared<ProcessMapper>();
    eng.register_module(pm);
    pm->tick();
    auto dot = pm->dot();
    EXPECT_NE(dot.find("digraph"), std::string::npos);
}
