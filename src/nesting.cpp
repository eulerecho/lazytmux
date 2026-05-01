#include <lazytmux/nesting.hpp>

#include <charconv>
#include <cstdint>
#include <optional>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace lazytmux::nesting {

std::uint32_t count_tmux_in_walk(std::uint32_t start_pid, const Walker& walker) {
    std::uint32_t count = 0;
    std::unordered_set<std::uint32_t> visited;
    std::uint32_t pid = start_pid;
    for (std::uint32_t i = 0; i < kMaxWalk; ++i) {
        if (!visited.insert(pid).second) {
            break;
        }
        auto step = walker(pid);
        if (!step) {
            break;
        }
        const auto [parent, is_tmux] = *step;
        if (is_tmux) {
            ++count;
        }
        if (parent == 0 || parent == pid) {
            break;
        }
        pid = parent;
    }
    return count;
}

std::optional<std::uint32_t> parse_ppid(std::string_view status_body) {
    constexpr std::string_view kKey = "PPid:";
    const auto key_pos = status_body.find(kKey);
    if (key_pos == std::string_view::npos) {
        return std::nullopt;
    }
    auto rest = status_body.substr(key_pos + kKey.size());
    while (!rest.empty() && (rest.front() == '\t' || rest.front() == ' ')) {
        rest.remove_prefix(1);
    }
    const auto nl = rest.find('\n');
    auto field = (nl == std::string_view::npos) ? rest : rest.substr(0, nl);

    std::uint32_t value{};
    const auto* first = field.data();
    const auto* last = field.data() + field.size();
    auto [ptr, ec] = std::from_chars(first, last, value);
    if (ec != std::errc{} || ptr != last) {
        return std::nullopt;
    }
    return value;
}

}  // namespace lazytmux::nesting
