#pragma once

#include <lazytmux/error.hpp>

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

/// @file
/// @brief Zoxide scored-directory query wrapper and parser.

namespace lazytmux::zoxide {

/// @brief One row from `zoxide query --list --score`.
struct Entry {
    /// @brief Directory path printed by zoxide.
    std::filesystem::path path;

    /// @brief Zoxide's already-decayed rank.
    double rank{};

    auto operator<=>(const Entry&) const = default;
    bool operator==(const Entry&) const = default;
};

/// @brief Parse `zoxide query --list --score` text output.
/// @param text Command stdout.
/// @return Parsed entries in input order.
/// @return @ref Error when a non-empty line is malformed.
[[nodiscard]] Result<std::vector<Entry>> parse_entries(std::string_view text);

/// @brief Load up to @p limit zoxide entries.
/// @param limit Maximum number of entries to return.
/// @param executable Zoxide executable name or path.
/// @return Parsed entries, truncated to @p limit. A missing
///         zoxide executable returns an empty vector.
/// @return @ref Error when zoxide exits non-zero or prints
///         malformed output.
[[nodiscard]] Result<std::vector<Entry>> load_top(std::size_t limit,
                                                  const std::string& executable = "zoxide");

}  // namespace lazytmux::zoxide
