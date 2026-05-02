#include <lazytmux/error.hpp>

#include <format>
#include <utility>

namespace lazytmux {

Error::Error(std::string message, std::source_location loc)
    : Error(ErrorKind::kInternal, std::move(message), loc) {}

Error::Error(ErrorKind kind, std::string message, std::source_location loc)
    : kind_(kind), message_(std::move(message)), where_(loc) {}

std::string_view error_kind_name(ErrorKind kind) noexcept {
    switch (kind) {
        case ErrorKind::kInvalidInput:
            return "invalid-input";
        case ErrorKind::kNotFound:
            return "not-found";
        case ErrorKind::kPermissionDenied:
            return "permission-denied";
        case ErrorKind::kParse:
            return "parse";
        case ErrorKind::kConfig:
            return "config";
        case ErrorKind::kExternalCommandFailure:
            return "external-command-failure";
        case ErrorKind::kTimeout:
            return "timeout";
        case ErrorKind::kIo:
            return "io";
        case ErrorKind::kUnsupportedPlatform:
            return "unsupported-platform";
        case ErrorKind::kInternal:
            return "internal";
    }
    return "internal";
}

Error& Error::with_context(std::string ctx) {
    contexts_.push_back(std::move(ctx));
    return *this;
}

std::string_view Error::message() const noexcept {
    return message_;
}

ErrorKind Error::kind() const noexcept {
    return kind_;
}

const std::source_location& Error::where() const noexcept {
    return where_;
}

std::span<const std::string> Error::contexts() const noexcept {
    return contexts_;
}

std::string Error::display() const {
    std::string out = message_;
    for (const auto& ctx : contexts_) {
        out += std::format(" [{}]", ctx);
    }
    out += std::format(" @ {}:{}", where_.file_name(), where_.line());
    return out;
}

}  // namespace lazytmux
