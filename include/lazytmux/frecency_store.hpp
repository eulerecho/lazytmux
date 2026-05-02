#pragma once

#include <lazytmux/error.hpp>
#include <lazytmux/frecency.hpp>

#include <filesystem>

/// @file
/// @brief Filesystem persistence for the frecency index.

namespace lazytmux::frecency {

/// @brief Load a frecency index from a JSON file.
/// @param path Path to the JSON file.
/// @return Parsed index on success. A missing file returns an
///         empty index.
/// @return @ref Error on unreadable files, malformed JSON, or
///         invalid session names.
[[nodiscard]] Result<Index> load_index(const std::filesystem::path& path);

/// @brief Save a frecency index with a temp-file-and-rename
/// update.
/// @param index Index to serialize.
/// @param path Destination JSON path.
/// @return Empty success on write and rename.
/// @return @ref Error on directory creation, write, or rename
///         failure.
[[nodiscard]] Result<void> save_index(const Index& index, const std::filesystem::path& path);

}  // namespace lazytmux::frecency
