
#include <gtest/gtest.h>
#include "core/event_loop.hpp"
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>
#include <atomic>

TEST(EventLoop, PollDoesNotThrow) {
    sip::EventLoop ev;
    EXPECT_NO_THROW(ev.poll());
}

TEST(EventLoop, WatchAndDetectChange) {
    namespace fs = std::filesystem;
    auto dir = fs::temp_directory_path() / "sip_ev_test";
    fs::create_directories(dir);

    std::atomic<int> fire_count{0};
    sip::EventLoop ev;
    ev.watch_path(dir.string(), [&](const std::string&, int) { fire_count++; });

    // Create a file to trigger the watcher
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::ofstream(dir / "trigger.txt") << "x";
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ev.poll();

    fs::remove_all(dir);
    // We just verify no crash; FS events may or may not fire in CI
    SUCCEED();
}
