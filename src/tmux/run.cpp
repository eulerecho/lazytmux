#include <lazytmux/io/process.hpp>
#include <lazytmux/tmux/run.hpp>

#include <format>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace lazytmux::tmux {
std::vector<std::string> argv_for(std::span<const std::string> args, const RunConfig& config) {
    std::vector<std::string> argv;
    argv.reserve(args.size() + 3U);
    argv.push_back(config.executable);

    switch (config.socket.kind) {
        case SocketSelectionKind::kDefault:
            break;
        case SocketSelectionKind::kNamed:
            argv.emplace_back("-L");
            argv.push_back(config.socket.value);
            break;
        case SocketSelectionKind::kPath:
            argv.emplace_back("-S");
            argv.push_back(config.socket.value);
            break;
    }

    argv.insert(argv.end(), args.begin(), args.end());
    return argv;
}

SocketSelection SocketSelection::default_socket() {
    return {};
}

SocketSelection SocketSelection::named(std::string name) {
    return {.kind = SocketSelectionKind::kNamed, .value = std::move(name)};
}

SocketSelection SocketSelection::path(std::string path) {
    return {.kind = SocketSelectionKind::kPath, .value = std::move(path)};
}

Result<std::string> run(std::span<const std::string> args, const RunConfig& config) {
    const auto argv = argv_for(args, config);
    auto result = io::run_command(argv, config.command_options);
    if (!result) {
        return std::unexpected(std::move(result).error());
    }
    if (result->exit_code != 0) {
        auto detail = result->stderr_text.empty() ? result->stdout_text : result->stderr_text;
        return std::unexpected(
            Error{ErrorKind::kExternalCommandFailure,
                  std::format("tmux command exited with code {}: {}", result->exit_code, detail)});
    }
    return result->stdout_text;
}

Result<bool> run_status(std::span<const std::string> args, const RunConfig& config) {
    const auto argv = argv_for(args, config);
    auto result = io::run_command_status(argv, config.command_options);
    if (!result) {
        return std::unexpected(std::move(result).error());
    }
    return *result;
}

}  // namespace lazytmux::tmux
