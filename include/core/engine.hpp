#pragma once
#include <memory>
#include <vector>
#include <string>
#include "core/scheduler.hpp"
#include "core/storage.hpp"
#include "core/event_loop.hpp"

namespace sip {

class IModule {
public:
    virtual ~IModule() = default;
    virtual std::string name() const = 0;
    virtual void init(class Engine&) = 0;
    virtual void tick()              = 0;
    virtual void shutdown()          = 0;
};

class Engine {
public:
    Engine(); ~Engine();
    void register_module(std::shared_ptr<IModule> mod);
    void run();
    void stop();
    Scheduler&  scheduler();
    Storage&    storage();
    EventLoop&  event_loop();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace sip
