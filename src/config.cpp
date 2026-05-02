#include <lazytmux/config.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <toml++/toml.hpp>
#include <utility>

namespace lazytmux::config {
namespace {

constexpr std::array<std::string_view, 11> kThemeColorKeys{
    "focus_border",
    "focus_title",
    "status_text",
    "accent_info",
    "accent_warn",
    "accent_error",
    "selected_bg",
    "prompt_accent",
    "header_text",
    "border_accent",
    "body_text",
};

std::optional<std::string_view> getenv_view(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr) {
        return std::nullopt;
    }
    return std::string_view{value};
}

bool key_is(std::string_view key, std::string_view expected) {
    return key == expected;
}

std::string key_string(const toml::key& key) {
    return std::string{key.str()};
}

Result<std::string> require_string(const toml::node& node, std::string_view field) {
    auto value = node.value<std::string>();
    if (!value) {
        return std::unexpected(Error(std::format("config field {} must be a string", field)));
    }
    return *value;
}

Result<Rgb> parse_color_node(const toml::node& node, std::string_view field) {
    auto raw = require_string(node, field);
    if (!raw) {
        return std::unexpected(std::move(raw).error());
    }
    auto color = parse_hex(*raw);
    if (!color) {
        return std::unexpected(Error(std::format("invalid hex color for {}: \"{}\"", field, *raw)));
    }
    return *color;
}

Result<BorderStyle> parse_border_style(const toml::node& node) {
    auto raw = require_string(node, "theme.border_style");
    if (!raw) {
        return std::unexpected(std::move(raw).error());
    }
    if (*raw == "plain") {
        return BorderStyle::kPlain;
    }
    if (*raw == "rounded") {
        return BorderStyle::kRounded;
    }
    if (*raw == "double") {
        return BorderStyle::kDouble;
    }
    if (*raw == "thick") {
        return BorderStyle::kThick;
    }
    if (*raw == "quadrant_inside") {
        return BorderStyle::kQuadrantInside;
    }
    if (*raw == "quadrant_outside") {
        return BorderStyle::kQuadrantOutside;
    }
    return std::unexpected(Error(std::format("unknown border_style: \"{}\"", *raw)));
}

bool is_theme_color_key(std::string_view key) {
    return std::ranges::find(kThemeColorKeys, key) != kThemeColorKeys.end();
}

Result<void> assign_color(Rgb& target, const toml::node& value, std::string_view field) {
    auto color = parse_color_node(value, field);
    if (!color) {
        return std::unexpected(std::move(color).error());
    }
    target = *color;
    return {};
}

Result<Theme> parse_theme(const toml::node& node) {
    const auto* table = node.as_table();
    if (table == nullptr) {
        return std::unexpected(Error("theme must be a table"));
    }

    Theme theme;
    for (const auto& [key, value] : *table) {
        const auto name = key.str();
        if (!is_theme_color_key(name) && !key_is(name, "border_style")) {
            return std::unexpected(Error(std::format("unknown theme field: {}", name)));
        }

        Result<void> assigned{};
        if (key_is(name, "focus_border")) {
            assigned = assign_color(theme.focus_border, value, "theme.focus_border");
        } else if (key_is(name, "focus_title")) {
            assigned = assign_color(theme.focus_title, value, "theme.focus_title");
        } else if (key_is(name, "status_text")) {
            assigned = assign_color(theme.status_text, value, "theme.status_text");
        } else if (key_is(name, "accent_info")) {
            assigned = assign_color(theme.accent_info, value, "theme.accent_info");
        } else if (key_is(name, "accent_warn")) {
            assigned = assign_color(theme.accent_warn, value, "theme.accent_warn");
        } else if (key_is(name, "accent_error")) {
            assigned = assign_color(theme.accent_error, value, "theme.accent_error");
        } else if (key_is(name, "selected_bg")) {
            assigned = assign_color(theme.selected_bg, value, "theme.selected_bg");
        } else if (key_is(name, "prompt_accent")) {
            assigned = assign_color(theme.prompt_accent, value, "theme.prompt_accent");
        } else if (key_is(name, "header_text")) {
            assigned = assign_color(theme.header_text, value, "theme.header_text");
        } else if (key_is(name, "border_accent")) {
            assigned = assign_color(theme.border_accent, value, "theme.border_accent");
        } else if (key_is(name, "body_text")) {
            assigned = assign_color(theme.body_text, value, "theme.body_text");
        } else if (key_is(name, "border_style")) {
            auto border_style = parse_border_style(value);
            if (!border_style) {
                return std::unexpected(std::move(border_style).error());
            }
            theme.border_style = *border_style;
        }
        if (!assigned) {
            return std::unexpected(std::move(assigned).error());
        }
    }
    return theme;
}

Result<TemplateWindow> parse_window(const toml::node& node) {
    const auto* table = node.as_table();
    if (table == nullptr) {
        return std::unexpected(Error("template window must be a table"));
    }

    TemplateWindow window;
    bool has_name = false;
    for (const auto& [key, value] : *table) {
        const auto name = key.str();
        if (key_is(name, "name")) {
            auto parsed = require_string(value, "window.name");
            if (!parsed) {
                return std::unexpected(std::move(parsed).error());
            }
            window.name = *parsed;
            has_name = true;
        } else if (key_is(name, "command")) {
            auto parsed = require_string(value, "window.command");
            if (!parsed) {
                return std::unexpected(std::move(parsed).error());
            }
            window.command = *parsed;
        } else {
            return std::unexpected(Error(std::format("unknown window field: {}", name)));
        }
    }
    if (!has_name) {
        return std::unexpected(Error("template window missing name"));
    }
    return window;
}

Result<Template> parse_template(const toml::node& node) {
    const auto* table = node.as_table();
    if (table == nullptr) {
        return std::unexpected(Error("template must be a table"));
    }

    const toml::array* windows = nullptr;
    for (const auto& [key, value] : *table) {
        const auto name = key.str();
        if (key_is(name, "windows")) {
            windows = value.as_array();
            if (windows == nullptr) {
                return std::unexpected(Error("template windows must be an array"));
            }
        } else {
            return std::unexpected(Error(std::format("unknown template field: {}", name)));
        }
    }
    if (windows == nullptr) {
        return std::unexpected(Error("template missing windows"));
    }
    if (windows->empty()) {
        return std::unexpected(Error("template must have at least one window"));
    }

    Template out;
    for (const auto& value : *windows) {
        auto window = parse_window(value);
        if (!window) {
            return std::unexpected(std::move(window).error());
        }
        out.windows.push_back(std::move(*window));
    }
    return out;
}

Result<Templates> parse_templates(const toml::node& node) {
    const auto* table = node.as_table();
    if (table == nullptr) {
        return std::unexpected(Error("templates must be a table"));
    }

    Templates out;
    for (const auto& [key, value] : *table) {
        const auto name_text = key_string(key);
        auto parsed = parse_template(value);
        if (!parsed) {
            out.errors.push_back(
                std::format("template '{}': {}", name_text, parsed.error().message()));
            continue;
        }
        auto name = tmux::TemplateName::make(name_text);
        if (!name) {
            out.errors.push_back(
                std::format("template '{}': {}", name_text, name.error().message()));
            continue;
        }
        out.map.emplace(std::move(*name), std::move(*parsed));
    }
    return out;
}

Result<std::string> read_file(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        return std::unexpected(Error(std::format("failed to open {}", path.string())));
    }
    std::ostringstream out;
    out << in.rdbuf();
    if (in.bad()) {
        return std::unexpected(Error(std::format("failed to read {}", path.string())));
    }
    return out.str();
}

