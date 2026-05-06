#include "ui_vertical_slice.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/screen/terminal.hpp>

int main() {
    namespace slice = lazytmux::examples::ui_vertical_slice;

    auto state = slice::demo_state();
    auto screen = ftxui::ScreenInteractive::Fullscreen();

    auto component = ftxui::Renderer([&] {
        const auto size = ftxui::Terminal::Size();
        return slice::render(state, size.dimx, size.dimy);
    });
    component = ftxui::CatchEvent(component, [&](const ftxui::Event& event) {
        if (!slice::handle_event(state, event)) {
            return false;
        }
        if (state.quit_requested) {
            screen.ExitLoopClosure()();
        }
        return true;
    });

    screen.Loop(component);
    return 0;
}
