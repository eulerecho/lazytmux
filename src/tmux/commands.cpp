#include <lazytmux/error.hpp>
#include <lazytmux/tmux/commands.hpp>
#include <lazytmux/tmux/parse.hpp>
#include <lazytmux/tmux/run.hpp>

#include <charconv>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace lazytmux::tmux {
namespace {

std::string to_string(std::string_view value) {
    return std::string{value};
}

Result<std::string> call_run(const CommandRunner& runner,
                             std::span<const std::string> args,
                             const RunConfig& config,
                             std::string context) {
    if (!runner.run) {
        return std::unexpected(
            Error{ErrorKind::kInternal, "tmux command runner output callback is not configured"});
    }
    auto out = runner.run(args, config);
    if (!out) {
        auto err = std::move(out).error();
        err.with_context(std::move(context));
        return std::unexpected(std::move(err));
    }
    return out;
}

Result<bool> call_status(const CommandRunner& runner,
                         std::span<const std::string> args,
                         const RunConfig& config,
                         std::string context) {
    if (!runner.run_status) {
        return std::unexpected(
            Error{ErrorKind::kInternal, "tmux command runner status callback is not configured"});
    }
    auto ok = runner.run_status(args, config);
    if (!ok) {
        auto err = std::move(ok).error();
        err.with_context(std::move(context));
        return std::unexpected(std::move(err));
    }
    return ok;
}

Result<void> call_mutation(const CommandRunner& runner,
                           std::span<const std::string> args,
                           const RunConfig& config,
                           std::string context) {
    auto ok = call_status(runner, args, config, context);
    if (!ok) {
        return std::unexpected(std::move(ok).error());
    }
    if (!*ok) {
        return std::unexpected(Error{
            ErrorKind::kExternalCommandFailure,
            std::format("{} failed", context),
        });
    }
    return {};
}

template <typename T>
Result<T> add_parse_context(Result<T> parsed, std::string context) {
    if (!parsed) {
        auto err = std::move(parsed).error();
        err.with_context(std::move(context));
        return std::unexpected(std::move(err));
    }
    return parsed;
}

std::string_view trim_ascii_whitespace(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' ||
                              value.front() == '\n' || value.front() == '\r')) {
        value.remove_prefix(1);
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\n' ||
                              value.back() == '\r')) {
        value.remove_suffix(1);
    }
    return value;
}

std::string target_to_string(const SwitchTarget& target) {
    return std::visit([](const auto& value) { return to_string(value.as_str()); }, target);
}

std::string session_window_target(const SessionName& session) {
    auto target = to_string(session.as_str());
    target.push_back(':');
    return target;
}

std::string split_flag(SplitDirection direction) {
    switch (direction) {
        case SplitDirection::kHorizontal:
            return "-h";
        case SplitDirection::kVertical:
            return "-v";
    }
    return "-h";
}

std::string rotate_flag(RotateDirection direction) {
    switch (direction) {
        case RotateDirection::kDown:
            return "-D";
        case RotateDirection::kUp:
            return "-U";
    }
    return "-D";
}

std::string layout_name(Layout layout) {
    switch (layout) {
        case Layout::kEvenHorizontal:
            return "even-horizontal";
        case Layout::kEvenVertical:
            return "even-vertical";
        case Layout::kMainHorizontal:
            return "main-horizontal";
        case Layout::kMainVertical:
            return "main-vertical";
        case Layout::kTiled:
            return "tiled";
    }
    return "tiled";
}

Result<std::int64_t> parse_unix_timestamp(std::string_view value) {
    std::int64_t timestamp{};
    const auto* first = value.data();
    const auto* last = value.data() + value.size();
    auto [ptr, ec] = std::from_chars(first, last, timestamp);
    if (ec != std::errc{} || ptr != last) {
        return std::unexpected(Error{
            ErrorKind::kParse,
            std::format("invalid continuum last-save timestamp: \"{}\"", value),
        });
    }
    return timestamp;
}

}  // namespace

CommandRunner default_command_runner() {
    return CommandRunner{
        .run = [](std::span<const std::string> args, const RunConfig& config)
            -> Result<std::string> { return tmux::run(args, config); },
        .run_status = [](std::span<const std::string> args, const RunConfig& config)
            -> Result<bool> { return tmux::run_status(args, config); },
    };
}

Result<std::vector<Session>> list_sessions(const RunConfig& config) {
    return list_sessions(default_command_runner(), config);
}

Result<std::vector<Session>> list_sessions(const CommandRunner& runner, const RunConfig& config) {
    const std::vector<std::string> args{"list-sessions", "-F", to_string(kSessionFormat)};
    auto out = call_run(runner, args, config, "tmux list-sessions");
    if (!out) {
        return std::unexpected(std::move(out).error());
    }
    return add_parse_context(parse_sessions(*out), "parse list-sessions output");
}

