#pragma once
#include <memory>

namespace sip {
class Engine;
class BehaviorDNA;
class FileDecayTracker;
class ProcessDepMapper;
class ThermalScheduler;
class StateDiff;

/// Full-screen ftxui dashboard. Run on the main thread after all modules start.
class Dashboard {
public:
    struct Modules {
        BehaviorDNA*       dna{};
        FileDecayTracker*  decay{};
        ProcessDepMapper*  mapper{};
        ThermalScheduler*  thermal{};
        StateDiff*         state{};
    };

    Dashboard(Engine& engine, Modules mods);
    ~Dashboard();

    /// Blocks until the user presses 'q'.
    void run();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace sip
