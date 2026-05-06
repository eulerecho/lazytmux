#include "ui_vertical_slice.hpp"
#include <ftxui/component/event.hpp>
#include <gtest/gtest.h>
#include <string>

namespace lazytmux::examples::ui_vertical_slice {
namespace {

TEST(UiVerticalSliceTest, RendersMinimumLayoutWithFakeSessions) {
    auto state = demo_state();

    const std::string rendered = render_to_string(state, kMinWidth, kMinHeight);

    EXPECT_NE(rendered.find("lazytmux UI vertical slice"), std::string::npos);
    EXPECT_NE(rendered.find("Sessions"), std::string::npos);
    EXPECT_NE(rendered.find("Details"), std::string::npos);
    EXPECT_NE(rendered.find("dev"), std::string::npos);
    EXPECT_NE(rendered.find("attached"), std::string::npos);
}

TEST(UiVerticalSliceTest, RendersTooSmallScreenBeforeNormalLayout) {
    auto state = demo_state();

    const std::string rendered = render_to_string(state, kMinWidth - 1, kMinHeight);

    EXPECT_NE(rendered.find("Terminal too small"), std::string::npos);
    EXPECT_NE(rendered.find("Minimum size: 100x30"), std::string::npos);
    EXPECT_EQ(rendered.find("Sessions"), std::string::npos);
}

TEST(UiVerticalSliceTest, RoutesMovementHelpFocusAndQuitKeys) {
    auto state = demo_state();

    EXPECT_TRUE(handle_event(state, ftxui::Event::ArrowDown));
    EXPECT_EQ(state.selected, 1U);
    EXPECT_TRUE(handle_event(state, ftxui::Event::Character("k")));
    EXPECT_EQ(state.selected, 0U);

    EXPECT_TRUE(handle_event(state, ftxui::Event::Tab));
    EXPECT_EQ(state.focus, FocusTarget::kDetails);
    EXPECT_TRUE(handle_event(state, ftxui::Event::Character("?")));
    EXPECT_TRUE(state.help_open);

    EXPECT_TRUE(handle_event(state, ftxui::Event::Character("q")));
    EXPECT_TRUE(state.quit_requested);
}

TEST(UiVerticalSliceTest, LeavesUnknownKeysUnhandled) {
    auto state = demo_state();

    EXPECT_FALSE(handle_event(state, ftxui::Event::Character("x")));
    EXPECT_FALSE(state.quit_requested);
}

}  // namespace
}  // namespace lazytmux::examples::ui_vertical_slice
