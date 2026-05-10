#include "core/engine.hpp"
#include "modules/behavior_dna.hpp"
#include "modules/file_decay.hpp"
#include "modules/process_mapper.hpp"
#include "modules/thermal_scheduler.hpp"
#include "modules/state_diff.hpp"
#include <spdlog/spdlog.h>
#include <csignal>
#include <memory>
#include <iostream>

#ifdef SIP_ENABLE_FTXUI
#include "ui/dashboard.hpp"
#include <thread>
#endif

static sip::Engine* g_engine = nullptr;

static void handle_signal(int) {
    if (g_engine) g_engine->stop();
}

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::info);
    spdlog::info("SIP v0.1.0 starting");

    sip::Engine engine;
    g_engine = &engine;
    std::signal(SIGINT,  handle_signal);
    std::signal(SIGTERM, handle_signal);

    // Register all modules
    engine.register_module(std::make_shared<sip::BehaviorDNA>());
    engine.register_module(std::make_shared<sip::ProcessMapper>());
    engine.register_module(std::make_shared<sip::ThermalScheduler>());
    engine.register_module(std::make_shared<sip::StateDiff>());

    auto fd = std::make_shared<sip::FileDecayTracker>();
    engine.register_module(fd);
    // Watch current directory by default; override via CLI args
    const char* watch_root = (argc > 1) ? argv[1] : ".";
    fd->watch(watch_root);

#ifdef SIP_ENABLE_FTXUI
    sip::Dashboard dash(engine);
    std::thread ui_thread([&]{ dash.run(); });
    engine.run();
    dash.stop();
    ui_thread.join();
#else
    engine.run();
#endif

    // Export final state
    engine.storage().snapshot("sip_snapshot.json");
    spdlog::info("Snapshot written to sip_snapshot.json");
    return 0;
}
