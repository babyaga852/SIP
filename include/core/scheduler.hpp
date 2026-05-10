#pragma once
#include <chrono>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <atomic>

namespace sip {
using TaskFn   = std::function<void()>;
using TaskId   = uint64_t;
using Duration = std::chrono::milliseconds;

class Scheduler {
public:
    TaskId every(Duration interval, TaskFn fn, std::string label = "");
    TaskId after(Duration delay,    TaskFn fn, std::string label = "");
    void   cancel(TaskId id);
    void   tick();
private:
    struct Task {
        TaskId id; std::string label; TaskFn fn;
        Duration interval; bool repeating;
        std::chrono::steady_clock::time_point next_run;
    };
    std::mutex            mu_;
    std::map<TaskId,Task> tasks_;
    std::atomic<TaskId>   next_id_{1};
};
} // namespace sip
