#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>
#include <string>

TEST(FtxuiSmokeTest, RendersTextElementToScreen) {
    auto document = ftxui::text("lazytmux") | ftxui::border;
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(20), ftxui::Dimension::Fixed(3));

    ftxui::Render(screen, document);

    const std::string rendered = screen.ToString();
    EXPECT_NE(rendered.find("lazytmux"), std::string::npos);
}
