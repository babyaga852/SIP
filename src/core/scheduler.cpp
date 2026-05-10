#include "core/scheduler.hpp"
#include <spdlog/spdlog.h>

namespace sip {

TaskId Scheduler::every(Duration interval, TaskFn fn, std::string label) {
    std::lock_guard lock(mu_);
    TaskId id = next_id_++;
    tasks_[id] = {id, std::move(label), std::move(fn), interval, true,
                  std::chrono::steady_clock::now() + interval};
    return id;
}

TaskId Scheduler::after(Duration delay, TaskFn fn, std::string label) {
    std::lock_guard lock(mu_);
    TaskId id = next_id_++;
    tasks_[id] = {id, std::move(label), std::move(fn), delay, false,
                  std::chrono::steady_clock::now() + delay};
    return id;
}

void Scheduler::cancel(TaskId id) {
    std::lock_guard lock(mu_);
    tasks_.erase(id);
}

void Scheduler::tick() {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard lock(mu_);
    std::vector<TaskId> to_remove;
    for (auto& [id, t] : tasks_) {
        if (now >= t.next_run) {
            try { t.fn(); }
            catch (const std::exception& e) {
                spdlog::error("[Scheduler] task '{}' threw: {}", t.label, e.what());
            }
            if (t.repeating) t.next_run = now + t.interval;
            else             to_remove.push_back(id);
        }
    }
    for (auto id : to_remove) tasks_.erase(id);
}

} // namespace sip
