#pragma once

#include <lazytmux/error.hpp>

#include <span>
#include <string>

/// @file
/// @brief Child-process runner with stdout and stderr capture.

namespace lazytmux::io {

/// @brief Captured result from a completed child process.
struct CommandResult {
    /// @brief Process exit code, or `128 + signal` when the
    ///        child was terminated by a signal.
    int exit_code{};

    /// @brief Captured standard output bytes.
    std::string stdout_text;

    /// @brief Captured standard error bytes.
    std::string stderr_text;
};

/// @brief Run an executable and capture its stdout and stderr.
/// @param argv Full argument vector. `argv[0]` is passed to
///        `execvp`.
/// @return Exit code plus captured output on successful spawn
///         and wait.
/// @return @ref Error when `argv` is empty, pipes cannot be
///         created, fork fails, reading output fails, or wait
///         fails.
[[nodiscard]] Result<CommandResult> run_command(std::span<const std::string> argv);

/// @brief Run an executable and report whether it exited with
/// code 0.
/// @param argv Full argument vector. `argv[0]` is passed to
///        `execvp`.
/// @return `true` for exit code 0, `false` for any non-zero
///         exit code or terminating signal.
/// @return @ref Error on spawn, pipe, read, or wait failure.
[[nodiscard]] Result<bool> run_command_status(std::span<const std::string> argv);

}  // namespace lazytmux::io
