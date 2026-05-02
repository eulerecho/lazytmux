#pragma once

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// @file
/// @brief Cmdmode tokenizer, argument schemas, shell-quoter, and
/// argument-aware completer.

namespace lazytmux::cmdmode {

/// @brief Argument-kind discriminant for command schemas. Drives
/// the candidate source the completer consults at Tab time.
enum class ArgKind { Session, Window, Pane, FreeText };

/// @brief Per-command argument shape. `args[0]` is the first
/// positional after the command name; `-t flag` argument
/// positions are not modeled.
struct ArgSchema {
    std::string_view command;
    std::span<const ArgKind> args;
};

/// @brief Tokenize @p input as a shell command line.
///
/// Supports double quotes (with `\"` and `\\` escapes), single
/// quotes (no escapes inside), and backslash escapes outside
/// quotes. Whitespace splits unquoted regions.
///
/// @param input The raw user input.
/// @return Tokens on success.
/// @return @c std::nullopt if a quote or trailing backslash is
///         unterminated.
[[nodiscard]] std::optional<std::vector<std::string>> tokenize(std::string_view input);

/// @brief Look up @p command in the schema table.
/// @return Pointer to the static schema, or @c nullptr if no
///         schema is registered.
[[nodiscard]] const ArgSchema* lookup_schema(std::string_view command);

/// @brief Render @p s as a single shell-quoted token.
///
/// Empty strings render as `""`. Strings containing whitespace,
/// `'`, `"`, or `\` are wrapped in double quotes with embedded
/// `"` and `\` escaped. Other strings are returned verbatim.
[[nodiscard]] std::string shell_quote(std::string_view s);

/// @brief Compute completion suggestions for @p input.
///
/// Two paths:
/// 1. Head completion (when the head token is not yet a
///    recognized command, or there's no trailing whitespace and
///    only one token): filter @p command_names by prefix and
///    reattach the raw tail of @p input. Unbalanced quotes fall
///    here too; the tail is preserved so the user sees their
///    own input echoed back.
/// 2. Argument completion (when the head is recognized and the
///    cursor has moved past it): look up the schema, ask
///    @p candidates for the resolved @ref ArgKind, and render
///    each match as a shell-quoted completion of the existing
///    input.
[[nodiscard]] std::vector<std::string> complete(
    std::string_view input,
    std::span<const std::string_view> command_names,
    const std::function<std::vector<std::string>(ArgKind)>& candidates);

/// @brief All cmdmode argument schemas. Span over a static
/// table; entries are ordered by family (attach, switch,
/// kill, rename, select, swap) within session / window / pane.
extern const std::span<const ArgSchema> kSchemas;

}  // namespace lazytmux::cmdmode
