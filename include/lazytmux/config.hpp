#pragma once

#include <lazytmux/colors.hpp>
#include <lazytmux/error.hpp>
#include <lazytmux/tmux/ids.hpp>

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// @file
/// @brief User configuration schema and TOML loading helpers.

namespace lazytmux::config {

/// @brief Border style requested by the TOML config.
enum class BorderStyle {
    /// @brief Plain square border.
    kPlain,
    /// @brief Rounded border.
    kRounded,
    /// @brief Double-line border.
    kDouble,
    /// @brief Thick border.
    kThick,
    /// @brief Quadrant-inside border.
    kQuadrantInside,
    /// @brief Quadrant-outside border.
    kQuadrantOutside,
};

/// @brief User-overridable color and border settings.
struct Theme {
    /// @brief Focused panel border color.
    Rgb focus_border{0xfa, 0xbd, 0x2f};
    /// @brief Focused panel title color.
    Rgb focus_title{0x83, 0xa5, 0x98};
    /// @brief Status line text color.
    Rgb status_text{0x92, 0x83, 0x74};
    /// @brief Informational accent color.
    Rgb accent_info{0xb8, 0xbb, 0x26};
    /// @brief Warning accent color.
    Rgb accent_warn{0xfa, 0xbd, 0x2f};
    /// @brief Error accent color.
    Rgb accent_error{0xfb, 0x49, 0x34};
    /// @brief Selected row background color.
    Rgb selected_bg{0x50, 0x49, 0x45};
    /// @brief Prompt accent color.
    Rgb prompt_accent{0xfe, 0x80, 0x19};
    /// @brief Header text color.
    Rgb header_text{0x83, 0xa5, 0x98};
    /// @brief Unfocused border accent color.
    Rgb border_accent{0x83, 0xa5, 0x98};
    /// @brief Body text color.
    Rgb body_text{0xeb, 0xdb, 0xb2};
    /// @brief Border style used by framed UI components.
    BorderStyle border_style{BorderStyle::kPlain};

    auto operator<=>(const Theme&) const = default;
    bool operator==(const Theme&) const = default;
};

/// @brief One window in a session template.
struct TemplateWindow {
    /// @brief Window name.
    std::string name;
    /// @brief Initial command. Empty means the user's shell.
    std::string command;

    auto operator<=>(const TemplateWindow&) const = default;
    bool operator==(const TemplateWindow&) const = default;
};

/// @brief A named session template definition.
struct Template {
    /// @brief Windows to create for the template.
    std::vector<TemplateWindow> windows;

    auto operator<=>(const Template&) const = default;
    bool operator==(const Template&) const = default;
};

/// @brief Parsed templates plus isolated per-template errors.
struct Templates {
    /// @brief Valid templates keyed by template name.
    std::map<tmux::TemplateName, Template> map;
    /// @brief Malformed template messages that did not abort
    ///        wider config parsing.
    std::vector<std::string> errors;

    auto operator<=>(const Templates&) const = default;
    bool operator==(const Templates&) const = default;
};

/// @brief Top-level user configuration.
struct Config {
    /// @brief Theme settings.
    Theme theme;
    /// @brief Session templates.
    Templates templates;

    auto operator<=>(const Config&) const = default;
    bool operator==(const Config&) const = default;
};

/// @brief Config load result plus optional warning.
struct LoadResult {
    /// @brief Parsed config or defaults.
    Config config;
    /// @brief Warning shown when a present config was malformed
    ///        or had isolated template errors.
    std::optional<std::string> warning;
    /// @brief True when an existing config could not be read or
    ///        parsed and defaults were returned instead.
    bool used_defaults_due_to_error{false};
};

/// @brief Resolve the config path from XDG-style environment
/// values.
/// @param xdg_config_home Optional `$XDG_CONFIG_HOME` value.
/// @param home Optional `$HOME` value.
/// @return `$XDG_CONFIG_HOME/lazytmux/config.toml` when
///         non-empty, otherwise `$HOME/.config/lazytmux/config.toml`.
/// @return `std::nullopt` when neither value is usable.
[[nodiscard]] std::optional<std::filesystem::path> resolve_config_path(
    std::optional<std::string_view> xdg_config_home, std::optional<std::string_view> home);

/// @brief Resolve the config path from the process environment.
/// @return Path to the config file, or `std::nullopt` when
///         neither `$XDG_CONFIG_HOME` nor `$HOME` is set.
[[nodiscard]] std::optional<std::filesystem::path> default_config_path();

/// @brief Parse TOML config text.
/// @param text TOML document.
/// @return Parsed config.
/// @return @ref Error on malformed TOML, unknown top-level or
///         theme keys, malformed theme colors, or malformed
///         non-template config sections.
[[nodiscard]] Result<Config> parse_config(std::string_view text);

/// @brief Load config from a specific path.
/// @param path File path to read.
/// @return Parsed config. Missing or unreadable files return
///         defaults; malformed config returns defaults with a
///         warning.
[[nodiscard]] LoadResult load_config_from_path(const std::filesystem::path& path);

/// @brief Load config from @ref default_config_path.
/// @return Parsed config. Missing path or missing file returns
///         defaults.
[[nodiscard]] LoadResult load_config();

}  // namespace lazytmux::config
