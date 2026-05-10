#pragma once
#include <chrono>
#include <functional>
#include <cstdint>

namespace sip {

using TaskId = std::uint64_t;
using Task   = std::function<void()>;

/// Lightweight periodic task scheduler backed by the event loop timer.
class Scheduler {
public:
    Scheduler();
    ~Scheduler();

    /// Schedule a task to run every `interval`. Returns a cancellable id.
    TaskId schedule(Task task, std::chrono::milliseconds interval);

    /// Cancel a previously scheduled task.
    void cancel(TaskId id);

    /// Run all tasks whose next-fire time has passed. Called by the event loop.
    void tick();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace sip
