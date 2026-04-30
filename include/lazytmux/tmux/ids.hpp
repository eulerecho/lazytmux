#pragma once

#include <lazytmux/error.hpp>

#include <compare>
#include <string>
#include <string_view>
#include <utility>

/// @file
/// @brief Strongly-typed identifiers for tmux entities and templates.
///
/// Each newtype wraps a `std::string` and exposes a validating
/// factory @c make plus a trusted-source factory
/// @c from_validated. The strict newtypes (`SessionName`,
/// `SessionId`, `WindowId`, `PaneId`, `ClientTty`) reject
/// strings that are empty or contain `':'`, `'\n'`, `'\r'`, or
/// the field delimiter `"|||"` — the same character set tmux
/// disallows in identifiers and that would corrupt
/// `|||`-delimited format-string output. `TemplateName` is
/// loose: it rejects only empty input, since templates come
/// from the user's TOML config (a trusted source).

namespace lazytmux::tmux {

/// @brief Validated tmux session name.
class SessionName {
public:
    /// @brief Validating factory.
    /// @return The constructed name on success.
    /// @return @ref Error on empty, `':'`, `'\n'`, `'\r'`, or `"|||"`.
    [[nodiscard]] static Result<SessionName> make(std::string_view s);

    /// @brief Trusted-source factory. Skips validation.
    /// @pre Caller asserts @c s came from a parser where the
    ///      `|||` delimiter cannot appear inside any field.
    [[nodiscard]] static SessionName from_validated(std::string s);

    [[nodiscard]] std::string_view as_str() const noexcept;
    [[nodiscard]] std::string into_inner() && noexcept;

    auto operator<=>(const SessionName&) const = default;
    bool operator==(const SessionName&) const = default;

private:
    explicit SessionName(std::string s);
    std::string value_;
};

/// @brief Validated tmux session id (e.g. `$0`).
class SessionId {
public:
    [[nodiscard]] static Result<SessionId> make(std::string_view s);
    [[nodiscard]] static SessionId from_validated(std::string s);

    [[nodiscard]] std::string_view as_str() const noexcept;
    [[nodiscard]] std::string into_inner() && noexcept;

    auto operator<=>(const SessionId&) const = default;
    bool operator==(const SessionId&) const = default;

private:
    explicit SessionId(std::string s);
    std::string value_;
};

/// @brief Validated tmux window id (e.g. `@5`).
class WindowId {
public:
    [[nodiscard]] static Result<WindowId> make(std::string_view s);
    [[nodiscard]] static WindowId from_validated(std::string s);

    [[nodiscard]] std::string_view as_str() const noexcept;
    [[nodiscard]] std::string into_inner() && noexcept;

    auto operator<=>(const WindowId&) const = default;
    bool operator==(const WindowId&) const = default;

private:
    explicit WindowId(std::string s);
    std::string value_;
};

/// @brief Validated tmux pane id (e.g. `%12`).
class PaneId {
public:
    [[nodiscard]] static Result<PaneId> make(std::string_view s);
    [[nodiscard]] static PaneId from_validated(std::string s);

    [[nodiscard]] std::string_view as_str() const noexcept;
    [[nodiscard]] std::string into_inner() && noexcept;

    auto operator<=>(const PaneId&) const = default;
    bool operator==(const PaneId&) const = default;

private:
    explicit PaneId(std::string s);
    std::string value_;
};

/// @brief Validated tmux client tty path (e.g. `/dev/pts/2`).
class ClientTty {
public:
    [[nodiscard]] static Result<ClientTty> make(std::string_view s);
    [[nodiscard]] static ClientTty from_validated(std::string s);

    [[nodiscard]] std::string_view as_str() const noexcept;
    [[nodiscard]] std::string into_inner() && noexcept;

    auto operator<=>(const ClientTty&) const = default;
    bool operator==(const ClientTty&) const = default;

private:
    explicit ClientTty(std::string s);
    std::string value_;
};

/// @brief Loosely-validated template name (rejects only empty).
///
/// Template names come from the user's TOML config, which is a
/// trusted source. The validation rules for the strict
/// identifiers don't apply — a template named `"my:project"`
/// is allowed.
class TemplateName {
public:
    [[nodiscard]] static Result<TemplateName> make(std::string_view s);
    [[nodiscard]] static TemplateName from_validated(std::string s);

    [[nodiscard]] std::string_view as_str() const noexcept;
    [[nodiscard]] std::string into_inner() && noexcept;

    auto operator<=>(const TemplateName&) const = default;
    bool operator==(const TemplateName&) const = default;

private:
    explicit TemplateName(std::string s);
    std::string value_;
};

}  // namespace lazytmux::tmux
