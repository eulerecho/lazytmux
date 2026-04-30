#include <lazytmux/colors.hpp>

#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <system_error>

namespace lazytmux {

std::optional<Rgb> parse_hex(std::string_view s) {
    if (s.starts_with('#')) {
        s.remove_prefix(1);
    }
    if (s.size() != 6) {
        return std::nullopt;
    }
    for (char ch : s) {
        const auto c = static_cast<unsigned char>(ch);
        if (c >= 0x80 || std::isxdigit(c) == 0) {
            return std::nullopt;
        }
    }

    auto parse_byte = [&](std::size_t off) -> std::optional<std::uint8_t> {
        std::uint8_t value{};
        const auto* first = s.data() + off;
        const auto* last = first + 2;
        auto [ptr, ec] = std::from_chars(first, last, value, 16);
        if (ec != std::errc{} || ptr != last) {
            return std::nullopt;
        }
        return value;
    };

    auto r = parse_byte(0);
    auto g = parse_byte(2);
    auto b = parse_byte(4);
    if (!r || !g || !b) {
        return std::nullopt;
    }
    return Rgb{.r = *r, .g = *g, .b = *b};
}

}  // namespace lazytmux
