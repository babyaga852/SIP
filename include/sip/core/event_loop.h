#pragma once
#include <uv.h>
#include <functional>
#include <string>

namespace sip::core {

/// Thin RAII wrapper around a libuv event loop.
/// Provides FS watch helpers used by BehaviorDNA and FileDecayTracker.
class EventLoop {
public:
    using FsCallback = std::function<void(const std::string& path, int events)>;

    EventLoop();
    ~EventLoop();

    /// Start watching a path for change events.
    void watch(const std::string& path, FsCallback cb);

    /// Run the loop (blocks until stop() is called).
    void run();

    /// Signal the loop to exit after the current iteration.
    void stop();

    uv_loop_t* handle() { return loop_; }

private:
    uv_loop_t* loop_{nullptr};
};

} // namespace sip::core
