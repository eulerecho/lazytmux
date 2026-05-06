#pragma once

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace lazytmux::examples::ui_vertical_slice {

inline constexpr int kMinWidth = 100;
inline constexpr int kMinHeight = 30;

struct SessionRow {
    std::string name;
    int windows{};
    bool attached{};
};

enum class FocusTarget {
    kSessions,
    kDetails,
};

struct State {
    std::vector<SessionRow> sessions;
    std::size_t selected{};
    FocusTarget focus{FocusTarget::kSessions};
    bool help_open{};
    bool quit_requested{};
};

inline State demo_state() {
    return State{
        .sessions =
            {
                SessionRow{.name = "dev", .windows = 4, .attached = true},
                SessionRow{.name = "ops", .windows = 2, .attached = false},
                SessionRow{.name = "notes", .windows = 3, .attached = false},
                SessionRow{.name = "scratch", .windows = 1, .attached = false},
            },
    };
}

inline std::string focus_label(FocusTarget focus) {
    switch (focus) {
        case FocusTarget::kSessions:
            return "sessions";
        case FocusTarget::kDetails:
            return "details";
    }
    return "unknown";
}

inline void move_selection(State& state, int delta) {
    if (state.sessions.empty()) {
        return;
    }
    const auto size = static_cast<int>(state.sessions.size());
    const auto current = static_cast<int>(state.selected);
    state.selected = static_cast<std::size_t>((current + delta + size) % size);
}

inline bool handle_event(State& state, const ftxui::Event& event) {
    if (event == ftxui::Event::Character("q") || event == ftxui::Event::Escape) {
        state.quit_requested = true;
        return true;
    }
    if (event == ftxui::Event::Character("?")) {
        state.help_open = !state.help_open;
        return true;
    }
    if (event == ftxui::Event::Tab) {
        state.focus =
            state.focus == FocusTarget::kSessions ? FocusTarget::kDetails : FocusTarget::kSessions;
        return true;
    }
    if (event == ftxui::Event::ArrowDown || event == ftxui::Event::Character("j")) {
        move_selection(state, 1);
        return true;
    }
    if (event == ftxui::Event::ArrowUp || event == ftxui::Event::Character("k")) {
        move_selection(state, -1);
        return true;
    }
    return false;
}

inline ftxui::Element terminal_too_small(int width, int height) {
    return ftxui::vbox({
               ftxui::text("Terminal too small") | ftxui::bold,
               ftxui::text("Minimum size: 100x30"),
               ftxui::text("Current size: " + std::to_string(width) + "x" +
                           std::to_string(height)),
           }) |
           ftxui::center;
}

inline ftxui::Element panel_frame(std::string_view title,
                                  FocusTarget focus,
                                  FocusTarget self,
                                  ftxui::Element body) {
    auto framed = ftxui::vbox({
        ftxui::text(std::string{title}) | ftxui::bold,
        ftxui::separator(),
        std::move(body) | ftxui::flex,
    });
    if (focus == self) {
        return framed | ftxui::borderDouble;
    }
    return framed | ftxui::border;
}

inline ftxui::Element sessions_panel(const State& state) {
    std::vector<ftxui::Element> rows;
    rows.reserve(state.sessions.size());
    for (std::size_t i = 0; i < state.sessions.size(); ++i) {
        const auto& session = state.sessions[i];
        std::string line = "  " + session.name + "  " + std::to_string(session.windows) + "w";
        if (session.attached) {
            line += "  attached";
        }
        auto row = ftxui::text(line);
        if (i == state.selected) {
            line.replace(0, 2, "> ");
            row = ftxui::text(line) | ftxui::inverted;
        }
        rows.push_back(std::move(row));
    }
    return ftxui::vbox(std::move(rows));
}

inline ftxui::Element details_panel(const State& state) {
    if (state.sessions.empty()) {
        return ftxui::text("No sessions");
    }
    const auto& session = state.sessions[state.selected];
    return ftxui::vbox({
        ftxui::text("Selected: " + session.name),
        ftxui::text("Windows: " + std::to_string(session.windows)),
        ftxui::text(session.attached ? "Attached: yes" : "Attached: no"),
        ftxui::separator(),
        ftxui::text("Focus: " + focus_label(state.focus)),
    });
}

inline ftxui::Element help_panel() {
    return ftxui::vbox({
               ftxui::text("Help") | ftxui::bold,
               ftxui::separator(),
               ftxui::text("Tab: move focus"),
               ftxui::text("j/k or arrows: move selection"),
               ftxui::text("?: toggle help"),
               ftxui::text("q: quit"),
           }) |
           ftxui::border;
}

inline ftxui::Element render(const State& state, int width, int height) {
    if (width < kMinWidth || height < kMinHeight) {
        return terminal_too_small(width, height);
    }

    auto body = ftxui::hbox({
        panel_frame("Sessions", state.focus, FocusTarget::kSessions, sessions_panel(state)) |
            ftxui::flex,
        panel_frame("Details", state.focus, FocusTarget::kDetails, details_panel(state)) |
            ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 34),
    });

    std::vector<ftxui::Element> sections{
        ftxui::hbox({
            ftxui::text("lazytmux UI vertical slice") | ftxui::bold,
            ftxui::filler(),
            ftxui::text("Tab focus  ? help  q quit"),
        }),
        ftxui::separator(),
        std::move(body) | ftxui::flex,
    };
    if (state.help_open) {
        sections.push_back(help_panel());
    }
    return ftxui::vbox(std::move(sections));
}

inline std::string render_to_string(const State& state, int width, int height) {
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(width),
                                        ftxui::Dimension::Fixed(height));
    ftxui::Render(screen, render(state, width, height));
    return screen.ToString();
}

}  // namespace lazytmux::examples::ui_vertical_slice
