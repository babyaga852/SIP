# SIP — System Intelligence Platform

A cross-platform C++ system monitoring daemon with five intelligence modules, a live ftxui TUI dashboard, and a persistent log strip.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Shared Core Engine                       │
│            Scheduler · EventLoop · Storage                  │
└────┬──────────┬──────────┬──────────┬──────────┬───────────┘
     │          │          │          │          │
┌────▼───┐ ┌───▼────┐ ┌───▼────┐ ┌───▼────┐ ┌──▼─────┐
│Behavior│ │  File  │ │Process │ │Thermal │ │ State  │
│  DNA   │ │ Decay  │ │ Mapper │ │Sched.  │ │  Diff  │
└────────┘ └────────┘ └────────┘ └────────┘ └────────┘
     │          │          │          │          │
┌────▼──────────▼──────────▼──────────▼──────────▼───────────┐
│                    OS Abstraction Layer                      │
│     Linux (/proc)  ·  macOS (IOKit)  ·  Windows (PDH)      │
└─────────────────────────────────────────────────────────────┘
     │                                          │
┌────▼────────────────────┐  ┌──────────────────▼────────────┐
│     CLI / TUI Output    │  │      Data Store / Export       │
│  ftxui · log strip      │  │  SQLite · JSON · CSV diffs     │
└─────────────────────────┘  └───────────────────────────────┘
```

---

## Modules

| Module | What it does |
|---|---|
| **BehaviorDNA** | Builds process fingerprints (CPU, RSS, FDs) via exponential moving average. Flags anomalies when Mahalanobis distance from baseline exceeds 3σ |
| **FileDecayTracker** | Watches a directory via libuv fs events. Scores each file: `0.6 × recency_decay + 0.4 × log_frequency` |
| **ProcessMapper** | Reads the live PID→PPID tree and IPC edges. Exports as Graphviz DOT or JSON |
| **ThermalScheduler** | Reads thermal zone temperatures. Proactively throttles CPU at configurable thresholds (default warn 75°C, critical 90°C) |
| **StateDiff** | Takes periodic system snapshots and diffs them. Records added/removed/changed keys. Exports to CSV |

---

## TUI Dashboard

Five live tabs, switchable by mouse click or keyboard:

```
 Thermal | BehaviorDNA | FileDecay | ProcessMap | StateDiff      OK
─────────────────────────────────────────────────────────────────────
  [tab content with sparkline charts and live data]

 LOG  last 3 messages — all modules                        42 total
09:42:51 WARN  [Thermal]      critical temp 93.0°C – throttling CPU
09:42:51 WARN  [BehaviorDNA]  4 anomalies detected
09:42:52 INFO  [StateDiff]    2 changes detected
```

**Controls:**

| Key | Action |
|-----|--------|
| `←` `→` or `h` `l` | Switch tabs |
| `1` – `5` | Jump to tab directly |
| Mouse click | Click tab to switch |
| `q` | Quit |

---

## Build

### Requirements

- CMake ≥ 3.20
- GCC 12+ / Clang 14+ / MSVC 2022
- C++20

### Linux (apt)

```bash
sudo apt install -y \
  build-essential cmake pkg-config \
  libspdlog-dev nlohmann-json3-dev libsqlite3-dev \
  libboost-all-dev libuv1-dev libgtest-dev libftxui-dev
```

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DSIP_ENABLE_FTXUI=ON -DSIP_BUILD_TESTS=ON
cmake --build build -j$(nproc)
```

### macOS (brew)

```bash
brew install cmake spdlog nlohmann-json sqlite3 boost libuv googletest ftxui
cmake -B build -DCMAKE_BUILD_TYPE=Release -DSIP_ENABLE_FTXUI=ON
cmake --build build -j$(nproc)
```

### Windows (vcpkg)

```powershell
git clone https://github.com/microsoft/vcpkg
.\vcpkg\bootstrap-vcpkg.bat
cmake -B build -DCMAKE_TOOLCHAIN_FILE=.\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release
```

---

## Run

```bash
# Monitor home directory
./build/sip ~

# Monitor a specific path
./build/sip /path/to/watch

# Run tests
./build/tests/sip_tests --gtest_color=yes

# View a CSV diff export
./build/sip_csv_diff sip_diff.csv
```

### Output files

| File | Contents |
|------|----------|
| `sip.db` | SQLite database — all metrics and KV state |
| `sip_snapshot.json` | Full JSON export written on clean shutdown |
| `sip_diff.csv` | State diff history (written by StateDiff module) |

---

## Project Structure

```
sip/
├── include/
│   ├── core/          # Engine, Scheduler, Storage, EventLoop
│   ├── modules/       # BehaviorDNA, FileDecay, ProcessMapper, Thermal, StateDiff
│   ├── os/            # OS abstraction header
│   └── ui/            # Dashboard, MemSink log capture
├── src/
│   ├── core/          # Core engine implementation
│   ├── modules/       # Module implementations
│   ├── os/
│   │   ├── os_layer.cpp          # Linux implementation
│   │   └── platform/
│   │       ├── macos.cpp         # IOKit SMC, sysctl, libproc
│   │       └── windows.cpp       # WinAPI, PDH, WMI
│   └── ui/            # ftxui dashboard + log strip
├── tests/             # 27 GTest unit tests
├── tools/             # sip_csv_diff viewer
├── CMakeLists.txt
├── vcpkg.json
└── README.md
```

---

## Tech Stack

| Library | Purpose |
|---------|---------|
| [libuv](https://libuv.org) | Async event loop, filesystem watching |
| [Boost](https://www.boost.org) | Filesystem, Asio |
| [spdlog](https://github.com/gabime/spdlog) | Fast logging with custom MemSink |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON serialization |
| [SQLite3](https://sqlite.org) | Metric storage, KV store |
| [ftxui](https://github.com/ArthurSonzogni/FTXUI) | Terminal UI, charts, mouse input |
| [GoogleTest](https://github.com/google/googletest) | Unit testing |

---

## License

[Shivam Bhoyar](https://github.com/babyaga852)
