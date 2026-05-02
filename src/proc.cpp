#include <lazytmux/proc.hpp>

#include <format>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

namespace lazytmux::proc {
namespace {

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

}  // namespace

Result<ProcessInfo> read_process_info(std::uint32_t pid, const std::filesystem::path& proc_root) {
    const auto dir = proc_root / std::to_string(pid);
    auto status = read_file(dir / "status");
    if (!status) {
        return std::unexpected(std::move(status).error());
    }
    auto ppid = nesting::parse_ppid(*status);
    if (!ppid) {
        return std::unexpected(Error(std::format("failed to parse PPid for process {}", pid)));
    }

    std::error_code exe_ec;
    const auto exe = std::filesystem::read_symlink(dir / "exe", exe_ec);
    if (exe_ec) {
        return std::unexpected(Error(
            std::format("failed to read executable for process {}: {}", pid, exe_ec.message())));
    }

    return ProcessInfo{
        .parent_pid = *ppid,
        .is_tmux = exe.filename() == "tmux",
    };
}

nesting::Walker make_proc_walker(std::filesystem::path proc_root) {
    return [root = std::move(proc_root)](
               std::uint32_t pid) -> std::optional<std::pair<std::uint32_t, bool>> {
        auto info = read_process_info(pid, root);
        if (!info) {
            return std::nullopt;
        }
        return std::pair{info->parent_pid, info->is_tmux};
    };
}

}  // namespace lazytmux::proc
