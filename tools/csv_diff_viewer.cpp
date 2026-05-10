
// sip_csv_diff_viewer — reads the CSV produced by StateDiff::export_csv()
// and renders a colour-coded summary in the terminal using ftxui.
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <map>

#ifdef SIP_ENABLE_FTXUI
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
using namespace ftxui;
#endif

struct DiffRow {
    std::string key;
    std::string op;
    std::string before;
    std::string after;
};

static std::vector<DiffRow> load_csv(const std::string& path) {
    std::vector<DiffRow> rows;
    std::ifstream f(path);
    if (!f) { std::cerr << "Cannot open: " << path << "\n"; return rows; }
    std::string line;
    std::getline(f, line); // header
    while (std::getline(f, line)) {
        std::istringstream ss(line);
        DiffRow r;
        std::getline(ss, r.key,    ',');
        std::getline(ss, r.op,     ',');
        std::getline(ss, r.before, ',');
        std::getline(ss, r.after);
        rows.push_back(r);
    }
    return rows;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: sip_csv_diff_viewer <diff.csv>\n";
        return 1;
    }
    auto rows = load_csv(argv[1]);
    if (rows.empty()) { std::cerr << "No data\n"; return 1; }

#ifdef SIP_ENABLE_FTXUI
    // Count operations for summary
    std::map<std::string,int> op_count;
    for (auto& r : rows) op_count[r.op]++;

    int selected = 0;
    auto screen = ScreenInteractive::Fullscreen();

    auto render = [&]() -> Element {
        Elements header_row = {
            text("Op      ") | bold | color(Color::Blue)   | size(WIDTH, EQUAL, 10),
            text("Key                     ") | bold | color(Color::Blue) | size(WIDTH, EQUAL, 26),
            text("Before          ") | bold | color(Color::Blue) | size(WIDTH, EQUAL, 18),
            text("After") | bold | color(Color::Blue),
        };

        Elements data_rows;
        int i = 0;
        for (auto& r : rows) {
            Color c = r.op == "added"   ? Color::Green :
                      r.op == "removed" ? Color::Red   : Color::Yellow;
            bool sel = (i++ == selected);
            auto row_el = hbox({
                text("[" + r.op + "]") | color(c)          | size(WIDTH, EQUAL, 10),
                text(r.key.substr(0, 24))                   | size(WIDTH, EQUAL, 26),
                text(r.before.substr(0, 16)) | color(Color::GrayLight) | size(WIDTH, EQUAL, 18),
                text(r.after.substr(0, 30))  | color(Color::White),
            });
            data_rows.push_back(sel ? (row_el | inverted) : row_el);
        }

        return vbox({
            text(" SIP CSV Diff Viewer — " + std::string(argv[1])) | bold | color(Color::Cyan),
            separator(),
            hbox({
                text("added: " + std::to_string(op_count["added"]))   | color(Color::Green),
                text("  changed: " + std::to_string(op_count["changed"])) | color(Color::Yellow),
                text("  removed: " + std::to_string(op_count["removed"])) | color(Color::Red),
                text("  total: "  + std::to_string(rows.size())),
            }),
            separator(),
            hbox(header_row),
            separator(),
            vbox(std::move(data_rows)) | vscroll_indicator | frame | flex,
            separator(),
            text(" [↑↓] Navigate   [q] Quit") | color(Color::GrayLight),
        });
    };

    auto component = Renderer([&]{ return render(); });
    auto handler = CatchEvent(component, [&](Event ev) {
        if (ev == Event::Character('q')) { screen.ExitLoopClosure()(); return true; }
        if (ev == Event::ArrowDown) { selected = std::min(selected+1,(int)rows.size()-1); return true; }
        if (ev == Event::ArrowUp)   { selected = std::max(selected-1, 0);                 return true; }
        return false;
    });
    screen.Loop(handler);

#else
    // Plain terminal fallback
    std::cout << "key,op,before,after\n";
    for (auto& r : rows)
        std::cout << r.key << "  [" << r.op << "]  " << r.before << " -> " << r.after << "\n";
#endif
    return 0;
}
