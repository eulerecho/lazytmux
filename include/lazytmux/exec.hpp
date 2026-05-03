#pragma once

#include <lazytmux/error.hpp>

#include <functional>
#include <span>
#include <string>

/// @file
/// @brief Process replacement seam for commands that hand off to tmux.

namespace lazytmux::exec {

/// @brief Injectable process replacement callback.
using ReplaceProcess = std::function<Result<void>(std::span<const std::string>)>;

/// @brief Replace the current process with @p argv using execvp.
/// @param argv Full argv. `argv[0]` is passed to `execvp`.
/// @return Only returns on invalid input or exec failure. On
///         success, the current process image is replaced.
[[nodiscard]] Result<void> replace_process(std::span<const std::string> argv);

}  // namespace lazytmux::exec
