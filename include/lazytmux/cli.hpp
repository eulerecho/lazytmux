#pragma once

#include <lazytmux/error.hpp>
#include <lazytmux/tmux/ids.hpp>
#include <lazytmux/tmux/run.hpp>

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <variant>

/// @file
/// @brief CLI argument parser for the lazytmux binary.

namespace lazytmux::cli {

/// @brief Parsed command category.
enum class CommandKind {
    /// @brief Show top-level usage.
    kHelp,
    /// @brief Print the binary version.
    kVersion,
    /// @brief Attach to a tmux session.
    kAttach,
    /// @brief Ask tmux-resurrect to save state.
    kSave,
    /// @brief List configured templates.
    kListTemplates,
    /// @brief Materialize a configured template.
    kMaterialize,
};

/// @brief Request to show top-level usage.
struct HelpCommand {};

/// @brief Request to print the binary version.
struct VersionCommand {};

/// @brief Request to attach to an existing tmux session.
struct AttachCommand {
    /// @brief Target session name.
    tmux::SessionName session;
};

/// @brief Request to save tmux-resurrect state.
struct SaveCommand {};

/// @brief Request to list configured templates.
struct ListTemplatesCommand {};

/// @brief Request to materialize a configured template.
struct MaterializeCommand {
    /// @brief Template selected by name.
    tmux::TemplateName template_name;

    /// @brief Working directory used when creating tmux windows.
    std::filesystem::path directory;
};

/// @brief Parsed subcommand payload.
using Command = std::variant<HelpCommand,
                             VersionCommand,
                             AttachCommand,
                             SaveCommand,
                             ListTemplatesCommand,
                             MaterializeCommand>;

/// @brief Parsed CLI request.
struct CliRequest {
    /// @brief User-selected tmux socket.
    tmux::SocketSelection socket;

    /// @brief Requested command.
    Command command;
};

/// @brief Parse lazytmux command-line arguments.
/// @param args Arguments after argv[0].
/// @return Parsed request on success.
/// @return @ref ErrorKind::kInvalidInput for usage errors.
[[nodiscard]] Result<CliRequest> parse_args(std::span<const std::string_view> args);

/// @brief Top-level usage text.
/// @return Human-readable usage and command summary.
[[nodiscard]] std::string usage();

/// @brief Usage text for one command.
/// @param kind Command to describe.
/// @return Human-readable command usage.
[[nodiscard]] std::string command_usage(CommandKind kind);

/// @brief Return the category for a parsed command payload.
/// @param command Parsed command payload.
/// @return The command category.
[[nodiscard]] CommandKind command_kind(const Command& command) noexcept;

}  // namespace lazytmux::cli
