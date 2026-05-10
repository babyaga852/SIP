#pragma once
#include <memory>
#include <string>

namespace sip {

class Scheduler;
class EventLoop;
class Storage;

/// Central engine — owns the scheduler, event loop, and storage.
/// Every module receives a non-owning reference to the engine.
class Engine {
public:
    explicit Engine(const std::string& data_dir);
    ~Engine();

    Engine(const Engine&)            = delete;
    Engine& operator=(const Engine&) = delete;

    /// Start all subsystems. Blocks until stop() is called.
    void run();

    /// Signal a clean shutdown from any thread.
    void stop();

    Scheduler&  scheduler()  noexcept;
    EventLoop&  event_loop() noexcept;
    Storage&    storage()    noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace sip
