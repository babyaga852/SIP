#include "core/engine.hpp"
#include "core/scheduler.hpp"
#include "core/storage.hpp"
#include "core/event_loop.hpp"
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>

namespace sip {

struct Engine::Impl {
    std::unique_ptr<Scheduler>  scheduler  = std::make_unique<Scheduler>();
    std::unique_ptr<Storage>    storage    = std::make_unique<Storage>("sip.db");
    std::unique_ptr<EventLoop>  event_loop = std::make_unique<EventLoop>();
    std::vector<std::shared_ptr<IModule>> modules;
    bool running{false};
};

Engine::Engine()  : impl_(std::make_unique<Impl>()) {}
Engine::~Engine() { stop(); }

Scheduler&  Engine::scheduler()  { return *impl_->scheduler; }
Storage&    Engine::storage()    { return *impl_->storage; }
EventLoop&  Engine::event_loop() { return *impl_->event_loop; }

void Engine::register_module(std::shared_ptr<IModule> mod) {
    spdlog::info("[Engine] registering module: {}", mod->name());
    mod->init(*this);
    impl_->modules.push_back(std::move(mod));
}

void Engine::run() {
    impl_->running = true;
    spdlog::info("[Engine] starting main loop");
    while (impl_->running) {
        impl_->scheduler->tick();
        impl_->event_loop->poll();
        for (auto& m : impl_->modules) m->tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    for (auto& m : impl_->modules) m->shutdown();
    spdlog::info("[Engine] stopped");
}

void Engine::stop() {
    impl_->running = false;
    impl_->event_loop->stop();
}

} // namespace sip
