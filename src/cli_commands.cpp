#include <lazytmux/cli_commands.hpp>
#include <lazytmux/config.hpp>
#include <lazytmux/error.hpp>
#include <lazytmux/templates.hpp>
#include <lazytmux/tmux/commands.hpp>
#include <lazytmux/version.hpp>

#include <expected>
#include <format>
#include <string>
#include <string_view>
#include <variant>

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

Result<config::LoadResult> load_user_config(const CommandDependencies& dependencies) {
    if (!dependencies.load_config) {
        return std::unexpected(
            Error{ErrorKind::kInternal, "CLI config loader callback is not configured"});
    }
    return dependencies.load_config();
}

void write_config_warning(const OutputSinks& sinks, const config::LoadResult& loaded) {
    if (loaded.warning) {
        write(sinks.err, std::format("warning: {}\n", *loaded.warning));
    }
}

tmux::RunConfig run_config_for(const CliRequest& request) {
    tmux::RunConfig config;
    config.socket = request.socket;
    return config;
}

int config_defaulted_failure(const OutputSinks& sinks, const config::LoadResult& loaded) {
    if (!loaded.used_defaults_due_to_error) {
        return kExitSuccess;
    }
    write(sinks.err, "error: config could not be loaded; refusing to run command with defaults\n");
    return kExitFailure;
}

int run_list_templates(const CommandDependencies& dependencies, const OutputSinks& sinks) {
    auto loaded = load_user_config(dependencies);
    if (!loaded) {
        write_error(sinks, loaded.error());
        return kExitFailure;
    }

    write_config_warning(sinks, *loaded);
    const int config_status = config_defaulted_failure(sinks, *loaded);
    if (config_status != kExitSuccess) {
        return config_status;
    }

    for (const auto& entry : loaded->config.templates.map) {
        write(sinks.out, std::format("{}\n", entry.first.as_str()));
    }
    return kExitSuccess;
}

int run_materialize_template(const CliRequest& request,
                             const MaterializeCommand& command,
                             const CommandDependencies& dependencies,
                             const OutputSinks& sinks) {
    auto loaded = load_user_config(dependencies);
    if (!loaded) {
        write_error(sinks, loaded.error());
        return kExitFailure;
    }

    write_config_warning(sinks, *loaded);
    const int config_status = config_defaulted_failure(sinks, *loaded);
    if (config_status != kExitSuccess) {
        return config_status;
    }

    const auto& templates = loaded->config.templates.map;
    const auto template_it = templates.find(command.template_name);
    if (template_it == templates.end()) {
        write(sinks.err,
              std::format("error: template not found: {}\n", command.template_name.as_str()));
        return kExitFailure;
    }

    auto session = tmux::SessionName::make(command.template_name.as_str());
    if (!session) {
        write_error(sinks,
                    Error{ErrorKind::kInvalidInput,
                          std::format("template name cannot be used as a tmux session name: {}",
                                      session.error().message())});
        return kExitFailure;
    }

    auto materialized = templates::materialize_template(*session,
                                                        template_it->second,
                                                        command.directory,
                                                        dependencies.tmux_runner,
                                                        run_config_for(request));
    if (!materialized) {
        write_error(sinks, materialized.error());
        return kExitFailure;
    }

    return kExitSuccess;
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
                const CommandDependencies& dependencies,
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
            return unsupported_command(request, sinks);
        case CommandKind::kListTemplates:
            return run_list_templates(dependencies, sinks);
        case CommandKind::kMaterialize:
            return run_materialize_template(
                request, std::get<MaterializeCommand>(request.command), dependencies, sinks);
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
