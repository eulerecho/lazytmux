#include <lazytmux/error.hpp>

#include <format>
#include <utility>

namespace lazytmux {

Error::Error(std::string message, std::source_location loc)
    : message_(std::move(message)), where_(loc) {}

Error& Error::with_context(std::string ctx) {
    contexts_.push_back(std::move(ctx));
    return *this;
}

std::string_view Error::message() const noexcept {
    return message_;
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