std::optional<std::string> warning_for_templates(const std::filesystem::path& path,
                                                 const Config& config) {
    if (config.templates.errors.empty()) {
        return std::nullopt;
    }
    std::string joined;
    for (std::size_t i = 0; i < config.templates.errors.size(); ++i) {
        if (i != 0) {
            joined += "; ";
        }
        joined += config.templates.errors[i];
    }
    return std::format("config {}: {}", path.string(), joined);
}

}  // namespace

std::optional<std::filesystem::path> resolve_config_path(
    std::optional<std::string_view> xdg_config_home, std::optional<std::string_view> home) {
    if (xdg_config_home && !xdg_config_home->empty()) {
        return std::filesystem::path{std::string{*xdg_config_home}} / "lazytmux" / "config.toml";
    }
    if (home && !home->empty()) {
        return std::filesystem::path{std::string{*home}} / ".config" / "lazytmux" / "config.toml";
    }
    return std::nullopt;
}

std::optional<std::filesystem::path> default_config_path() {
    return resolve_config_path(getenv_view("XDG_CONFIG_HOME"), getenv_view("HOME"));
}

Result<Config> parse_config(std::string_view text) {
    toml::table table;
    try {
        table = toml::parse(text);
    } catch (const toml::parse_error& e) {
        return std::unexpected(
            Error(std::format("failed to parse config TOML: {}", e.description())));
    }

    Config config;
    for (const auto& [key, value] : table) {
        const auto name = key.str();
        if (key_is(name, "theme")) {
            auto theme = parse_theme(value);
            if (!theme) {
                return std::unexpected(std::move(theme).error());
            }
            config.theme = *theme;
        } else if (key_is(name, "templates")) {
            auto templates = parse_templates(value);
            if (!templates) {
                return std::unexpected(std::move(templates).error());
            }
            config.templates = std::move(*templates);
        } else {
            return std::unexpected(Error(std::format("unknown config field: {}", name)));
        }
    }
    return config;
}

LoadResult load_config_from_path(const std::filesystem::path& path) {
    std::error_code exists_ec;
    if (!std::filesystem::exists(path, exists_ec) || exists_ec) {
        return LoadResult{.config = Config{}, .warning = std::nullopt};
    }

    auto text = read_file(path);
    if (!text) {
        return LoadResult{
            .config = Config{},
            .warning =
                std::format("config {}: {}; using defaults", path.string(), text.error().message()),
        };
    }

    auto config = parse_config(*text);
    if (!config) {
        return LoadResult{
            .config = Config{},
            .warning = std::format(
                "config {}: {}; using defaults", path.string(), config.error().message()),
        };
    }
    return LoadResult{
        .config = *config,
        .warning = warning_for_templates(path, *config),
    };
}

LoadResult load_config() {
    auto path = default_config_path();
    if (!path) {
        return LoadResult{.config = Config{}, .warning = std::nullopt};
    }
    return load_config_from_path(*path);
}

}  // namespace lazytmux::config
