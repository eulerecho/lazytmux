#include <lazytmux/error.hpp>
#include <lazytmux/tmux/ids.hpp>
#include <lazytmux/tmux/parse.hpp>
#include <lazytmux/tmux/types.hpp>

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace lazytmux::tmux {

namespace {

// Hand-rolled multi-char split. std::ranges::views::split with a
// std::string_view pattern works on GCC 13+, but writing the
// split here keeps the parser path free of range-pipe ceremony
// and produces a flat vector of views directly.
std::vector<std::string_view> split_on_delim(std::string_view s, std::string_view delim) {
    std::vector<std::string_view> out;
    if (delim.empty()) {
        out.emplace_back(s);
        return out;
    }
    std::size_t start = 0;
    while (start <= s.size()) {
        const auto pos = s.find(delim, start);
        if (pos == std::string_view::npos) {
            out.emplace_back(s.substr(start));
            break;
        }
        out.emplace_back(s.substr(start, pos - start));
        start = pos + delim.size();
    }
    return out;
}

template <typename T>
Result<T> parse_uint(std::string_view s, std::string_view field) {
    T value{};
    const auto* first = s.data();
    const auto* last = s.data() + s.size();
    auto [ptr, ec] = std::from_chars(first, last, value);
    if (ec != std::errc{} || ptr != last) {
        return std::unexpected(Error{std::format("invalid integer in field {}: \"{}\"", field, s)});
    }
    return value;
}

Result<std::int64_t> parse_int64(std::string_view s, std::string_view field) {
    std::int64_t value{};
    const auto* first = s.data();
    const auto* last = s.data() + s.size();
    auto [ptr, ec] = std::from_chars(first, last, value);
    if (ec != std::errc{} || ptr != last) {
        return std::unexpected(Error{std::format("invalid integer in field {}: \"{}\"", field, s)});
    }
    return value;
}

bool parse_bool01(std::string_view s) {
    return s == "1";
}

}  // namespace

Result<Session> parse_session_row(std::string_view row) {
    auto fields = split_on_delim(row, kFieldDelim);
    if (fields.size() != 5) {
        return std::unexpected(
            Error{std::format("session: expected 5 fields, got {}: \"{}\"", fields.size(), row)});
    }
    auto windows = parse_uint<std::uint32_t>(fields[2], "session.windows");
    if (!windows) {
        return std::unexpected(std::move(windows).error());
    }
    auto last_activity = parse_int64(fields[4], "session.last_activity");
    if (!last_activity) {
        return std::unexpected(std::move(last_activity).error());
    }
    return Session{
        .id = SessionId::from_validated(std::string{fields[0]}),
        .name = SessionName::from_validated(std::string{fields[1]}),
        .windows = *windows,
        .attached = parse_bool01(fields[3]),
        .last_activity_unix = *last_activity,
    };
}

Result<Window> parse_window_row(std::string_view row) {
    auto fields = split_on_delim(row, kFieldDelim);
    if (fields.size() != 6) {
        return std::unexpected(
            Error{std::format("window: expected 6 fields, got {}: \"{}\"", fields.size(), row)});
    }
    auto index = parse_uint<std::uint32_t>(fields[1], "window.index");
    if (!index) {
        return std::unexpected(std::move(index).error());
    }
    auto panes = parse_uint<std::uint32_t>(fields[5], "window.panes");
    if (!panes) {
        return std::unexpected(std::move(panes).error());
    }
    return Window{
        .session = SessionName::from_validated(std::string{fields[0]}),
        .index = *index,
        .id = WindowId::from_validated(std::string{fields[2]}),
        .name = std::string{fields[3]},
        .active = parse_bool01(fields[4]),
        .panes = *panes,
    };
}

Result<Pane> parse_pane_row(std::string_view row) {
    auto fields = split_on_delim(row, kFieldDelim);
    if (fields.size() != 6) {
        return std::unexpected(
            Error{std::format("pane: expected 6 fields, got {}: \"{}\"", fields.size(), row)});
    }
    auto index = parse_uint<std::uint32_t>(fields[1], "pane.index");
    if (!index) {
        return std::unexpected(std::move(index).error());
    }
    return Pane{
        .window_id = WindowId::from_validated(std::string{fields[0]}),
        .index = *index,
        .id = PaneId::from_validated(std::string{fields[2]}),
        .command = std::string{fields[3]},
        .current_path = std::string{fields[4]},
        .active = parse_bool01(fields[5]),
    };
}

Result<Client> parse_client_row(std::string_view row) {
    auto fields = split_on_delim(row, kFieldDelim);
    if (fields.size() != 4) {
        return std::unexpected(
            Error{std::format("client: expected 4 fields, got {}: \"{}\"", fields.size(), row)});
    }
    auto pid = parse_uint<std::uint32_t>(fields[0], "client.pid");
    if (!pid) {
        return std::unexpected(std::move(pid).error());
    }
    return Client{
        .pid = *pid,
        .session = SessionName::from_validated(std::string{fields[1]}),
        .tty = ClientTty::from_validated(std::string{fields[2]}),
        .term = std::string{fields[3]},
    };
}

Result<ServerInfo> parse_server_info_row(std::string_view row) {
    auto fields = split_on_delim(row, kFieldDelim);
    if (fields.size() != 3) {
        return std::unexpected(Error{
            std::format("server_info: expected 3 fields, got {}: \"{}\"", fields.size(), row)});
    }
    auto server_pid = parse_uint<std::uint32_t>(fields[0], "server_info.pid");
    if (!server_pid) {
        return std::unexpected(std::move(server_pid).error());
    }
    auto session_count = parse_uint<std::uint32_t>(fields[1], "server_info.sessions");
    if (!session_count) {
        return std::unexpected(std::move(session_count).error());
    }
    auto start_time = parse_int64(fields[2], "server_info.start_time");
    if (!start_time) {
        return std::unexpected(std::move(start_time).error());
    }
    return ServerInfo{
        .server_pid = *server_pid,
        .session_count = *session_count,
        .start_time_unix = *start_time,
    };
}

namespace {

// Aggregate parser shape: split on '\n', skip empty lines,
// parse each, propagate the first error with row-index context.
template <typename Row, typename RowParser>
Result<std::vector<Row>> parse_rows(std::string_view out,
                                    RowParser row_parser,
                                    std::string_view kind) {
    std::vector<Row> rows;
    std::size_t row_index = 0;
    std::size_t start = 0;
    while (start <= out.size()) {
        const auto nl = out.find('\n', start);
        const auto end = (nl == std::string_view::npos) ? out.size() : nl;
        auto line = out.substr(start, end - start);
        if (!line.empty()) {
            auto parsed = row_parser(line);
            if (!parsed) {
                auto err = std::move(parsed).error();
                err.with_context(std::format("{} row {}", kind, row_index));
                return std::unexpected(std::move(err));
            }
            rows.push_back(std::move(*parsed));
            ++row_index;
        }
        if (nl == std::string_view::npos) {
            break;
        }
        start = nl + 1;
    }
    return rows;
}

}  // namespace

Result<std::vector<Session>> parse_sessions(std::string_view out) {
    return parse_rows<Session>(out, parse_session_row, "session");
}

Result<std::vector<Window>> parse_windows(std::string_view out) {
    return parse_rows<Window>(out, parse_window_row, "window");
}

Result<std::vector<Pane>> parse_panes(std::string_view out) {
    return parse_rows<Pane>(out, parse_pane_row, "pane");
}

Result<std::vector<Client>> parse_clients(std::string_view out) {
    return parse_rows<Client>(out, parse_client_row, "client");
}

Result<ServerInfo> parse_server_info(std::string_view out) {
    // Trim a single trailing newline if present; tmux
    // display-message ends its single line that way.
    if (!out.empty() && out.back() == '\n') {
        out.remove_suffix(1);
    }
    return parse_server_info_row(out);
}

}  // namespace lazytmux::tmux
