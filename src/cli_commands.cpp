#include <lazytmux/cli_commands.hpp>

#include <lazytmux/config.hpp>
#include <lazytmux/error.hpp>
#include <lazytmux/tmux/commands.hpp>
#include <lazytmux/version.hpp>

#include <format>
#include <string>
#include <string_view>

namespace lazytmux::cli {
namespace {

void write(const std::function<void(std::string_view)>& sink, std::string_view text) {
    if (sink) {
        sink(text);
    }
}

std::string error_text(const Error& error) {
    std::string text{error.message()};
    for (const auto& context : error.contexts()) {
        text += std::format(" [{}]", context);
    }
    return text;
}

void write_error(const OutputSinks& sinks, const Error& error) {
    write(sinks.err, std::format("error: {}\n", error_text(error)));
}

void write_usage_error(const OutputSinks& sinks, const Error& error) {
    write_error(sinks, error);
    write(sinks.err, "\n");
    write(sinks.err, usage());
}

std::string command_name(CommandKind kind) {
    switch (kind) {
        case CommandKind::kHelp:
            return "help";
        case CommandKind::kVersion:
            return "version";
        case CommandKind::kAttach:
            return "attach";
        case CommandKind::kSave:
            return "save";
        case CommandKind::kListTemplates:
            return "list-templates";
        case CommandKind::kMaterialize:
            return "materialize";
    }
    return "command";
}

int unsupported_command(const CliRequest& request, const OutputSinks& sinks) {
    const auto kind = command_kind(request.command);
    write(sinks.err, std::format("error: {} is not implemented yet\n", command_name(kind)));
    return kExitFailure;
}

}  // namespace

CommandDependencies default_command_dependencies() {
    return CommandDependencies{
        .load_config = [] { return config::load_config(); },
        .tmux_runner = tmux::default_command_runner(),
    };
}

int run_request(const CliRequest& request,
                const CommandDependencies&,
                const OutputSinks& sinks) {
    const auto kind = command_kind(request.command);
    switch (kind) {
        case CommandKind::kHelp:
            write(sinks.out, usage());
            return kExitSuccess;
        case CommandKind::kVersion:
            write(sinks.out, std::format("lazytmux {}\n", kVersion));
            return kExitSuccess;
        case CommandKind::kAttach:
        case CommandKind::kSave:
        case CommandKind::kListTemplates:
        case CommandKind::kMaterialize:
            return unsupported_command(request, sinks);
    }
    return unsupported_command(request, sinks);
}

int run_args(std::span<const std::string_view> args,
             const CommandDependencies& dependencies,
             const OutputSinks& sinks) {
    auto request = parse_args(args);
    if (!request) {
        write_usage_error(sinks, request.error());
        return kExitUsage;
    }
    return run_request(*request, dependencies, sinks);
}

}  // namespace lazytmux::cli
