#include <gtest/gtest.h>
#include "core/storage.hpp"

using namespace sip;

TEST(Storage, SetGet) {
    Storage st;
    st.set("foo", 42);
    EXPECT_EQ(st.get("foo"), 42);
}

TEST(Storage, Has) {
    Storage st;
    EXPECT_FALSE(st.has("bar"));
    st.set("bar", "hello");
    EXPECT_TRUE(st.has("bar"));
}

TEST(Storage, Del) {
    Storage st;
    st.set("x", 1);
    st.del("x");
    EXPECT_FALSE(st.has("x"));
}

TEST(Storage, AppendMetric) {
    Storage st;
    EXPECT_NO_THROW(st.append_metric("cpu_temp", 55.5));
}
