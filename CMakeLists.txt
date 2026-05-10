
cmake_minimum_required(VERSION 3.20)
project(SIP VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SIP_BUILD_TESTS   "Build unit tests"    ON)
option(SIP_ENABLE_FTXUI  "Build TUI dashboard" ON)

find_package(Threads     REQUIRED)
find_package(PkgConfig   REQUIRED)
pkg_check_modules(LIBUV  REQUIRED libuv)
find_package(Boost 1.90 REQUIRED)
find_package(spdlog      REQUIRED)
find_package(nlohmann_json REQUIRED)
pkg_check_modules(SQLite3 REQUIRED sqlite3)
if(SIP_ENABLE_FTXUI)
  find_package(ftxui REQUIRED)
endif()

# ── Core Engine ───────────────────────────────────────────────────────────────
add_library(sip_core STATIC
  src/core/engine.cpp
  src/core/scheduler.cpp
  src/core/storage.cpp
  src/core/event_loop.cpp
)
target_include_directories(sip_core PUBLIC include)
target_link_libraries(sip_core PUBLIC
  Threads::Threads ${LIBUV_LIBRARIES}
  Boost::boost
  spdlog::spdlog nlohmann_json::nlohmann_json sqlite3
)

# ── Modules ───────────────────────────────────────────────────────────────────
foreach(mod behavior_dna file_decay process_mapper thermal_scheduler state_diff)
  add_library(sip_${mod} STATIC src/modules/${mod}.cpp)
  target_include_directories(sip_${mod} PUBLIC include)
  target_link_libraries(sip_${mod} PUBLIC sip_core)
endforeach()

# ── OS Abstraction ────────────────────────────────────────────────────────────
add_library(sip_os STATIC src/os/os_layer.cpp)
target_include_directories(sip_os PUBLIC include)
target_link_libraries(sip_os PUBLIC sip_core)

if(UNIX AND NOT APPLE)
  target_compile_definitions(sip_os PUBLIC SIP_OS_LINUX)
add_compile_definitions(SIP_OS_LINUX)
add_compile_definitions(SIP_OS_LINUX)
  # Linux uses the unified os_layer.cpp
elseif(APPLE)
  target_compile_definitions(sip_os PUBLIC SIP_OS_MACOS)
  target_sources(sip_os PRIVATE src/os/platform/macos.cpp)
  target_link_libraries(sip_os PUBLIC
    "-framework IOKit" "-framework CoreFoundation")
elseif(WIN32)
  target_compile_definitions(sip_os PUBLIC SIP_OS_WINDOWS)
  target_sources(sip_os PRIVATE src/os/platform/windows.cpp)
  target_link_libraries(sip_os PUBLIC pdh iphlpapi wbemuuid psapi)
endif()

# ── UI ────────────────────────────────────────────────────────────────────────
if(SIP_ENABLE_FTXUI)
  add_library(sip_ui STATIC src/ui/dashboard.cpp)
  target_include_directories(sip_ui PUBLIC include)
  target_link_libraries(sip_ui PUBLIC
    sip_core ftxui::screen ftxui::dom ftxui::component)
  target_compile_definitions(sip_ui PUBLIC SIP_ENABLE_FTXUI)

  # CSV diff viewer tool
  add_executable(sip_csv_diff tools/csv_diff_viewer.cpp)
  target_include_directories(sip_csv_diff PRIVATE include)
  target_compile_definitions(sip_csv_diff PRIVATE SIP_ENABLE_FTXUI)
  target_link_libraries(sip_csv_diff PRIVATE
    ftxui::screen ftxui::dom ftxui::component)
endif()

# ── Main executable ───────────────────────────────────────────────────────────
add_executable(sip src/main.cpp)
target_link_libraries(sip PRIVATE
  sip_behavior_dna sip_file_decay sip_process_mapper
  sip_thermal_scheduler sip_state_diff
  sip_core sip_os
  sip_behavior_dna sip_file_decay sip_process_mapper
  sip_thermal_scheduler sip_state_diff
)
if(SIP_ENABLE_FTXUI)
  target_link_libraries(sip PRIVATE sip_ui)
  target_compile_definitions(sip PRIVATE SIP_ENABLE_FTXUI)
endif()

# ── Tests ─────────────────────────────────────────────────────────────────────
if(SIP_BUILD_TESTS)
  enable_testing()
  find_package(GTest REQUIRED)
  add_subdirectory(tests)
endif()

install(TARGETS sip DESTINATION bin)
if(SIP_ENABLE_FTXUI)
  install(TARGETS sip_csv_diff DESTINATION bin)
endif()
