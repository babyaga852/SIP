#pragma once
#include <functional>
#include <memory>

namespace sip {

/// Thin wrapper around libuv's uv_loop_t.
class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    /// Run until stop() is called.
    void run();

    /// Signal the loop to exit after current iteration.
    void stop();

    /// Post a callback to be executed on the loop thread (thread-safe).
    void post(std::function<void()> fn);

    /// Underlying loop handle (for libuv interop).
    void* handle();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace sip
