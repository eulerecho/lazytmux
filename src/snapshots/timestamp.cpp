#include <lazytmux/error.hpp>
#include <lazytmux/snapshots/timestamp.hpp>

#include <charconv>
#include <cstdint>
#include <expected>
#include <format>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace lazytmux::snapshots {

namespace {

constexpr std::string_view kPrefix = "tmux_resurrect_";
constexpr std::string_view kSuffix = ".txt";

template <typename T>
Result<T> parse_segment(std::string_view s, std::string_view field) {
    T value{};
    const auto* first = s.data();
    const auto* last = s.data() + s.size();
    auto [ptr, ec] = std::from_chars(first, last, value);
    if (ec != std::errc{} || ptr != last) {
        return std::unexpected(
            Error{std::format("snapshot timestamp: invalid {} segment \"{}\"", field, s)});
    }
    return value;
}

}  // namespace

std::string Timestamp::display() const {
    return std::format(
        "{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}", year, month, day, hour, minute, second);
}

Result<Timestamp> parse_snapshot_filename(std::string_view filename) {
    if (!filename.starts_with(kPrefix)) {
        return std::unexpected(Error{std::format(
            "snapshot timestamp: missing prefix \"tmux_resurrect_\" in \"{}\"", filename)});
    }
    if (!filename.ends_with(kSuffix)) {
        return std::unexpected(
            Error{std::format("snapshot timestamp: missing suffix \".txt\" in \"{}\"", filename)});
    }
    auto stem = filename;
    stem.remove_prefix(kPrefix.size());
    stem.remove_suffix(kSuffix.size());

    const auto t_pos = stem.find('T');
    if (t_pos == std::string_view::npos) {
        return std::unexpected(
            Error{std::format("snapshot timestamp: missing 'T' separator in \"{}\"", filename)});
    }
    auto date = stem.substr(0, t_pos);
    auto time = stem.substr(t_pos + 1);
    if (date.size() != 8 || time.size() != 6) {
        return std::unexpected(
            Error{std::format("snapshot timestamp: unexpected shape in \"{}\"", filename)});
    }

    auto year = parse_segment<std::uint16_t>(date.substr(0, 4), "year");
    if (!year) {
        return std::unexpected(std::move(year).error());
    }
    auto month = parse_segment<std::uint8_t>(date.substr(4, 2), "month");
    if (!month) {
        return std::unexpected(std::move(month).error());
    }
    auto day = parse_segment<std::uint8_t>(date.substr(6, 2), "day");
    if (!day) {
        return std::unexpected(std::move(day).error());
    }
    auto hour = parse_segment<std::uint8_t>(time.substr(0, 2), "hour");
    if (!hour) {
        return std::unexpected(std::move(hour).error());
    }
    auto minute = parse_segment<std::uint8_t>(time.substr(2, 2), "minute");
    if (!minute) {
        return std::unexpected(std::move(minute).error());
    }
    auto second = parse_segment<std::uint8_t>(time.substr(4, 2), "second");
    if (!second) {
        return std::unexpected(std::move(second).error());
    }

    return Timestamp{
        .year = *year,
        .month = *month,
        .day = *day,
        .hour = *hour,
        .minute = *minute,
        .second = *second,
    };
}

}  // namespace lazytmux::snapshots
