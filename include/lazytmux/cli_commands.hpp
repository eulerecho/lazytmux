#pragma once

#include <lazytmux/cli.hpp>
#include <lazytmux/config.hpp>
#include <lazytmux/exec.hpp>
#include <lazytmux/tmux/commands.hpp>

#include <filesystem>
#include <functional>
#include <span>
#include <string_view>

/// @file
/// @brief Testable CLI command execution.

namespace lazytmux::cli {

inline constexpr int kExitSuccess = 0;
inline constexpr int kExitFailure = 1;
inline constexpr int kExitUsage = 2;

/// @brief External dependencies used by CLI command execution.
struct CommandDependencies {
    /// @brief Load user config. Injected in tests.
    std::function<config::LoadResult()> load_config;

    /// @brief tmux command runner. Injected in tests.
    tmux::CommandRunner tmux_runner;

    /// @brief Locate the tmux-resurrect save script. Injected in tests.
    std::function<Result<std::filesystem::path>()> locate_save_script;

    /// @brief Replace this process with another executable. Injected in tests.
    exec::ReplaceProcess replace_process;
};

/// @brief Writable output channels used by CLI command execution.
struct OutputSinks {
    /// @brief Normal command output.
    std::function<void(std::string_view)> out;

    /// @brief Errors and warnings.
    std::function<void(std::string_view)> err;
};

/// @brief Production dependencies backed by process environment and tmux.
/// @return Default command dependencies.
[[nodiscard]] CommandDependencies default_command_dependencies();

/// @brief Execute an already parsed request.
/// @param request Parsed CLI request.
/// @param dependencies External dependencies.
/// @param sinks Output callbacks.
/// @return Process exit code.
[[nodiscard]] int run_request(const CliRequest& request,
                              const CommandDependencies& dependencies,
                              const OutputSinks& sinks);

/// @brief Parse and execute CLI args.
/// @param args Arguments after argv[0].
/// @param dependencies External dependencies.
/// @param sinks Output callbacks.
/// @return Process exit code.
[[nodiscard]] int run_args(std::span<const std::string_view> args,
                           const CommandDependencies& dependencies,
                           const OutputSinks& sinks);

}  // namespace lazytmux::cli
