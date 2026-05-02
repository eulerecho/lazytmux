#pragma once

#include <lazytmux/error.hpp>

#include <optional>
#include <span>
#include <string>

/// @file
/// @brief Thin tmux command runner built on the process runner.

namespace lazytmux::tmux {

/// @brief Configuration for invoking the tmux executable.
struct RunConfig {
    /// @brief Executable passed as `argv[0]`.
    std::string executable{"tmux"};

    /// @brief Optional socket path passed as `-S <path>`.
    std::optional<std::string> socket_path;
};

/// @brief Run tmux and return stdout when it exits with code 0.
/// @param args Arguments to pass after the executable and
///        optional socket selector.
/// @param config Executable and optional socket path.
/// @return Captured stdout on exit code 0.
/// @return @ref Error on spawn failure or non-zero exit.
[[nodiscard]] Result<std::string> run(std::span<const std::string> args,
                                      const RunConfig& config = {});

/// @brief Run tmux as a predicate.
/// @param args Arguments to pass after the executable and
///        optional socket selector.
/// @param config Executable and optional socket path.
/// @return `true` for exit code 0 and `false` for non-zero exit.
/// @return @ref Error on spawn, pipe, read, or wait failure.
[[nodiscard]] Result<bool> run_status(std::span<const std::string> args,
                                      const RunConfig& config = {});

}  // namespace lazytmux::tmux
