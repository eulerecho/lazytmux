#include <lazytmux/filter.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace lazytmux {

namespace {

bool needs_case_sensitive(std::string_view buf) {
    return std::ranges::any_of(
        buf, [](char c) { return std::isupper(static_cast<unsigned char>(c)) != 0; });
}

}  // namespace

bool FilterState::is_active() const noexcept {
    return active_;
}

std::string_view FilterState::buffer() const noexcept {
    return buffer_;
}

void FilterState::enter() {
    active_ = true;
}

const FilterState::Cache& FilterState::ensure_cache() const {
    if (!cache_ || cache_->buffer != buffer_) {
        const bool cs = needs_case_sensitive(buffer_);
        std::string folded = buffer_;
        if (!cs) {
            std::ranges::transform(folded, folded.begin(), [](char c) {
                return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            });
        }
        cache_ = Cache{.buffer = buffer_, .folded = std::move(folded), .case_sensitive = cs};
    }
    return *cache_;
}

bool FilterState::handle_key(const KeyEvent& k) {
    if (!active_) {
        return false;
    }
    using C = KeyEvent::Code;
    switch (k.code) {
        case C::Esc:
            active_ = false;
            buffer_.clear();
            return true;
        case C::Enter:
            active_ = false;
            return true;
        case C::Backspace:
            if (!buffer_.empty()) {
                buffer_.pop_back();
            }
            return true;
        case C::Char:
            break;
    }

    if (k.ctrl && (k.ch == 'u' || k.ch == 'U')) {
        buffer_.clear();
        return true;
    }
    if (k.ctrl && (k.ch == 'w' || k.ch == 'W')) {
        while (!buffer_.empty() && std::isspace(static_cast<unsigned char>(buffer_.back())) != 0) {
            buffer_.pop_back();
        }
        while (!buffer_.empty() && std::isspace(static_cast<unsigned char>(buffer_.back())) == 0) {
            buffer_.pop_back();
        }
        return true;
    }
    if (!k.ctrl) {
        buffer_.push_back(k.ch);
        return true;
    }
    return false;
}

std::optional<std::uint32_t> FilterState::score(std::string_view hay) const {
    if (buffer_.empty()) {
        return std::nullopt;
    }
    const auto& c = ensure_cache();
    const auto& pat = c.folded;

    constexpr std::uint32_t kBaseBonus = 16;
    constexpr std::uint32_t kRunBonus = 32;
    constexpr std::uint32_t kPrefixBonus = 32;
    constexpr std::uint32_t kWordBoundaryBonus = 8;

    std::size_t pi = 0;
    std::uint32_t score = 0;
    bool prev_matched = false;
    char prev_hay = ' ';

    for (std::size_t hi = 0; hi < hay.size() && pi < pat.size(); ++hi) {
        const char hc_raw = hay[hi];
        const char hc = c.case_sensitive
                            ? hc_raw
                            : static_cast<char>(std::tolower(static_cast<unsigned char>(hc_raw)));
        if (hc == pat[pi]) {
            std::uint32_t add = kBaseBonus;
            if (prev_matched) {
                add += kRunBonus;
            }
            if (hi == 0) {
                add += kPrefixBonus;
            } else if (prev_hay == ' ' || prev_hay == '_' || prev_hay == '-' || prev_hay == '/') {
                add += kWordBoundaryBonus;
            }
            score += add;
            prev_matched = true;
            ++pi;
        } else {
            prev_matched = false;
        }
        prev_hay = hc_raw;
    }

    if (pi != pat.size()) {
        return std::nullopt;
    }
    return score;
}

}  // namespace lazytmux
