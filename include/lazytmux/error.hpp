#pragma once

#include <expected>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// @file
/// @brief Error type and Result alias used at every module boundary.

namespace lazytmux {

/// @brief Coarse category for a failure.
///
/// Error kinds are intentionally broad. They exist so CLI and TUI
/// code can choose exit codes, notification severity, retry policy,
/// and user-facing copy without parsing @ref Error::message text.
enum class ErrorKind {
    /// @brief Caller supplied invalid input.
    kInvalidInput,
    /// @brief Requested file, process, tmux object, or executable was not found.
    kNotFound,
    /// @brief The OS or external tool denied access to the requested resource.
    kPermissionDenied,
    /// @brief Non-config input could not be parsed.
    kParse,
    /// @brief User configuration was malformed or unsupported.
    kConfig,
    /// @brief An external command exited unsuccessfully.
    kExternalCommandFailure,
    /// @brief An operation exceeded its deadline.
    kTimeout,
    /// @brief Filesystem, process, socket, or other OS I/O failed.
    kIo,
    /// @brief The current platform cannot support the requested operation.
    kUnsupportedPlatform,
    /// @brief A programmer error or unexpected internal invariant failure.
    kInternal,
};

/// @brief Stable ASCII name for an @ref ErrorKind.
/// @param kind Kind to name.
/// @return String name suitable for logs and debug output.
[[nodiscard]] std::string_view error_kind_name(ErrorKind kind) noexcept;

/// @brief A failure description with a capture site and an
/// append-only stack of context labels.
///
/// `Error` is the failure half of @ref Result. Construct it
/// with a message; the source location is captured by default
/// from the call site. As an error propagates upward, callers
/// may append context labels via @ref with_context to record
/// what they were doing when the failure surfaced. The chain
/// is intentionally one-way (append-only); errors do not
/// support equality because two errors with different context
/// stacks describe the same underlying failure differently.
class Error {
public:
    /// @brief Construct an internal error with a message.
    /// @param message Human-readable failure description.
    /// @param loc Capture site, defaulted to the caller's
    ///        location via `std::source_location::current()`.
    Error(std::string message, std::source_location loc = std::source_location::current());

    /// @brief Construct an error with a message.
    /// @param kind Coarse failure category.
    /// @param message Human-readable failure description.
    /// @param loc Capture site, defaulted to the caller's
    ///        location via `std::source_location::current()`.
    Error(ErrorKind kind,
          std::string message,
          std::source_location loc = std::source_location::current());

    /// @brief Append a context label and return *this for
    /// chaining.
    /// @param ctx Label describing what the caller was doing.
    Error& with_context(std::string ctx);

    /// @brief The message passed at construction.
    [[nodiscard]] std::string_view message() const noexcept;

    /// @brief The coarse failure category.
    [[nodiscard]] ErrorKind kind() const noexcept;

    /// @brief The source location captured at construction.
    [[nodiscard]] const std::source_location& where() const noexcept;

    /// @brief The context-label stack, in append order
    /// (oldest first).
    [[nodiscard]] std::span<const std::string> contexts() const noexcept;

    /// @brief Render the error as a single line:
    /// `"<message> [<ctx_oldest>] [<ctx_newer>] @ <file>:<line>"`.
    [[nodiscard]] std::string display() const;

private:
    ErrorKind kind_;
    std::string message_;
    std::source_location where_;
    std::vector<std::string> contexts_;
};

/// @brief Project-wide @c std::expected alias for fallible
/// returns. Every public function in lazytmux that can fail
/// returns @c Result<T>.
template <typename T>
using Result = std::expected<T, Error>;

}  // namespace lazytmux

/// @brief Propagate a @ref lazytmux::Result on the failure
/// path, evaluating to the success value otherwise.
///
/// Implemented as a GCC statement-expression. The project
/// targets GCC 14+ and Clang 18+, both of which support this.
/// Use only inside `.cpp` files; never in headers.
#define LAZYTMUX_TRY(expr_)                                     \
    ({                                                          \
        auto&& _r = (expr_);                                    \
        if (!_r) return std::unexpected(std::move(_r).error()); \
        std::move(*_r);                                         \
    })
