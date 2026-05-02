#include <lazytmux/cli.hpp>

#include <format>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace lazytmux::cli {
namespace {

[[nodiscard]] Error usage_error(std::string message) {
    return Error{ErrorKind::kInvalidInput, std::move(message)};
}

[[nodiscard]] bool is_global_option(std::string_view arg) noexcept {
    return arg == "--help" || arg == "--version" || arg == "-L" || arg == "-S";
}

[[nodiscard]] Result<std::string_view> require_value(std::span<const std::string_view> args,
                                                     std::size_t& index,
                                                     std::string_view option) {
    if (index + 1 >= args.size()) {
        return std::unexpected(usage_error(std::format("{} requires a value", option)));
    }

    ++index;
    if (args[index].empty()) {
        return std::unexpected(usage_error(std::format("{} requires a non-empty value", option)));
    }
    return args[index];
}

[[nodiscard]] Result<void> reject_global_option_after_command(
    std::span<const std::string_view> args) {
    for (const std::string_view arg : args) {
        if (is_global_option(arg)) {
            return std::unexpected(
                usage_error(std::format("{} must appear before the subcommand", arg)));
        }
    }
    return {};
}

[[nodiscard]] Result<CliRequest> make_request(tmux::SocketSelection socket,
                                              Result<Command> command) {
    if (!command) {
        return std::unexpected(std::move(command).error());
    }
    return CliRequest{.socket = std::move(socket), .command = std::move(*command)};
}

[[nodiscard]] Result<Command> parse_attach(std::span<const std::string_view> args) {
    auto global_option = reject_global_option_after_command(args);
    if (!global_option) {
        return std::unexpected(std::move(global_option).error());
    }
    if (args.size() != 1) {
        return std::unexpected(usage_error("attach expects exactly one session"));
    }

    auto session = tmux::SessionName::make(args[0]);
    if (!session) {
        return std::unexpected(
            usage_error(std::format("invalid session name: {}", session.error().message())));
    }
    return AttachCommand{.session = std::move(*session)};
}

[[nodiscard]] Result<Command> parse_save(std::span<const std::string_view> args) {
    auto global_option = reject_global_option_after_command(args);
    if (!global_option) {
        return std::unexpected(std::move(global_option).error());
    }
    if (!args.empty()) {
        return std::unexpected(usage_error("save does not accept positional arguments"));
    }
    return SaveCommand{};
}

[[nodiscard]] Result<Command> parse_list_templates(std::span<const std::string_view> args) {
    auto global_option = reject_global_option_after_command(args);
    if (!global_option) {
        return std::unexpected(std::move(global_option).error());
    }
    if (!args.empty()) {
        return std::unexpected(usage_error("list-templates does not accept positional arguments"));
    }
    return ListTemplatesCommand{};
}

[[nodiscard]] Result<Command> parse_materialize(std::span<const std::string_view> args) {
    auto global_option = reject_global_option_after_command(args);
    if (!global_option) {
        return std::unexpected(std::move(global_option).error());
    }
    if (args.size() != 2) {
        return std::unexpected(usage_error("materialize expects a template name and directory"));
    }

    auto template_name = tmux::TemplateName::make(args[0]);
    if (!template_name) {
        return std::unexpected(
            usage_error(std::format("invalid template name: {}", template_name.error().message())));
    }
    if (args[1].empty()) {
        return std::unexpected(usage_error("materialize directory must be non-empty"));
    }

    return MaterializeCommand{
        .template_name = std::move(*template_name),
        .directory = std::filesystem::path{std::string{args[1]}},
    };
}

}  // namespace

Result<CliRequest> parse_args(std::span<const std::string_view> args) {
    tmux::SocketSelection socket = tmux::SocketSelection::default_socket();
    bool saw_socket_option = false;

    std::size_t index = 0;
    for (; index < args.size(); ++index) {
        const std::string_view arg = args[index];
        if (arg == "--help") {
            if (index + 1 != args.size()) {
                return std::unexpected(usage_error("--help does not accept arguments"));
            }
            return CliRequest{.socket = std::move(socket), .command = HelpCommand{}};
        }
        if (arg == "--version") {
            if (index + 1 != args.size()) {
                return std::unexpected(usage_error("--version does not accept arguments"));
            }
            return CliRequest{.socket = std::move(socket), .command = VersionCommand{}};
        }
        if (arg == "-L" || arg == "-S") {
            if (saw_socket_option) {
                return std::unexpected(usage_error("only one tmux socket option may be used"));
            }

            auto value_result = require_value(args, index, arg);
            if (!value_result) {
                return std::unexpected(std::move(value_result).error());
            }
            const std::string value{*value_result};
            socket = arg == "-L" ? tmux::SocketSelection::named(value)
                                 : tmux::SocketSelection::path(value);
            saw_socket_option = true;
            continue;
        }
        if (!arg.empty() && arg.front() == '-') {
            return std::unexpected(usage_error(std::format("unknown option {}", arg)));
        }
        break;
    }

    if (index == args.size()) {
        return CliRequest{.socket = std::move(socket), .command = HelpCommand{}};
    }

    const std::string_view command = args[index];
    ++index;
    const auto command_args = args.subspan(index);

    if (command == "attach") {
        return make_request(std::move(socket), parse_attach(command_args));
    }
    if (command == "save") {
        return make_request(std::move(socket), parse_save(command_args));
    }
    if (command == "list-templates") {
        return make_request(std::move(socket), parse_list_templates(command_args));
    }
    if (command == "materialize") {
        return make_request(std::move(socket), parse_materialize(command_args));
    }

    return std::unexpected(usage_error(std::format("unknown subcommand {}", command)));
}

std::string usage() {
    return "usage: lazytmux [--help] [--version] [-L name | -S path] <command> [args]\n"
           "\n"
           "commands:\n"
           "  attach <session>\n"
           "  save\n"
           "  list-templates\n"
           "  materialize <template> <dir>\n";
}

std::string command_usage(CommandKind kind) {
    switch (kind) {
        case CommandKind::kHelp:
            return usage();
        case CommandKind::kVersion:
            return "usage: lazytmux --version\n";
        case CommandKind::kAttach:
            return "usage: lazytmux [-L name | -S path] attach <session>\n";
        case CommandKind::kSave:
            return "usage: lazytmux [-L name | -S path] save\n";
        case CommandKind::kListTemplates:
            return "usage: lazytmux list-templates\n";
        case CommandKind::kMaterialize:
            return "usage: lazytmux [-L name | -S path] materialize <template> <dir>\n";
    }

    return usage();
}

CommandKind command_kind(const Command& command) noexcept {
    return std::visit(
        [](const auto& payload) noexcept -> CommandKind {
            using T = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<T, HelpCommand>) {
                return CommandKind::kHelp;
            } else if constexpr (std::is_same_v<T, VersionCommand>) {
                return CommandKind::kVersion;
            } else if constexpr (std::is_same_v<T, AttachCommand>) {
                return CommandKind::kAttach;
            } else if constexpr (std::is_same_v<T, SaveCommand>) {
                return CommandKind::kSave;
            } else if constexpr (std::is_same_v<T, ListTemplatesCommand>) {
                return CommandKind::kListTemplates;
            } else {
                return CommandKind::kMaterialize;
            }
        },
        command);
}

}  // namespace lazytmux::cli
