#pragma once

#include <lazytmux/error.hpp>

#include <chrono>
#include <cstddef>
#include <span>
#include <string>

/// @file
/// @brief Child-process runner with stdout and stderr capture.

namespace lazytmux::io {

/// @brief Default deadline for a child process.
inline constexpr std::chrono::milliseconds kDefaultCommandTimeout{5000};

/// @brief Default maximum combined stdout/stderr bytes captured
/// from a child process.
inline constexpr std::size_t kDefaultMaxCapturedOutputBytes = 4U * 1024U * 1024U;

/// @brief Execution limits for a child process.
struct CommandOptions {
    /// @brief Deadline for process completion and output capture.
    std::chrono::milliseconds timeout{kDefaultCommandTimeout};

    /// @brief Maximum combined stdout/stderr bytes to capture.
    std::size_t max_output_bytes{kDefaultMaxCapturedOutputBytes};
};

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
/// @param options Timeout and captured-output limits.
/// @return Exit code plus captured output on successful spawn
///         and wait.
/// @return @ref Error when `argv` is empty, pipes cannot be
///         created, fork fails, reading output fails, the command
///         times out, the output cap is exceeded, or wait fails.
[[nodiscard]] Result<CommandResult> run_command(std::span<const std::string> argv,
                                                const CommandOptions& options = {});

/// @brief Run an executable and report whether it exited with
/// code 0.
/// @param argv Full argument vector. `argv[0]` is passed to
///        `execvp`.
/// @param options Timeout and captured-output limits.
/// @return `true` for exit code 0, `false` for any non-zero
///         exit code or terminating signal.
/// @return @ref Error on spawn, pipe, read, wait, timeout, or
///         captured-output-limit failure.
[[nodiscard]] Result<bool> run_command_status(std::span<const std::string> argv,
                                              const CommandOptions& options = {});

}  // namespace lazytmux::io
