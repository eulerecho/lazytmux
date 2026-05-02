#pragma once

#include <lazytmux/config.hpp>
#include <lazytmux/error.hpp>
#include <lazytmux/tmux/commands.hpp>
#include <lazytmux/tmux/ids.hpp>
#include <lazytmux/tmux/run.hpp>

#include <filesystem>

/// @file
/// @brief Session-template materialization over the tmux command library.

namespace lazytmux::templates {

/// @brief Materialize a parsed session template into a new tmux session.
/// @param session Target session name. The session must not already exist.
/// @param tmpl Parsed template to materialize.
/// @param cwd Working directory for every created template window.
/// @param config tmux executable, socket selector, and process limits.
/// @return Success after all windows and configured commands are created.
/// @return @ref Error if the target session exists, the template is empty,
///         any tmux command fails, or created ids cannot be parsed.
[[nodiscard]] Result<void> materialize_template(const tmux::SessionName& session,
                                                const config::Template& tmpl,
                                                const std::filesystem::path& cwd,
                                                const tmux::RunConfig& config = {});

/// @brief Materialize a parsed session template through an injected runner.
/// @param runner tmux command runner, usually a fake in tests.
[[nodiscard]] Result<void> materialize_template(const tmux::SessionName& session,
                                                const config::Template& tmpl,
                                                const std::filesystem::path& cwd,
                                                const tmux::CommandRunner& runner,
                                                const tmux::RunConfig& config = {});

}  // namespace lazytmux::templates
