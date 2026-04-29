#pragma once

#include <string_view>

/// @file
/// @brief Compile-time version metadata for the lazytmux binary.

namespace lazytmux {

/// Compile-time version string of the lazytmux binary.
///
/// Format: semver with an optional pre-release suffix (e.g.
/// `"0.1.0-dev"`, `"0.1.0"`, `"0.2.0-rc.1"`). Updated by hand
/// on releases; tooling that needs a version at runtime should
/// read this constant rather than parsing the binary name.
inline constexpr std::string_view kVersion = "0.1.0-dev";

}  // namespace lazytmux
