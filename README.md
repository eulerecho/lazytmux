# lazytmux

A multi-panel TUI for tmux. Status, sessions, windows, panes, and snapshots in one popup you open from any tmux session. Templates, frecency-ranked sessions, zoxide-aware new-session flow, scripting subcommands, multi-server support, nested-tmux detection.

```
prefix L → lazytmux popup → 1-5 panels → keystrokes for everything
you used to remember chord-by-chord
```

> **Status: early development.** Build system and CI are in. The binary handles `--version` and nothing else yet. None of the panels, command mode, templates, or CLI subcommands described below are wired up.

## Goals

What lazytmux will do once v0.1 is ready:

- **Five-panel popup.** Status, Sessions, Windows, Panes, Snapshots. Open from any tmux session, dismiss back to where you were.
- **Frecency ranking** for sessions, with a 7-day half-life decay. State persisted at `$XDG_DATA_HOME/lazytmux/frecency.json`.
- **Templates** in `~/.config/lazytmux/config.toml`. Materialize a named layout into a fresh tmux session in one keystroke. Per-window commands, init scripts, the works.
- **Zoxide integration** for the new-session directory picker (`n` on the sessions panel).
- **Snapshots** via tmux-resurrect. List, restore, and delete from the panel. Restore happens in the background; the UI keeps drawing.
- **Command mode** (`:`) with shell quoting and per-command argument completion. `:switch-client <Tab>` lists session names.
- **Scripting subcommands**: `lazytmux attach <name>`, `lazytmux save`, `lazytmux list-templates`, `lazytmux materialize <name> <dir>`. Useful from shell aliases.
- **Multi-server**: picker over `/tmp/tmux-<uid>/` when multiple sockets exist. `-L <name>` / `-S <path>` for explicit selection.
- **Nested-tmux detection** with a confirm step before `switch-client` from inside a nested tmux.
- **Theming** via TOML palette. Gruvbox-dark defaults.

## What works today

`lazytmux --version`. The build, test, format, and CI pipeline is wired. That's the whole list.

## What comes next

The domain library lands first: format-string parsers, ID newtypes, an error model. Pure-logic code, no filesystem, no process spawning. After that come the I/O wrappers (process runner, `/proc` reader, socket discovery, TOML config loader) and the tmux command builders that sit on top.

CLI subcommands ship before the TUI does, so the binary is useful from shell aliases while the FTXUI integration is still going in. Once the CLI is in, the TUI framework, panels, and action handlers come last. v0.1 ships when the full keybind set in [Goals](#goals) works against a live tmux server.

A `docs/v0.1.md` with acceptance criteria lands once the CLI subcommands are in.

## Build

```bash
./scripts/configure.sh
./scripts/build.sh
./scripts/test.sh
```

Or manually:

```bash
uv sync --group dev
uv run conan install . -s build_type=Debug -s compiler.cppstd=23 --build=missing
cmake --preset conan-enable_tsan_false-debug
cmake --build --preset conan-enable_tsan_false-debug
ctest --preset conan-enable_tsan_false-debug --output-on-failure
```

## Develop

```bash
./scripts/format.sh --check    # clang-format dry-run, fails on any diff
./scripts/format.sh            # clang-format -i in place
./scripts/test.sh -L live      # live-tmux suite (opt-in)
```

Sanitizers go through Conan options:

```bash
uv run conan install . -s build_type=Debug -s compiler.cppstd=23 \
    -o '&:enable_asan=True' -o '&:enable_ubsan=True' --build=missing
cmake --preset conan-enable_tsan_false-debug

LAZYTMUX_TSAN=1 ./scripts/configure.sh
LAZYTMUX_TSAN=1 ./scripts/build.sh
LAZYTMUX_TSAN=1 ./scripts/test.sh
```

ASan and TSan together is a configure error. The helper enforces it.

## Layout

```text
include/lazytmux/    Public headers.
src/                 Implementation.
tests/               GoogleTest suite.
cmake/               Reusable CMake helpers (warnings, sanitizers).
scripts/             Build, test, and format wrappers.
```

## Requirements

- CMake 3.24+
- GCC 13+ or Clang 17+ (C++23)
- [uv](https://docs.astral.sh/uv/) for the Python tooling (Conan, clang-format, ruff)
- tmux 3.0+ at runtime (not needed to build)

## License

MIT. See [LICENSE](./LICENSE).