Result<std::vector<Window>> list_windows(const SessionName& session, const RunConfig& config) {
    return list_windows(session, default_command_runner(), config);
}

Result<std::vector<Window>> list_windows(const SessionName& session,
                                         const CommandRunner& runner,
                                         const RunConfig& config) {
    const std::vector<std::string> args{
        "list-windows", "-t", to_string(session.as_str()), "-F", to_string(kWindowFormat)};
    auto out = call_run(runner, args, config, "tmux list-windows");
    if (!out) {
        return std::unexpected(std::move(out).error());
    }
    return add_parse_context(parse_windows(*out), "parse list-windows output");
}

Result<std::vector<Window>> list_windows_all(const RunConfig& config) {
    return list_windows_all(default_command_runner(), config);
}

Result<std::vector<Window>> list_windows_all(const CommandRunner& runner, const RunConfig& config) {
    const std::vector<std::string> args{"list-windows", "-a", "-F", to_string(kWindowFormat)};
    auto out = call_run(runner, args, config, "tmux list-windows -a");
    if (!out) {
        return std::unexpected(std::move(out).error());
    }
    return add_parse_context(parse_windows(*out), "parse list-windows output");
}

Result<std::vector<Pane>> list_panes(const WindowId& window, const RunConfig& config) {
    return list_panes(window, default_command_runner(), config);
}

Result<std::vector<Pane>> list_panes(const WindowId& window,
                                     const CommandRunner& runner,
                                     const RunConfig& config) {
    const std::vector<std::string> args{
        "list-panes", "-t", to_string(window.as_str()), "-F", to_string(kPaneFormat)};
    auto out = call_run(runner, args, config, "tmux list-panes");
    if (!out) {
        return std::unexpected(std::move(out).error());
    }
    return add_parse_context(parse_panes(*out), "parse list-panes output");
}

Result<std::vector<Pane>> list_panes_all(const RunConfig& config) {
    return list_panes_all(default_command_runner(), config);
}

Result<std::vector<Pane>> list_panes_all(const CommandRunner& runner, const RunConfig& config) {
    const std::vector<std::string> args{"list-panes", "-a", "-F", to_string(kPaneFormat)};
    auto out = call_run(runner, args, config, "tmux list-panes -a");
    if (!out) {
        return std::unexpected(std::move(out).error());
    }
    return add_parse_context(parse_panes(*out), "parse list-panes output");
}

Result<std::vector<Client>> list_clients(const RunConfig& config) {
    return list_clients(default_command_runner(), config);
}

Result<std::vector<Client>> list_clients(const CommandRunner& runner, const RunConfig& config) {
    const std::vector<std::string> args{"list-clients", "-F", to_string(kClientFormat)};
    auto out = call_run(runner, args, config, "tmux list-clients");
    if (!out) {
        return std::unexpected(std::move(out).error());
    }
    return add_parse_context(parse_clients(*out), "parse list-clients output");
}

Result<ServerInfo> server_info(const RunConfig& config) {
    return server_info(default_command_runner(), config);
}

Result<ServerInfo> server_info(const CommandRunner& runner, const RunConfig& config) {
    const std::vector<std::string> args{"display-message", "-p", to_string(kServerInfoFormat)};
    auto out = call_run(runner, args, config, "tmux display-message server info");
    if (!out) {
        return std::unexpected(std::move(out).error());
    }
    return add_parse_context(parse_server_info(*out), "parse server-info output");
}

Result<std::string> capture_pane(const PaneId& pane, const RunConfig& config) {
    return capture_pane(pane, default_command_runner(), config);
}

Result<std::string> capture_pane(const PaneId& pane,
                                 const CommandRunner& runner,
                                 const RunConfig& config) {
    const std::vector<std::string> args{
        "capture-pane", "-p", "-t", to_string(pane.as_str()), "-S", "-"};
    return call_run(runner, args, config, "tmux capture-pane");
}

Result<std::optional<std::int64_t>> continuum_last_save(const RunConfig& config) {
    return continuum_last_save(default_command_runner(), config);
}

Result<std::optional<std::int64_t>> continuum_last_save(const CommandRunner& runner,
                                                        const RunConfig& config) {
    const std::vector<std::string> args{"show-option", "-gqv", to_string(kContinuumLastSaveOption)};
    auto out = call_run(runner, args, config, "tmux show continuum last-save option");
    if (!out) {
        return std::unexpected(std::move(out).error());
    }

    auto trimmed = trim_ascii_whitespace(*out);
    if (trimmed.empty()) {
        return std::optional<std::int64_t>{};
    }

    auto parsed = parse_unix_timestamp(trimmed);
    if (!parsed) {
        auto err = std::move(parsed).error();
        err.with_context("parse continuum last-save option");
        return std::unexpected(std::move(err));
    }
    return std::optional<std::int64_t>{*parsed};
}

