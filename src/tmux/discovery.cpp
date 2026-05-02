#include <lazytmux/tmux/discovery.hpp>

#include <algorithm>
#include <filesystem>
#include <format>
#include <string>
#include <system_error>
#include <vector>

namespace lazytmux::tmux {

Result<std::vector<SocketInfo>> list_sockets_in(const std::filesystem::path& dir) {
    std::error_code ec;
    std::filesystem::directory_iterator it(dir, ec);
    if (ec) {
        return std::unexpected(Error(
            std::format("failed to read socket directory {}: {}", dir.string(), ec.message())));
    }

    std::vector<SocketInfo> out;
    const std::filesystem::directory_iterator end;
    for (; it != end; it.increment(ec)) {
        if (ec) {
            return std::unexpected(Error(std::format(
                "failed to iterate socket directory {}: {}", dir.string(), ec.message())));
        }
        const auto status = it->symlink_status(ec);
        if (ec) {
            return std::unexpected(
                Error(std::format("failed to inspect {}: {}", it->path().string(), ec.message())));
        }
        if (std::filesystem::is_socket(status)) {
            out.push_back(SocketInfo{
                .path = it->path(),
                .name = it->path().filename().string(),
            });
        }
    }

    std::ranges::sort(out, {}, [](const SocketInfo& s) { return s.path.string(); });
    return out;
}

std::filesystem::path socket_path_for_name(std::uint32_t uid, std::string_view name) {
    return std::filesystem::path{"/tmp"} / std::format("tmux-{}", uid) / std::string{name};
}

}  // namespace lazytmux::tmux
