#pragma once

#include <lazytmux/error.hpp>
#include <lazytmux/io/process.hpp>

#include <span>
#include <string>

/// @file
/// @brief Thin tmux command runner built on the process runner.

namespace lazytmux::tmux {

/// @brief Socket selector kind for tmux invocation.
enum class SocketSelectionKind {
    /// @brief Use tmux's default socket selection.
    kDefault,
    /// @brief Pass `-L <name>` to tmux.
    kNamed,
    /// @brief Pass `-S <path>` to tmux.
    kPath,
};

/// @brief User-requested tmux socket selection.
struct SocketSelection {
    /// @brief Socket selection mode.
    SocketSelectionKind kind{SocketSelectionKind::kDefault};

    /// @brief Named socket or socket path, depending on @ref kind.
    std::string value;

    /// @brief Select tmux's default socket.
    /// @return Default socket selection.
    [[nodiscard]] static SocketSelection default_socket();

    /// @brief Select a named tmux socket through `-L`.
    /// @param name Socket name.
    /// @return Named socket selection.
    [[nodiscard]] static SocketSelection named(std::string name);

    /// @brief Select an explicit tmux socket path through `-S`.
    /// @param path Socket path.
    /// @return Socket-path selection.
    [[nodiscard]] static SocketSelection path(std::string path);
};

/// @brief Configuration for invoking the tmux executable.
struct RunConfig {
    /// @brief Executable passed as `argv[0]`.
    std::string executable{"tmux"};

    /// @brief Socket selector passed before tmux subcommand args.
    SocketSelection socket;

    /// @brief Process execution limits.
    io::CommandOptions command_options;
};

/// @brief Run tmux and return stdout when it exits with code 0.
/// @param args Arguments to pass after the executable and
///        optional socket selector.
/// @param config Executable, socket selector, and process limits.
/// @return Captured stdout on exit code 0.
/// @return @ref Error on spawn failure or non-zero exit.
[[nodiscard]] Result<std::string> run(std::span<const std::string> args,
                                      const RunConfig& config = {});

/// @brief Run tmux as a predicate.
/// @param args Arguments to pass after the executable and
///        optional socket selector.
/// @param config Executable, socket selector, and process limits.
/// @return `true` for exit code 0 and `false` for non-zero exit.
/// @return @ref Error on spawn, pipe, read, or wait failure.
[[nodiscard]] Result<bool> run_status(std::span<const std::string> args,
                                      const RunConfig& config = {});

}  // namespace lazytmux::tmux