Result<void> switch_client(const SwitchTarget& target, const RunConfig& config) {
    return switch_client(target, default_command_runner(), config);
}

Result<void> switch_client(const SwitchTarget& target,
                           const CommandRunner& runner,
                           const RunConfig& config) {
    const std::vector<std::string> args{"switch-client", "-t", target_to_string(target)};
    return call_mutation(runner, args, config, "tmux switch-client");
}

Result<void> detach_client(const ClientTty& tty, const RunConfig& config) {
    return detach_client(tty, default_command_runner(), config);
}

Result<void> detach_client(const ClientTty& tty,
                           const CommandRunner& runner,
                           const RunConfig& config) {
    const std::vector<std::string> args{"detach-client", "-t", to_string(tty.as_str())};
    return call_mutation(runner, args, config, "tmux detach-client");
}

Result<bool> session_exists(const SessionName& name, const RunConfig& config) {
    return session_exists(name, default_command_runner(), config);
}

Result<bool> session_exists(const SessionName& name,
                            const CommandRunner& runner,
                            const RunConfig& config) {
    const std::vector<std::string> args{"has-session", "-t", to_string(name.as_str())};
    return call_status(runner, args, config, "tmux has-session");
}

Result<void> new_session(const SessionName& name,
                         const std::filesystem::path& path,
                         bool detached,
                         const RunConfig& config) {
    return new_session(name, path, detached, default_command_runner(), config);
}

Result<void> new_session(const SessionName& name,
                         const std::filesystem::path& path,
                         bool detached,
                         const CommandRunner& runner,
                         const RunConfig& config) {
    std::vector<std::string> args{"new-session"};
    if (detached) {
        args.emplace_back("-d");
    }
    args.emplace_back("-s");
    args.emplace_back(to_string(name.as_str()));
    args.emplace_back("-c");
    args.emplace_back(path.string());
    return call_mutation(runner, args, config, "tmux new-session");
}

Result<void> rename_session(const SessionName& old_name,
                            const SessionName& new_name,
                            const RunConfig& config) {
    return rename_session(old_name, new_name, default_command_runner(), config);
}

Result<void> rename_session(const SessionName& old_name,
                            const SessionName& new_name,
                            const CommandRunner& runner,
                            const RunConfig& config) {
    const std::vector<std::string> args{
        "rename-session", "-t", to_string(old_name.as_str()), to_string(new_name.as_str())};
    return call_mutation(runner, args, config, "tmux rename-session");
}

Result<void> kill_session(const SessionName& name, const RunConfig& config) {
    return kill_session(name, default_command_runner(), config);
}

Result<void> kill_session(const SessionName& name,
                          const CommandRunner& runner,
                          const RunConfig& config) {
    const std::vector<std::string> args{"kill-session", "-t", to_string(name.as_str())};
    return call_mutation(runner, args, config, "tmux kill-session");
}

Result<void> new_window(const SessionName& session,
                        std::string name,
                        const std::filesystem::path& path,
                        const RunConfig& config) {
    return new_window(session, std::move(name), path, default_command_runner(), config);
}

Result<void> new_window(const SessionName& session,
                        std::string name,
                        const std::filesystem::path& path,
                        const CommandRunner& runner,
                        const RunConfig& config) {
    const std::vector<std::string> args{"new-window",
                                        "-t",
                                        to_string(session.as_str()),
                                        "-n",
                                        std::move(name),
                                        "-c",
                                        path.string()};
    return call_mutation(runner, args, config, "tmux new-window");
}

Result<void> rename_window(const WindowId& window, std::string name, const RunConfig& config) {
    return rename_window(window, std::move(name), default_command_runner(), config);
}

Result<void> rename_window(const WindowId& window,
                           std::string name,
                           const CommandRunner& runner,
                           const RunConfig& config) {
    const std::vector<std::string> args{
        "rename-window", "-t", to_string(window.as_str()), std::move(name)};
    return call_mutation(runner, args, config, "tmux rename-window");
}

Result<void> kill_window(const WindowId& window, const RunConfig& config) {
    return kill_window(window, default_command_runner(), config);
}

Result<void> kill_window(const WindowId& window,
                         const CommandRunner& runner,
                         const RunConfig& config) {
    const std::vector<std::string> args{"kill-window", "-t", to_string(window.as_str())};
    return call_mutation(runner, args, config, "tmux kill-window");
}

Result<void> swap_windows(const WindowId& lhs, const WindowId& rhs, const RunConfig& config) {
    return swap_windows(lhs, rhs, default_command_runner(), config);
}

Result<void> swap_windows(const WindowId& lhs,
                          const WindowId& rhs,
                          const CommandRunner& runner,
                          const RunConfig& config) {
    const std::vector<std::string> args{
        "swap-window", "-s", to_string(lhs.as_str()), "-t", to_string(rhs.as_str())};
    return call_mutation(runner, args, config, "tmux swap-window");
}

