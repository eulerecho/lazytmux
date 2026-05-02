#pragma once

#include <span>
#include <string_view>

/// @file
/// @brief Static list of tmux subcommands recognized by
/// cmdmode head-completion.

namespace lazytmux::cmdmode {

/// @brief Sorted, lowercase tmux subcommands and well-known
/// aliases. Drawn from `man tmux`'s "COMMANDS" section.
extern const std::span<const std::string_view> kCommands;

}  // namespace lazytmux::cmdmode
