#include <lazytmux/tmux/socket.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace lazytmux::tmux {

std::optional<std::string> parse_tmux_socket(std::string_view raw) {
    const auto comma = raw.find(',');
    const auto path = (comma == std::string_view::npos) ? raw : raw.substr(0, comma);
    if (path.empty()) {
        return std::nullopt;
    }
    return std::string{path};
}

}  // namespace lazytmux::tmux
