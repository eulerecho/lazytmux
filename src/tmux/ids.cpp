#include <lazytmux/tmux/ids.hpp>

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace lazytmux::tmux {

namespace detail {

namespace {

constexpr std::string_view kFieldDelim = "|||";

}  // namespace

std::optional<Error> validate_id_chars(std::string_view s, std::string_view type_name) {
    if (s.empty()) {
        return Error{std::format("{}: empty string", type_name)};
    }
    if (s.find(':') != std::string_view::npos) {
        return Error{std::format("{}: invalid character ':' in \"{}\"", type_name, s)};
    }
    if (s.find('\n') != std::string_view::npos) {
        return Error{std::format("{}: invalid character '\\n' in \"{}\"", type_name, s)};
    }
    if (s.find('\r') != std::string_view::npos) {
        return Error{std::format("{}: invalid character '\\r' in \"{}\"", type_name, s)};
    }
    if (s.find(kFieldDelim) != std::string_view::npos) {
        return Error{std::format("{}: contains delimiter \"|||\" in \"{}\"", type_name, s)};
    }
    return std::nullopt;
}

namespace {

std::optional<Error> validate_template_name(std::string_view s) {
    if (s.empty()) {
        return Error{"TemplateName: empty string"};
    }
    return std::nullopt;
}

}  // namespace

}  // namespace detail

// ---- SessionName ------------------------------------------------------------

SessionName::SessionName(std::string s) : value_(std::move(s)) {}

Result<SessionName> SessionName::make(std::string_view s) {
    if (auto err = detail::validate_id_chars(s, "SessionName"); err) {
        return std::unexpected(std::move(*err));
    }
    return SessionName(std::string{s});
}

SessionName SessionName::from_validated(std::string s) {
    return SessionName(std::move(s));
}

std::string_view SessionName::as_str() const noexcept {
    return value_;
}

std::string SessionName::into_inner() && noexcept {
    return std::move(value_);
}

// ---- SessionId --------------------------------------------------------------

SessionId::SessionId(std::string s) : value_(std::move(s)) {}

Result<SessionId> SessionId::make(std::string_view s) {
    if (auto err = detail::validate_id_chars(s, "SessionId"); err) {
        return std::unexpected(std::move(*err));
    }
    return SessionId(std::string{s});
}

SessionId SessionId::from_validated(std::string s) {
    return SessionId(std::move(s));
}

std::string_view SessionId::as_str() const noexcept {
    return value_;
}

std::string SessionId::into_inner() && noexcept {
    return std::move(value_);
}

// ---- WindowId ---------------------------------------------------------------

WindowId::WindowId(std::string s) : value_(std::move(s)) {}

Result<WindowId> WindowId::make(std::string_view s) {
    if (auto err = detail::validate_id_chars(s, "WindowId"); err) {
        return std::unexpected(std::move(*err));
    }
    return WindowId(std::string{s});
}

WindowId WindowId::from_validated(std::string s) {
    return WindowId(std::move(s));
}

std::string_view WindowId::as_str() const noexcept {
    return value_;
}

std::string WindowId::into_inner() && noexcept {
    return std::move(value_);
}

// ---- PaneId -----------------------------------------------------------------

PaneId::PaneId(std::string s) : value_(std::move(s)) {}

Result<PaneId> PaneId::make(std::string_view s) {
    if (auto err = detail::validate_id_chars(s, "PaneId"); err) {
        return std::unexpected(std::move(*err));
    }
    return PaneId(std::string{s});
}

PaneId PaneId::from_validated(std::string s) {
    return PaneId(std::move(s));
}

std::string_view PaneId::as_str() const noexcept {
    return value_;
}

std::string PaneId::into_inner() && noexcept {
    return std::move(value_);
}

// ---- ClientTty --------------------------------------------------------------

ClientTty::ClientTty(std::string s) : value_(std::move(s)) {}

Result<ClientTty> ClientTty::make(std::string_view s) {
    if (auto err = detail::validate_id_chars(s, "ClientTty"); err) {
        return std::unexpected(std::move(*err));
    }
    return ClientTty(std::string{s});
}

ClientTty ClientTty::from_validated(std::string s) {
    return ClientTty(std::move(s));
}

std::string_view ClientTty::as_str() const noexcept {
    return value_;
}

std::string ClientTty::into_inner() && noexcept {
    return std::move(value_);
}

// ---- TemplateName -----------------------------------------------------------

TemplateName::TemplateName(std::string s) : value_(std::move(s)) {}

Result<TemplateName> TemplateName::make(std::string_view s) {
    if (auto err = detail::validate_template_name(s); err) {
        return std::unexpected(std::move(*err));
    }
    return TemplateName(std::string{s});
}

TemplateName TemplateName::from_validated(std::string s) {
    return TemplateName(std::move(s));
}

std::string_view TemplateName::as_str() const noexcept {
    return value_;
}

std::string TemplateName::into_inner() && noexcept {
    return std::move(value_);
}

}  // namespace lazytmux::tmux
