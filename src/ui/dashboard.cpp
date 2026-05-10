
#include "ui/dashboard.hpp"
#include "ui/log_sink.hpp"
#include "core/engine.hpp"
#include "core/storage.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <nlohmann/json.hpp>
#include <deque>
#include <algorithm>
#include <numeric>
#include <string>
#include <thread>
#include <atomic>
#include <map>
#include <iomanip>
#include <sstream>
#include <memory>

#ifdef SIP_ENABLE_FTXUI
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/dom/elements.hpp>
using namespace ftxui;
#endif

namespace sip {

// ── Ring buffer ───────────────────────────────────────────────────────────────
struct RingBuffer {
    std::deque<double> data;
    size_t max_size;
    explicit RingBuffer(size_t sz = 80) : max_size(sz) {}
    void push(double v) {
        data.push_back(v);
        if (data.size() > max_size) data.pop_front();
    }
    double max_val() const {
        if (data.empty()) return 1.0;
        return std::max(1.0, *std::max_element(data.begin(), data.end()));
    }
    double last() const { return data.empty() ? 0.0 : data.back(); }
    double avg()  const {
        if (data.empty()) return 0.0;
        return std::accumulate(data.begin(), data.end(), 0.0) / data.size();
    }
};

#ifdef SIP_ENABLE_FTXUI

// ── Helpers ───────────────────────────────────────────────────────────────────
static std::string fmt1(double v) {
    std::ostringstream ss; ss << std::fixed << std::setprecision(1) << v;
    return ss.str();
}
static std::string fmt0(double v) { return std::to_string((int)v); }
static std::string pad(std::string s, int w) {
    while ((int)s.size() < w) s += ' ';
    return s.substr(0, w);
}

static Color temp_col(double t) {
    if (t >= 90) return Color::Red;
    if (t >= 75) return Color::Yellow;
    if (t >= 60) return Color::Green;
    return Color::Cyan;
}

// Canvas sparkline — filled bars from baseline
static Element sparkline_canvas(const RingBuffer& buf, int width, int height, Color col) {
    auto c = Canvas(width * 2, height * 4);
    double mx = buf.max_val();
    int n     = static_cast<int>(buf.data.size());
    int start = std::max(0, n - width);
    for (int i = start; i < n; ++i) {
        int xi   = i - start;
        double v = buf.data[i];
        double frac = (mx > 0) ? (v / mx) : 0.0;
        int top_y = static_cast<int>((1.0 - frac) * (height * 4 - 1));
        top_y = std::clamp(top_y, 0, height * 4 - 1);
        for (int y = height * 4 - 1; y >= top_y; --y) {
            c.DrawPointOn(xi * 2,     y);
            c.DrawPointOn(xi * 2 + 1, y);
        }
    }
    return canvas(std::move(c)) | color(col);
}

// Gradient horizontal bar
static Element hbar_grad(double pct, int width) {
    int filled = static_cast<int>(pct / 100.0 * width);
    filled = std::clamp(filled, 0, width);
    Color col = pct >= 80 ? Color::Red   :
                pct >= 60 ? Color::Yellow :
                pct >= 40 ? Color::Green  : Color::Cyan;
    return hbox({
        text("[") | color(Color::GrayDark),
        text(std::string(filled, '#'))          | color(col) | bold,
        text(std::string(width - filled, '-'))  | color(Color::GrayDark),
        text("]") | color(Color::GrayDark),
        text(" " + fmt0(pct) + "%") | color(col),
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// LOG STRIP — always-visible 3-line panel rendered at the bottom of every tab
// ─────────────────────────────────────────────────────────────────────────────
static Element render_log_strip(const std::shared_ptr<MemSink>& sink) {
    auto entries = sink->tail(3);

    Elements lines;
    for (auto& e : entries) {
        Color level_col = Color::GrayDark;
        std::string level_str = "     ";
        switch (e.level) {
            case spdlog::level::warn:
                level_col = Color::Yellow; level_str = "WARN "; break;
            case spdlog::level::err:
            case spdlog::level::critical:
                level_col = Color::Red;    level_str = "ERR  "; break;
            case spdlog::level::info:
                level_col = Color::Cyan;   level_str = "INFO "; break;
            case spdlog::level::debug:
                level_col = Color::GrayDark; level_str = "DBG  "; break;
            default: break;
        }

        // Truncate message to fit terminal width
        std::string msg = e.message;
        if (msg.size() > 70) msg = msg.substr(0, 67) + "...";

        lines.push_back(hbox({
            text(e.timestamp) | color(Color::GrayDark) | size(WIDTH, EQUAL, 9),
            text(level_str)   | color(level_col) | bold | size(WIDTH, EQUAL, 5),
            text(pad("[" + e.logger + "]", 16)) | color(level_col),
            text(msg) | color(e.level >= spdlog::level::warn
                              ? level_col : Color::White),
        }));
    }

    // Pad to always show 3 lines so the strip height is stable
    while ((int)lines.size() < 3)
        lines.push_back(text("") | size(HEIGHT, EQUAL, 1));

    return vbox({
        hbox({
            text(" LOG ") | bold | color(Color::Black) | bgcolor(Color::GrayDark),
            text(" last 3 messages — all modules") | color(Color::GrayDark),
            filler(),
            text(fmt0(sink->size()) + " total ") | color(Color::GrayDark),
        }),
        vbox(std::move(lines)) | color(Color::White),
    }) | border;
}

// ─────────────────────────────────────────────────────────────────────────────
// TAB renders (unchanged except wrapped inside layout_with_log)
// ─────────────────────────────────────────────────────────────────────────────
static Element render_thermal(Storage& st, RingBuffer& rb_temp, RingBuffer& rb_throttle) {
    nlohmann::json zones = nlohmann::json::array();
    if (st.has("thermal.zones")) zones = st.get("thermal.zones");

    double max_temp = 0.0, max_thr = 0.0;
    for (auto& z : zones) {
        max_temp = std::max(max_temp, z.value("temp_c", 0.0));
        max_thr  = std::max(max_thr,  z.value("throttle_pct", 0.0));
    }
    rb_temp.push(max_temp);
    rb_throttle.push(max_thr);

    Elements rows;
    rows.push_back(hbox({
        text(" THERMAL ZONES ") | bold | color(Color::Black) | bgcolor(Color::Cyan),
        text("  live system temperature monitor"),
    }));
    rows.push_back(separator());
    rows.push_back(hbox({
        text("  Max: ")  | color(Color::GrayLight),
        text(fmt1(max_temp) + "C") | color(temp_col(max_temp)) | bold,
        text("   Avg: ") | color(Color::GrayLight),
        text(fmt1(rb_temp.avg()) + "C") | color(temp_col(rb_temp.avg())),
        text("   Peak: ")| color(Color::GrayLight),
        text(fmt1(rb_temp.max_val()) + "C") | color(temp_col(rb_temp.max_val())),
        text("   Samples: ") | color(Color::GrayLight),
        text(fmt0(rb_temp.data.size())),
    }));
    rows.push_back(separator());
    rows.push_back(hbox({
        text(" Temp ") | color(Color::GrayLight) | size(WIDTH, EQUAL, 6),
        sparkline_canvas(rb_temp,     70, 5, Color::Yellow),
        text(" " + fmt1(rb_temp.last())    + "C") | color(temp_col(rb_temp.last())) | bold | size(WIDTH, EQUAL, 8),
    }));
    rows.push_back(hbox({
        text(" Thr  ") | color(Color::GrayLight) | size(WIDTH, EQUAL, 6),
        sparkline_canvas(rb_throttle, 70, 3, Color::Red),
        text(" " + fmt0(rb_throttle.last()) + "% ") | color(Color::Red) | size(WIDTH, EQUAL, 8),
    }));
    rows.push_back(separator());
    rows.push_back(hbox({
        text(pad(" Zone",     16)) | bold | color(Color::Blue),
        text(pad("Temp",       8)) | bold | color(Color::Blue),
        text(pad("Throttle",  38)) | bold | color(Color::Blue),
        text("Status")             | bold | color(Color::Blue),
    }));
    rows.push_back(separator());
    for (auto& z : zones) {
        double t   = z.value("temp_c", 0.0);
        double thr = z.value("throttle_pct", 0.0);
        Color  tc  = temp_col(t);
        std::string status = t >= 90 ? "CRITICAL" : t >= 75 ? "WARNING " : t >= 60 ? "WARM    " : "OK      ";
        Color  sc  = t >= 90 ? Color::Red : t >= 75 ? Color::Yellow : Color::Green;
        rows.push_back(hbox({
            text(pad(" " + z.value("id","?"), 16)) | color(Color::White),
            text(pad(fmt1(t) + "C", 8))            | color(tc) | bold,
            hbar_grad(thr, 30),
            text("  "),
            text(status) | color(sc) | bold,
        }));
    }
    if (zones.empty())
        rows.push_back(text("  No thermal zones detected") | color(Color::GrayDark));
    return vbox(std::move(rows)) | border;
}

static Element render_behavior(Storage& st, RingBuffer& rb) {
    nlohmann::json anomalies = nlohmann::json::array();
    if (st.has("behavior_dna.anomalies")) anomalies = st.get("behavior_dna.anomalies");
    rb.push(static_cast<double>(anomalies.size()));

    Elements rows;
    rows.push_back(hbox({
        text(" BEHAVIOR DNA ") | bold | color(Color::Black) | bgcolor(Color::Red),
        text("  process anomaly detection"),
    }));
    rows.push_back(separator());
    rows.push_back(hbox({
        text("  Current: ") | color(Color::GrayLight),
        text(fmt0(rb.last())) | color(rb.last() > 0 ? Color::Red : Color::Green) | bold,
        text("  Peak: ")    | color(Color::GrayLight),
        text(fmt0(rb.max_val())) | color(rb.max_val() > 0 ? Color::Red : Color::Green),
    }));
    rows.push_back(separator());
    rows.push_back(hbox({
        text(" Rate ") | color(Color::GrayLight) | size(WIDTH, EQUAL, 6),
        sparkline_canvas(rb, 70, 5, Color::Red),
        text(" " + fmt0(rb.last())) | color(Color::Red) | bold | size(WIDTH, EQUAL, 8),
    }));
    rows.push_back(separator());
    if (anomalies.empty()) {
        rows.push_back(text("  All processes within normal baseline") | color(Color::Green));
    } else {
        rows.push_back(hbox({
            text(pad(" PID",    8)) | bold | color(Color::Blue),
            text(pad("Score",   8)) | bold | color(Color::Blue),
            text("Description")     | bold | color(Color::Blue),
        }));
        rows.push_back(separator());
        int shown = 0;
        for (auto& a : anomalies) {
            if (shown++ > 10) break;
            double score = a.value("score", 0.0);
            Color  c = score > 10 ? Color::Red : Color::Yellow;
            rows.push_back(hbox({
                text(pad(" " + a.value("pid","?"), 8)) | color(Color::White),
                text(pad(fmt1(score), 8))              | color(c) | bold,
                text(a.value("desc",""))               | color(Color::GrayLight),
            }));
        }
    }
    return vbox(std::move(rows)) | border;
}

static Element render_file_decay(Storage& st, RingBuffer& rb) {
    nlohmann::json records = nlohmann::json::array();
    if (st.has("file_decay.records")) records = st.get("file_decay.records");

    std::vector<std::pair<double,std::string>> sorted;
    for (auto& r : records)
        sorted.push_back({r.value("score",1.0), r.value("path","")});
    std::sort(sorted.begin(), sorted.end());

    int dead=0, cold=0, warm=0, hot=0;
    for (auto& [s,p] : sorted) {
        if (s < 0.2) dead++; else if (s < 0.5) cold++;
        else if (s < 0.8) warm++; else hot++;
    }
    rb.push(static_cast<double>(dead));

    Elements rows;
    rows.push_back(hbox({
        text(" FILE DECAY ") | bold | color(Color::Black) | bgcolor(Color::Yellow),
        text("  filesystem usefulness scoring"),
    }));
    rows.push_back(separator());
    rows.push_back(hbox({
        text("  Dead: ")  | color(Color::GrayLight), text(fmt0(dead))  | color(Color::Red)    | bold,
        text("  Cold: ")  | color(Color::GrayLight), text(fmt0(cold))  | color(Color::Yellow) | bold,
        text("  Warm: ")  | color(Color::GrayLight), text(fmt0(warm))  | color(Color::Green)  | bold,
        text("  Hot: ")   | color(Color::GrayLight), text(fmt0(hot))   | color(Color::Cyan)   | bold,
        text("  Total: ") | color(Color::GrayLight), text(fmt0(records.size())),
    }));
    rows.push_back(separator());
    rows.push_back(hbox({
        text(" Dead ") | color(Color::GrayLight) | size(WIDTH, EQUAL, 6),
        sparkline_canvas(rb, 70, 4, Color::Red),
        text(" " + fmt0(rb.last())) | color(Color::Red) | bold | size(WIDTH, EQUAL, 8),
    }));
    rows.push_back(separator());
    rows.push_back(hbox({
        text(pad(" Score", 7))  | bold | color(Color::Blue),
        text(pad("Graph", 34))  | bold | color(Color::Blue),
        text("Path")             | bold | color(Color::Blue),
    }));
    rows.push_back(separator());
    int shown = 0;
    for (auto& [score, path] : sorted) {
        if (shown++ > 10) break;
        Color c = score < 0.2 ? Color::Red : score < 0.5 ? Color::Yellow :
                  score < 0.8 ? Color::Green : Color::Cyan;
        std::string sp = path.size() > 40 ? "..." + path.substr(path.size()-37) : path;
        rows.push_back(hbox({
            text(pad(fmt1(score), 7)) | color(c) | bold,
            hbar_grad(score * 100.0, 30),
            text("  " + sp) | color(Color::White),
        }));
    }
    if (records.empty())
        rows.push_back(text("  Watching... no files tracked yet") | color(Color::GrayDark));
    return vbox(std::move(rows)) | border;
}

static Element render_process_map(Storage& st, RingBuffer& rb_procs) {
    nlohmann::json graph;
    if (st.has("process_mapper.graph")) graph = st.get("process_mapper.graph");
    auto nodes = graph.value("nodes", nlohmann::json::array());
    auto edges = graph.value("edges", nlohmann::json::array());
    rb_procs.push(static_cast<double>(nodes.size()));

    Elements rows;
    rows.push_back(hbox({
        text(" PROCESS MAP ") | bold | color(Color::Black) | bgcolor(Color::Green),
        text("  IPC dependency graph"),
    }));
    rows.push_back(separator());
    rows.push_back(hbox({
        text("  Processes: ") | color(Color::GrayLight),
        text(fmt0(nodes.size())) | color(Color::Green) | bold,
        text("   Edges: ")    | color(Color::GrayLight),
        text(fmt0(edges.size())) | color(Color::Yellow) | bold,
    }));
    rows.push_back(separator());
    rows.push_back(hbox({
        text(" Procs") | color(Color::GrayLight) | size(WIDTH, EQUAL, 6),
        sparkline_canvas(rb_procs, 70, 4, Color::Green),
        text(" " + fmt0(rb_procs.last())) | color(Color::Green) | bold | size(WIDTH, EQUAL, 8),
    }));
    rows.push_back(separator());

    std::map<uint32_t,std::string> pid_name;
    for (auto& n : nodes) pid_name[n.value("pid",0u)] = n.value("name","?");

    rows.push_back(hbox({
        text(pad(" Parent", 20)) | bold | color(Color::Blue),
        text(pad("Link",    14)) | bold | color(Color::Blue),
        text("Child")            | bold | color(Color::Blue),
    }));
    rows.push_back(separator());
    int shown = 0;
    for (auto& e : edges) {
        if (shown++ > 12) break;
        uint32_t src = e.value("src", 0u);
        uint32_t dst = e.value("dst", 0u);
        std::string type = e.value("type","?");
        std::string sn = pid_name.count(src) ? pad(pid_name[src],16) : pad(std::to_string(src),16);
        std::string dn = pid_name.count(dst) ? pid_name[dst] : std::to_string(dst);
        Color tc = type=="pipe" ? Color::Cyan : type=="socket" ? Color::Yellow : Color::GrayLight;
        rows.push_back(hbox({
            text("  " + sn) | color(Color::White),
            text(" -[")     | color(Color::GrayDark),
            text(pad(type,6)) | color(tc) | bold,
            text("]-> ")    | color(Color::GrayDark),
            text(dn)        | color(Color::Green),
        }));
    }
    if (edges.empty())
        rows.push_back(text("  Scanning process tree...") | color(Color::GrayDark));
    return vbox(std::move(rows)) | border;
}

static Element render_state_diff(Storage& st, RingBuffer& rb) {
    nlohmann::json diffs = nlohmann::json::array();
    if (st.has("state_diff.latest")) diffs = st.get("state_diff.latest");
    rb.push(static_cast<double>(diffs.size()));

    int added=0, changed=0, removed=0;
    for (auto& d : diffs) {
        std::string op = d.value("op","");
        if (op=="added") added++; else if (op=="changed") changed++; else removed++;
    }

    Elements rows;
    rows.push_back(hbox({
        text(" STATE DIFF ") | bold | color(Color::Black) | bgcolor(Color::Magenta),
        text("  system snapshot delta tracker"),
    }));
    rows.push_back(separator());
    rows.push_back(hbox({
        text("  Added: ")   | color(Color::GrayLight), text(fmt0(added))   | color(Color::Green)  | bold,
        text("  Changed: ") | color(Color::GrayLight), text(fmt0(changed)) | color(Color::Yellow) | bold,
        text("  Removed: ") | color(Color::GrayLight), text(fmt0(removed)) | color(Color::Red)    | bold,
        text("  Total: ")   | color(Color::GrayLight), text(fmt0(diffs.size())),
    }));
    rows.push_back(separator());
    rows.push_back(hbox({
        text(" Delta") | color(Color::GrayLight) | size(WIDTH, EQUAL, 6),
        sparkline_canvas(rb, 70, 5, Color::Magenta),
        text(" " + fmt0(rb.last())) | color(Color::Magenta) | bold | size(WIDTH, EQUAL, 8),
    }));
    rows.push_back(separator());
    rows.push_back(hbox({
        text(pad(" Op",   10)) | bold | color(Color::Blue),
        text(pad("Key",   26)) | bold | color(Color::Blue),
        text("Value")           | bold | color(Color::Blue),
    }));
    rows.push_back(separator());
    int shown = 0;
    for (auto it = diffs.rbegin(); it != diffs.rend() && shown < 10; ++it, ++shown) {
        auto& d = *it;
        std::string op = d.value("op","?");
        Color c = op=="added" ? Color::Green : op=="removed" ? Color::Red : Color::Yellow;
        std::string after = d.value("after", nlohmann::json{}).dump();
        if (after.size() > 32) after = after.substr(0,29) + "...";
        rows.push_back(hbox({
            text(pad(" ["+op+"]", 10)) | color(c) | bold,
            text(pad(d.value("key",""), 26)) | color(Color::White),
            text(after) | color(Color::GrayLight),
        }));
    }
    if (diffs.empty())
        rows.push_back(text("  Waiting for first snapshot...") | color(Color::GrayDark));
    return vbox(std::move(rows)) | border;
}

#endif // SIP_ENABLE_FTXUI

// ─────────────────────────────────────────────────────────────────────────────
// Dashboard::Impl
// ─────────────────────────────────────────────────────────────────────────────
struct Dashboard::Impl {
    Engine&                      engine;
    std::shared_ptr<MemSink>     log_sink;
    std::atomic<bool>            running{false};
    int                          tab_index{0};
#ifdef SIP_ENABLE_FTXUI
    ScreenInteractive            screen{ScreenInteractive::Fullscreen()};
    RingBuffer rb_thermal{80}, rb_throttle{80}, rb_behavior{80};
    RingBuffer rb_decay{80},   rb_procs{80},    rb_state_diff{80};
#endif
    explicit Impl(Engine& e, std::shared_ptr<MemSink> sink)
        : engine(e), log_sink(std::move(sink)) {}
};

Dashboard::Dashboard(Engine& engine) : impl_(nullptr) {
    // Install in-memory sink — replaces stderr so logs don't bleed into TUI
    auto sink = std::make_shared<MemSink>(120);
    spdlog::default_logger()->sinks().clear();
    spdlog::default_logger()->sinks().push_back(sink);
    spdlog::set_level(spdlog::level::debug);
    impl_.reset(new Impl(engine, sink));
}

Dashboard::~Dashboard() { stop(); }

void Dashboard::run() {
#ifdef SIP_ENABLE_FTXUI
    impl_->running = true;
    auto& st   = impl_->engine.storage();
    auto& sink = impl_->log_sink;

    static const std::vector<std::string> tab_names = {
        "Thermal", "BehaviorDNA", "FileDecay", "ProcessMap", "StateDiff"
    };

    // Refresh ticker
    std::thread ticker([&]{
        while (impl_->running.load()) {
            impl_->screen.PostEvent(Event::Custom);
            std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        }
    });

    // Clickable mouse tab buttons
    std::vector<Component> tab_btns;
    for (int i = 0; i < 5; ++i) {
        tab_btns.push_back(Button(tab_names[i], [&, i]{ impl_->tab_index = i; },
                                  ButtonOption::Ascii()));
    }
    auto tab_bar_comp = Container::Horizontal(tab_btns);
    auto tab_toggle   = Container::Tab({tab_bar_comp}, &impl_->tab_index);

    auto renderer = Renderer(tab_toggle, [&]() -> Element {
        // ── Main content ──────────────────────────────────────────────────────
        Element content;
        switch (impl_->tab_index) {
            case 0: content = render_thermal   (st, impl_->rb_thermal, impl_->rb_throttle); break;
            case 1: content = render_behavior  (st, impl_->rb_behavior);  break;
            case 2: content = render_file_decay(st, impl_->rb_decay);     break;
            case 3: content = render_process_map(st, impl_->rb_procs);    break;
            case 4: content = render_state_diff(st, impl_->rb_state_diff);break;
            default: content = text("?");
        }

        // ── Tab bar — render buttons with active highlight ────────────────────
        Elements tab_els;
        for (int i = 0; i < (int)tab_names.size(); ++i) {
            bool active = (i == impl_->tab_index);
            auto btn_el = tab_btns[i]->Render();
            tab_els.push_back(
                btn_el | (active
                    ? (bold | color(Color::Black) | bgcolor(Color::White))
                    : color(Color::GrayLight)));
            if (i < (int)tab_names.size() - 1)
                tab_els.push_back(text("|") | color(Color::GrayDark));
        }

        // Warn badge: count warnings since last render
        auto entries = sink->tail(120);
        int warn_count = 0;
        for (auto& e : entries)
            if (e.level >= spdlog::level::warn) warn_count++;

        return vbox({
            // Status + tab bar row
            hbox({
                hbox(std::move(tab_els)),
                filler(),
                warn_count > 0
                    ? (text(" ! " + fmt0(warn_count) + " warn ") | bold | color(Color::Black) | bgcolor(Color::Red))
                    : (text(" OK ") | color(Color::Black) | bgcolor(Color::Green)),
                text(" [1-5] [</> h/l] [q] ") | color(Color::GrayDark),
            }),
            separator(),
            // Main tab content — flex so it takes all remaining space
            content | flex,
            // ── Persistent log strip ─────────────────────────────────────────
            render_log_strip(sink),
        });
    });

    auto handler = CatchEvent(renderer, [&](Event ev) -> bool {
        if (ev == Event::Character('q') || ev == Event::Character('Q')) {
            impl_->screen.ExitLoopClosure()(); return true;
        }
        if (ev == Event::ArrowRight || ev == Event::Character('\t') || ev == Event::Character('l')) {
            impl_->tab_index = (impl_->tab_index + 1) % 5; return true;
        }
        if (ev == Event::ArrowLeft || ev == Event::Character('h')) {
            impl_->tab_index = (impl_->tab_index + 4) % 5; return true;
        }
        for (int i = 0; i < 5; ++i) {
            if (ev == Event::Character(std::string(1, '1' + i))) {
                impl_->tab_index = i; return true;
            }
        }
        return false;
    });

    impl_->screen.Loop(handler);
    impl_->running = false;
    ticker.join();

    // Restore stderr logger for shutdown messages
    auto stderr_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    spdlog::default_logger()->sinks().push_back(stderr_sink);
#else
    spdlog::warn("[Dashboard] built without ftxui");
#endif
}

void Dashboard::stop() {
    if (!impl_) return;
#ifdef SIP_ENABLE_FTXUI
    impl_->running = false;
    impl_->screen.Exit();
#endif
}

} // namespace sip