Result<void> move_window(const WindowId& window,
                         const SessionName& destination_session,
                         const RunConfig& config) {
    return move_window(window, destination_session, default_command_runner(), config);
}

Result<void> move_window(const WindowId& window,
                         const SessionName& destination_session,
                         const CommandRunner& runner,
                         const RunConfig& config) {
    const std::vector<std::string> args{"move-window",
                                        "-s",
                                        to_string(window.as_str()),
                                        "-t",
                                        session_window_target(destination_session)};
    return call_mutation(runner, args, config, "tmux move-window");
}

Result<void> split_pane(const PaneId& pane,
                        const std::filesystem::path& path,
                        SplitDirection direction,
                        const RunConfig& config) {
    return split_pane(pane, path, direction, default_command_runner(), config);
}

Result<void> split_pane(const PaneId& pane,
                        const std::filesystem::path& path,
                        SplitDirection direction,
                        const CommandRunner& runner,
                        const RunConfig& config) {
    const std::vector<std::string> args{
        "split-window", split_flag(direction), "-t", to_string(pane.as_str()), "-c", path.string()};
    return call_mutation(runner, args, config, "tmux split-window");
}

Result<void> kill_pane(const PaneId& pane, const RunConfig& config) {
    return kill_pane(pane, default_command_runner(), config);
}

Result<void> kill_pane(const PaneId& pane, const CommandRunner& runner, const RunConfig& config) {
    const std::vector<std::string> args{"kill-pane", "-t", to_string(pane.as_str())};
    return call_mutation(runner, args, config, "tmux kill-pane");
}

Result<void> rotate_panes(const WindowId& window,
                          RotateDirection direction,
                          const RunConfig& config) {
    return rotate_panes(window, direction, default_command_runner(), config);
}

Result<void> rotate_panes(const WindowId& window,
                          RotateDirection direction,
                          const CommandRunner& runner,
                          const RunConfig& config) {
    const std::vector<std::string> args{
        "rotate-window", rotate_flag(direction), "-t", to_string(window.as_str())};
    return call_mutation(runner, args, config, "tmux rotate-window");
}

Result<void> break_pane(const PaneId& pane, const RunConfig& config) {
    return break_pane(pane, default_command_runner(), config);
}

Result<void> break_pane(const PaneId& pane, const CommandRunner& runner, const RunConfig& config) {
    const std::vector<std::string> args{"break-pane", "-t", to_string(pane.as_str())};
    return call_mutation(runner, args, config, "tmux break-pane");
}

Result<void> join_pane(const PaneId& pane, const WindowId& destination, const RunConfig& config) {
    return join_pane(pane, destination, default_command_runner(), config);
}

Result<void> join_pane(const PaneId& pane,
                       const WindowId& destination,
                       const CommandRunner& runner,
                       const RunConfig& config) {
    const std::vector<std::string> args{
        "join-pane", "-s", to_string(pane.as_str()), "-t", to_string(destination.as_str())};
    return call_mutation(runner, args, config, "tmux join-pane");
}

Result<void> select_layout(const WindowId& window, Layout layout, const RunConfig& config) {
    return select_layout(window, layout, default_command_runner(), config);
}

Result<void> select_layout(const WindowId& window,
                           Layout layout,
                           const CommandRunner& runner,
                           const RunConfig& config) {
    const std::vector<std::string> args{
        "select-layout", "-t", to_string(window.as_str()), layout_name(layout)};
    return call_mutation(runner, args, config, "tmux select-layout");
}

Result<void> toggle_zoom(const PaneId& pane, const RunConfig& config) {
    return toggle_zoom(pane, default_command_runner(), config);
}

Result<void> toggle_zoom(const PaneId& pane, const CommandRunner& runner, const RunConfig& config) {
    const std::vector<std::string> args{"resize-pane", "-Z", "-t", to_string(pane.as_str())};
    return call_mutation(runner, args, config, "tmux resize-pane -Z");
}

Result<void> send_keys(const PaneId& pane,
                       std::string keys,
                       SendEnter enter,
                       const RunConfig& config) {
    return send_keys(pane, std::move(keys), enter, default_command_runner(), config);
}

Result<void> send_keys(const PaneId& pane,
                       std::string keys,
                       SendEnter enter,
                       const CommandRunner& runner,
                       const RunConfig& config) {
    std::vector<std::string> args{
        "send-keys", "-t", to_string(pane.as_str()), "--", std::move(keys)};
    if (enter == SendEnter::kYes) {
        args.emplace_back("Enter");
    }
    return call_mutation(runner, args, config, "tmux send-keys");
}

}  // namespace lazytmux::tmux
