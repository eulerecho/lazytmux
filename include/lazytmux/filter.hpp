#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

/// @file
/// @brief Buffered fuzzy-filter state plus the keyboard-event
/// model it consumes.

namespace lazytmux {

/// @brief Subset of keyboard events the filter buffer reacts to.
///
/// Local to this header because @ref FilterState is the sole
/// consumer. If a second consumer appears, this type can move
/// to a top-level header without changing its shape.
struct KeyEvent {
    /// @brief Discriminant. `Char` carries the literal in @ref ch.
    enum class Code { Char, Backspace, Enter, Esc };

    Code code{Code::Char};
    char ch{0};
    bool ctrl{false};
};

/// @brief Buffered fuzzy-filter state.
///
/// Holds the user's filter buffer, dispatches keyboard edits
/// (Backspace, Ctrl-W word-delete, Ctrl-U buffer-clear, Esc
/// clear-and-exit, Enter keep-and-exit, plain chars), and
/// scores haystacks against the buffer using a hand-rolled
/// subsequence-with-bonus matcher.
///
/// Smart-case: a buffer with no uppercase characters matches
/// case-insensitively; any uppercase character flips matching
/// to case-sensitive.
class FilterState {
public:
    /// @brief Whether the filter is accepting keystrokes.
    [[nodiscard]] bool is_active() const noexcept;

    /// @brief Current buffer contents.
    [[nodiscard]] std::string_view buffer() const noexcept;

    /// @brief Begin filtering. Buffer is preserved across enter
    /// calls. Esc clears the buffer; Enter does not.
    void enter();

    /// @brief Apply a key event.
    ///
    /// @param key The event to apply.
    /// @return @c true if the event was consumed (caller should
    ///         rebuild rows).
    /// @return @c false if the filter is inactive or the event
    ///         was a no-op (e.g. an unrecognized control combo).
    bool handle_key(const KeyEvent& key);

    /// @brief Score @p haystack against the current buffer.
    ///
    /// @param haystack The string to score.
    /// @return The score on match. Higher values indicate a
    ///         better match.
    /// @return @c std::nullopt if the buffer is empty or the
    ///         haystack does not contain the buffer as a
    ///         (smart-case) subsequence.
    ///
    /// The compiled (case-folded) buffer is cached and reused
    /// across @ref score calls until the buffer mutates.
    [[nodiscard]] std::optional<std::uint32_t> score(std::string_view haystack) const;

private:
    struct Cache {
        std::string buffer;
        std::string folded;
        bool case_sensitive;
    };

    [[nodiscard]] const Cache& ensure_cache() const;

    bool active_{false};
    std::string buffer_;
    mutable std::optional<Cache> cache_;
};

}  // namespace lazytmux
