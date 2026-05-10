#pragma once
#include <functional>
#include <string>
#include <memory>
struct uv_loop_s;

namespace sip {
class EventLoop {
public:
    using FsCallback = std::function<void(const std::string& path, int events)>;
    EventLoop(); ~EventLoop();
    void watch_path(const std::string& path, FsCallback cb);
    void poll(); void run(); void stop();
    uv_loop_s* raw();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace sip
