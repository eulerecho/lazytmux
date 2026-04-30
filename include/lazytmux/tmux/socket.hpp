#pragma once

#include <optional>
#include <string>
#include <string_view>

/// @file
/// @brief String-only helper for parsing the socket-path field
/// out of a `$TMUX` environment-variable value.

namespace lazytmux::tmux {

/// @brief Parse the socket-path field out of a `$TMUX`
/// environment-variable value.
///
/// `$TMUX` is set by tmux to `"<socket-path>,<pid>,<session>"`.
/// This helper extracts the socket-path field. It does NOT read
/// the env var; callers do that and pass the value in.
///
/// @param raw The raw `$TMUX` value, or any comma-delimited
///            string. A value with no commas is returned
///            verbatim if non-empty.
/// @return The first comma-delimited field, copied into a new
///         string, on success.
/// @return @c std::nullopt when @p raw is empty or its first
///         field is empty (e.g. `",1,2"`).
[[nodiscard]] std::optional<std::string> parse_tmux_socket(std::string_view raw);

}  // namespace lazytmux::tmux
