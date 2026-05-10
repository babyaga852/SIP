#pragma once
#include "core/engine.hpp"
#include <memory>

namespace sip {

class Dashboard {
public:
    explicit Dashboard(Engine& engine);
    ~Dashboard();

    void run();   // blocking TUI loop
    void stop();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace sip
