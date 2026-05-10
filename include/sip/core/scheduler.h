#pragma once
#include <functional>
#include <chrono>
#include <string>
#include <map>
#include <mutex>
#include <atomic>

namespace sip::core {

using Task     = std::function<void()>;
using TaskId   = std::string;
using Duration = std::chrono::milliseconds;

/// Periodic task scheduler.
/// Tasks are registered with a name and an interval; the scheduler fires them
/// on a background thread via the shared event loop.
class Scheduler {
public:
    Scheduler();
    ~Scheduler();

    /// Register a recurring task. Safe to call from any thread.
    void add(TaskId id, Duration interval, Task fn);

    /// Remove a registered task. No-op if id is unknown.
    void remove(const TaskId& id);

    /// Start the scheduler loop (non-blocking; runs on internal thread).
    void start();

    /// Gracefully stop all tasks.
    void stop();

    bool running() const { return running_; }

private:
    struct Entry {
        Duration             interval;
        Task                 fn;
        std::chrono::steady_clock::time_point next_fire;
    };

    void loop();

    std::map<TaskId, Entry>  tasks_;
    std::mutex               mu_;
    std::atomic<bool>        running_{false};
    std::thread              thread_;
};

} // namespace sip::core
