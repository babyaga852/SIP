# ── Shared Core Engine library ─────────────────────────────────────────────────
add_library(sip_core
    core/engine.cpp
    core/scheduler.cpp
    core/event_loop.cpp
    core/storage.cpp
)
target_include_directories(sip_core PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(sip_core
    PUBLIC
        Threads::Threads
        Boost::filesystem
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        SQLite::SQLite3
        $<IF:$<TARGET_EXISTS:libuv::uv>,libuv::uv,${LIBUV_LIBRARIES}>
)

# ── OS Abstraction ────────────────────────────────────────────────────────────
add_library(sip_os
    os/os_layer.cpp
    os/process_info.cpp
    os/thermal_info.cpp
    os/fs_watch.cpp
)
target_include_directories(sip_os PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(sip_os PUBLIC sip_core)

# ── Modules ───────────────────────────────────────────────────────────────────
add_library(sip_modules
    modules/behavior_dna.cpp
    modules/file_decay_tracker.cpp
    modules/process_dep_mapper.cpp
    modules/thermal_scheduler.cpp
    modules/state_diff.cpp
)
target_link_libraries(sip_modules PUBLIC sip_core sip_os)

# ── UI (ftxui) ────────────────────────────────────────────────────────────────
add_library(sip_ui
    ui/dashboard.cpp
    ui/anomaly_view.cpp
)
target_link_libraries(sip_ui PUBLIC sip_core ftxui::screen ftxui::dom ftxui::component)

# ── Export / Data Store ───────────────────────────────────────────────────────
add_library(sip_export
    export/json_snapshot.cpp
    export/sqlite_history.cpp
    export/csv_report.cpp
    export/state_diff_export.cpp
)
target_link_libraries(sip_export PUBLIC sip_core)

# ── Main executable ───────────────────────────────────────────────────────────
add_executable(sip main.cpp)
target_link_libraries(sip PRIVATE sip_core sip_os sip_modules sip_ui sip_export)
install(TARGETS sip DESTINATION bin)
