#include <lazytmux/io/process.hpp>
#include <lazytmux/tmux/run.hpp>

#include <format>
#include <span>
#include <string>
#include <vector>

namespace lazytmux::tmux {
namespace {

std::vector<std::string> build_argv(std::span<const std::string> args, const RunConfig& config) {
    std::vector<std::string> argv;
    argv.reserve(args.size() + (config.socket_path ? 3U : 1U));
    argv.push_back(config.executable);
    if (config.socket_path) {
        argv.emplace_back("-S");
        argv.push_back(*config.socket_path);
    }
    argv.insert(argv.end(), args.begin(), args.end());
    return argv;
}

}  // namespace

Result<std::string> run(std::span<const std::string> args, const RunConfig& config) {
    const auto argv = build_argv(args, config);
    auto result = io::run_command(argv);
    if (!result) {
        return std::unexpected(std::move(result).error());
    }
    if (result->exit_code != 0) {
        auto detail = result->stderr_text.empty() ? result->stdout_text : result->stderr_text;
        return std::unexpected(
            Error(std::format("tmux command exited with code {}: {}", result->exit_code, detail)));
    }
    return result->stdout_text;
}

Result<bool> run_status(std::span<const std::string> args, const RunConfig& config) {
    const auto argv = build_argv(args, config);
    auto result = io::run_command_status(argv);
    if (!result) {
        return std::unexpected(std::move(result).error());
    }
    return *result;
}

}  // namespace lazytmux::tmux
