#include <lazytmux/config.hpp>
#include <lazytmux/error.hpp>
#include <lazytmux/templates.hpp>
#include <lazytmux/tmux/commands.hpp>
#include <lazytmux/tmux/ids.hpp>
#include <lazytmux/tmux/run.hpp>

#include <filesystem>
#include <format>
#include <string>
#include <utility>

namespace lazytmux::templates {
namespace {

std::string window_context(std::size_t index, const config::TemplateWindow& window) {
    return std::format("template window {} ({})", index, window.name);
}

template <typename T>
Result<T> add_context(Result<T> result, std::string context) {
    if (!result) {
        auto err = std::move(result).error();
        err.with_context(std::move(context));
        return std::unexpected(std::move(err));
    }
    return result;
}

Result<void> maybe_send_initial_command(const tmux::PaneId& pane,
                                        const config::TemplateWindow& window,
                                        std::size_t index,
                                        const tmux::CommandRunner& runner,
                                        const tmux::RunConfig& config) {
    if (window.command.empty()) {
        return {};
    }
    return add_context(tmux::send_keys(pane, window.command, tmux::SendEnter::kYes, runner, config),
                       window_context(index, window));
}

}  // namespace

Result<void> materialize_template(const tmux::SessionName& session,
                                  const config::Template& tmpl,
                                  const std::filesystem::path& cwd,
                                  const tmux::RunConfig& config) {
    return materialize_template(session, tmpl, cwd, tmux::default_command_runner(), config);
}

Result<void> materialize_template(const tmux::SessionName& session,
                                  const config::Template& tmpl,
                                  const std::filesystem::path& cwd,
                                  const tmux::CommandRunner& runner,
                                  const tmux::RunConfig& config) {
    if (tmpl.windows.empty()) {
        return std::unexpected(
            Error{ErrorKind::kInvalidInput, "template must contain at least one window"});
    }

    auto exists = tmux::session_exists(session, runner, config);
    if (!exists) {
        auto err = std::move(exists).error();
        err.with_context("check target session");
        return std::unexpected(std::move(err));
    }
    if (*exists) {
        return std::unexpected(Error{
            ErrorKind::kInvalidInput,
            std::format("target session already exists: {}", session.as_str()),
        });
    }

    const auto& first_window = tmpl.windows.front();
    auto first_created = add_context(
        tmux::new_session_with_ids(session, first_window.name, cwd, true, runner, config),
        window_context(0, first_window));
    if (!first_created) {
        return std::unexpected(std::move(first_created).error());
    }

    auto first_command =
        maybe_send_initial_command(first_created->pane_id, first_window, 0, runner, config);
    if (!first_command) {
        return std::unexpected(std::move(first_command).error());
    }

    for (std::size_t i = 1; i < tmpl.windows.size(); ++i) {
        const auto& window = tmpl.windows[i];
        auto created =
            add_context(tmux::new_window_with_ids(session, window.name, cwd, runner, config),
                        window_context(i, window));
        if (!created) {
            return std::unexpected(std::move(created).error());
        }

        auto command = maybe_send_initial_command(created->pane_id, window, i, runner, config);
        if (!command) {
            return std::unexpected(std::move(command).error());
        }
    }

    return {};
}

}  // namespace lazytmux::templates
