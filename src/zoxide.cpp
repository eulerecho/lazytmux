#include <lazytmux/io/process.hpp>
#include <lazytmux/zoxide.hpp>

#include <charconv>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace lazytmux::zoxide {
namespace {

std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
        s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) {
        s.remove_suffix(1);
    }
    return s;
}

Result<Entry> parse_line(std::string_view line) {
    const auto split = line.find_first_of(" \t");
    if (split == std::string_view::npos) {
        return std::unexpected(Error(std::format("zoxide line missing score/path: \"{}\"", line)));
    }

    const auto score_text = trim(line.substr(0, split));
    const auto path_text = trim(line.substr(split));
    if (path_text.empty()) {
        return std::unexpected(Error(std::format("zoxide line has empty path: \"{}\"", line)));
    }

    double rank{};
    const auto* first = score_text.data();
    const auto* last = score_text.data() + score_text.size();
    auto [ptr, ec] = std::from_chars(first, last, rank);
    if (ec != std::errc{} || ptr != last) {
        return std::unexpected(
            Error(std::format("zoxide line has invalid score \"{}\"", score_text)));
    }

    return Entry{
        .path = std::filesystem::path{std::string{path_text}},
        .rank = rank,
    };
}

}  // namespace

Result<std::vector<Entry>> parse_entries(std::string_view text) {
    std::vector<Entry> entries;
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto nl = text.find('\n', start);
        const auto end = (nl == std::string_view::npos) ? text.size() : nl;
        const auto line = trim(text.substr(start, end - start));
        if (!line.empty()) {
            auto entry = parse_line(line);
            if (!entry) {
                return std::unexpected(std::move(entry).error());
            }
            entries.push_back(std::move(*entry));
        }
        if (nl == std::string_view::npos) {
            break;
        }
        start = nl + 1;
    }
    return entries;
}

Result<std::vector<Entry>> load_top(std::size_t limit, const std::string& executable) {
    const std::vector<std::string> argv{executable, "query", "--list", "--score"};
    auto result = io::run_command(argv);
    if (!result) {
        return std::unexpected(std::move(result).error());
    }
    if (result->exit_code == 127 &&
        result->stderr_text.find("execvp failed") != std::string::npos) {
        return std::vector<Entry>{};
    }
    if (result->exit_code != 0) {
        return std::unexpected(Error{
            ErrorKind::kExternalCommandFailure,
            std::format(
                "zoxide query exited with code {}: {}", result->exit_code, result->stderr_text)});
    }

    auto entries = parse_entries(result->stdout_text);
    if (!entries) {
        return std::unexpected(std::move(entries).error());
    }
    if (entries->size() > limit) {
        entries->resize(limit);
    }
    return entries;
}

}  // namespace lazytmux::zoxide
