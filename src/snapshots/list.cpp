#include <lazytmux/snapshots/list.hpp>

#include <algorithm>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <limits>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace lazytmux::snapshots {
namespace {

std::optional<std::string_view> getenv_view(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr) {
        return std::nullopt;
    }
    return std::string_view{value};
}

std::vector<std::string_view> split_tabs(std::string_view line) {
    std::vector<std::string_view> fields;
    std::size_t start = 0;
    while (start <= line.size()) {
        const auto tab = line.find('\t', start);
        if (tab == std::string_view::npos) {
            fields.push_back(line.substr(start));
            break;
        }
        fields.push_back(line.substr(start, tab - start));
        start = tab + 1;
    }
    return fields;
}

std::uint32_t saturating_size(std::size_t value) {
    constexpr auto kMax = static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max());
    return value > kMax ? std::numeric_limits<std::uint32_t>::max()
                        : static_cast<std::uint32_t>(value);
}

Result<std::string> read_file(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        return std::unexpected(Error(std::format("failed to open {}", path.string())));
    }
    std::ostringstream out;
    out << in.rdbuf();
    if (in.bad()) {
        return std::unexpected(Error(std::format("failed to read {}", path.string())));
    }
    return out.str();
}

bool is_snapshot_filename(std::string_view name) {
    return name.starts_with("tmux_resurrect_") && name.ends_with(".txt");
}

}  // namespace

std::optional<std::filesystem::path> resolve_resurrect_dir(
    std::optional<std::string_view> xdg_data_home, std::optional<std::string_view> home) {
    if (xdg_data_home && !xdg_data_home->empty()) {
        return std::filesystem::path{std::string{*xdg_data_home}} / "tmux" / "resurrect";
    }
    if (home && !home->empty()) {
        return std::filesystem::path{std::string{*home}} / ".local" / "share" / "tmux" /
               "resurrect";
    }
    return std::nullopt;
}

std::optional<std::filesystem::path> default_resurrect_dir() {
    return resolve_resurrect_dir(getenv_view("XDG_DATA_HOME"), getenv_view("HOME"));
}

Result<Snapshot> parse_snapshot_text(std::filesystem::path path, std::string_view text) {
    const auto filename = path.filename().string();
    auto timestamp = parse_snapshot_filename(filename);
    if (!timestamp) {
        return std::unexpected(std::move(timestamp).error());
    }

    std::set<std::string> sessions;
    std::vector<std::pair<std::string, std::string>> windows;
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto nl = text.find('\n', start);
        const auto end = (nl == std::string_view::npos) ? text.size() : nl;
        const auto line = text.substr(start, end - start);
        if (!line.empty()) {
            const auto fields = split_tabs(line);
            if (!fields.empty() && fields.front() == "pane" && fields.size() >= 2) {
                sessions.insert(std::string{fields[1]});
            } else if (!fields.empty() && fields.front() == "window" && fields.size() >= 4) {
                const auto session = std::string{fields[1]};
                auto name = fields[3];
                if (name.starts_with(':')) {
                    name.remove_prefix(1);
                }
                sessions.insert(session);
                windows.emplace_back(session, std::string{name});
            }
        }
        if (nl == std::string_view::npos) {
            break;
        }
        start = nl + 1;
    }

    return Snapshot{
        .path = std::move(path),
        .timestamp = *timestamp,
        .session_count = saturating_size(sessions.size()),
        .window_count = saturating_size(windows.size()),
        .session_names = {sessions.begin(), sessions.end()},
        .windows = std::move(windows),
    };
}

Result<std::vector<Snapshot>> list_snapshots_in(const std::filesystem::path& dir) {
    std::error_code exists_ec;
    if (!std::filesystem::exists(dir, exists_ec)) {
        if (exists_ec) {
            return std::unexpected(Error(std::format(
                "failed to inspect snapshot directory {}: {}", dir.string(), exists_ec.message())));
        }
        return std::vector<Snapshot>{};
    }

    std::error_code status_ec;
    if (!std::filesystem::is_directory(dir, status_ec)) {
        if (status_ec) {
            return std::unexpected(Error(std::format(
                "failed to inspect snapshot directory {}: {}", dir.string(), status_ec.message())));
        }
        return std::vector<Snapshot>{};
    }

    std::error_code iter_ec;
    std::filesystem::directory_iterator it(dir, iter_ec);
    if (iter_ec) {
        return std::unexpected(Error(std::format(
            "failed to read snapshot directory {}: {}", dir.string(), iter_ec.message())));
    }

    std::vector<Snapshot> out;
    const std::filesystem::directory_iterator end;
    for (; it != end; it.increment(iter_ec)) {
        if (iter_ec) {
            return std::unexpected(Error(std::format(
                "failed to iterate snapshot directory {}: {}", dir.string(), iter_ec.message())));
        }

        const auto path = it->path();
        const auto name = path.filename().string();
        if (name == "last" || !is_snapshot_filename(name)) {
            continue;
        }

        auto text = read_file(path);
        if (!text) {
            continue;
        }
        auto snapshot = parse_snapshot_text(path, *text);
        if (snapshot) {
            out.push_back(std::move(*snapshot));
        }
    }

    std::ranges::sort(out, [](const Snapshot& lhs, const Snapshot& rhs) {
        return rhs.timestamp < lhs.timestamp;
    });
    return out;
}

Result<std::vector<Snapshot>> list_snapshots() {
    auto dir = default_resurrect_dir();
    if (!dir) {
        return std::vector<Snapshot>{};
    }
    return list_snapshots_in(*dir);
}

}  // namespace lazytmux::snapshots
