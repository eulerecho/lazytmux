#include <lazytmux/config.hpp>

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <unistd.h>

namespace lazytmux::config {
namespace {

class TempDir {
public:
    TempDir() {
        path_ = std::filesystem::temp_directory_path() /
                ("lazytmux-config-test-" + std::to_string(::getpid()));
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    ~TempDir() {
        std::filesystem::remove_all(path_);
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

tmux::TemplateName template_name(std::string_view s) {
    auto name = tmux::TemplateName::make(s);
    return std::move(name).value();
}

TEST(ConfigParseTest, EmptyTomlYieldsDefaults) {
    auto config = parse_config("");
    ASSERT_TRUE(config.has_value()) << config.error().display();
    EXPECT_EQ(config->theme.focus_border, (Rgb{0xfa, 0xbd, 0x2f}));
    EXPECT_TRUE(config->templates.map.empty());
}

TEST(ConfigParseTest, UnknownTopLevelFieldErrors) {
    auto config = parse_config("nonsense = true\n");
    ASSERT_FALSE(config.has_value());
    EXPECT_NE(config.error().message().find("unknown"), std::string_view::npos);
}

TEST(ConfigParseTest, ThemeCanOverrideOneColorAndBorderStyle) {
    auto config = parse_config(
        "[theme]\n"
        "focus_border = \"#ff00ff\"\n"
        "border_style = \"rounded\"\n");
    ASSERT_TRUE(config.has_value()) << config.error().display();
    EXPECT_EQ(config->theme.focus_border, (Rgb{0xff, 0x00, 0xff}));
    EXPECT_EQ(config->theme.accent_error, (Rgb{0xfb, 0x49, 0x34}));
    EXPECT_EQ(config->theme.border_style, BorderStyle::kRounded);
}

TEST(ConfigParseTest, MalformedThemeColorErrors) {
    auto config = parse_config("[theme]\nfocus_border = \"not-a-color\"\n");
    ASSERT_FALSE(config.has_value());
    EXPECT_NE(config.error().message().find("hex"), std::string_view::npos);
}

TEST(ConfigParseTest, TemplatesSectionParsesValidTemplates) {
    auto config = parse_config(
        "[templates.dev]\n"
        "windows = [\n"
        "  { name = \"editor\", command = \"nvim\" },\n"
        "  { name = \"shell\" },\n"
        "]\n");
    ASSERT_TRUE(config.has_value()) << config.error().display();
    ASSERT_EQ(config->templates.map.size(), 1u);
    ASSERT_TRUE(config->templates.errors.empty());
    auto it = config->templates.map.find(template_name("dev"));
    ASSERT_NE(it, config->templates.map.end());
    ASSERT_EQ(it->second.windows.size(), 2u);
    EXPECT_EQ(it->second.windows[0].command, "nvim");
    EXPECT_EQ(it->second.windows[1].command, "");
}

TEST(ConfigParseTest, BadTemplateIsIsolatedFromGoodOne) {
    auto config = parse_config(
        "[templates.good]\n"
        "windows = [{ name = \"editor\" }]\n"
        "[templates.bad]\n"
        "windows = \"not an array\"\n");
    ASSERT_TRUE(config.has_value()) << config.error().display();
    EXPECT_EQ(config->templates.map.size(), 1u);
    ASSERT_EQ(config->templates.errors.size(), 1u);
    EXPECT_NE(config->templates.errors[0].find("bad"), std::string::npos);
}

TEST(ConfigPathTest, ResolvesXdgBeforeHome) {
    auto path = resolve_config_path("/xdg", "/home/u");
    ASSERT_TRUE(path.has_value());
    EXPECT_EQ(*path, std::filesystem::path("/xdg/lazytmux/config.toml"));
}

TEST(ConfigPathTest, FallsBackToHome) {
    auto path = resolve_config_path(std::nullopt, "/home/u");
    ASSERT_TRUE(path.has_value());
    EXPECT_EQ(*path, std::filesystem::path("/home/u/.config/lazytmux/config.toml"));
}

TEST(ConfigLoadTest, MissingFileLoadsDefaultsWithoutWarning) {
    TempDir tmp;
    auto loaded = load_config_from_path(tmp.path() / "missing.toml");
    EXPECT_FALSE(loaded.warning.has_value());
    EXPECT_TRUE(loaded.config.templates.map.empty());
}

TEST(ConfigLoadTest, MalformedFileLoadsDefaultsWithWarning) {
    TempDir tmp;
    const auto path = tmp.path() / "config.toml";
    std::ofstream out(path);
    out << "[theme\n";
    out.close();

    auto loaded = load_config_from_path(path);
    ASSERT_TRUE(loaded.warning.has_value());
    EXPECT_NE(loaded.warning->find("using defaults"), std::string::npos);
}

TEST(ConfigLoadTest, TemplateErrorsBecomeWarning) {
    TempDir tmp;
    const auto path = tmp.path() / "config.toml";
    std::ofstream out(path);
    out << "[templates.bad]\nwindows = []\n";
    out.close();

    auto loaded = load_config_from_path(path);
    ASSERT_TRUE(loaded.warning.has_value());
    EXPECT_NE(loaded.warning->find("at least one window"), std::string::npos);
}

}  // namespace
}  // namespace lazytmux::config
