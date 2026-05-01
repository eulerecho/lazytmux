#include <lazytmux/cmdmode/parse.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lazytmux::cmdmode {

namespace {

inline constexpr std::array<ArgKind, 1> kArgs_Attach{ArgKind::Session};
inline constexpr std::array<ArgKind, 1> kArgs_AttachSession{ArgKind::Session};
inline constexpr std::array<ArgKind, 1> kArgs_SwitchClient{ArgKind::Session};
inline constexpr std::array<ArgKind, 1> kArgs_KillSession{ArgKind::Session};
inline constexpr std::array<ArgKind, 2> kArgs_RenameSession{ArgKind::Session, ArgKind::FreeText};
inline constexpr std::array<ArgKind, 1> kArgs_KillWindow{ArgKind::Window};
inline constexpr std::array<ArgKind, 2> kArgs_RenameWindow{ArgKind::Window, ArgKind::FreeText};
inline constexpr std::array<ArgKind, 1> kArgs_SelectWindow{ArgKind::Window};
inline constexpr std::array<ArgKind, 1> kArgs_SwapWindow{ArgKind::Window};
inline constexpr std::array<ArgKind, 1> kArgs_KillPane{ArgKind::Pane};
inline constexpr std::array<ArgKind, 1> kArgs_SelectPane{ArgKind::Pane};

inline constexpr std::array<ArgSchema, 11> kSchemasArr{{
    {.command = "attach", .args = kArgs_Attach},
    {.command = "attach-session", .args = kArgs_AttachSession},
    {.command = "switch-client", .args = kArgs_SwitchClient},
    {.command = "kill-session", .args = kArgs_KillSession},
    {.command = "rename-session", .args = kArgs_RenameSession},
    {.command = "kill-window", .args = kArgs_KillWindow},
    {.command = "rename-window", .args = kArgs_RenameWindow},
    {.command = "select-window", .args = kArgs_SelectWindow},
    {.command = "swap-window", .args = kArgs_SwapWindow},
    {.command = "kill-pane", .args = kArgs_KillPane},
    {.command = "select-pane", .args = kArgs_SelectPane},
}};

}  // namespace

const std::span<const ArgSchema> kSchemas{kSchemasArr};

std::optional<std::vector<std::string>> tokenize(std::string_view in) {
    enum class S { Normal, Double, Single };

    std::vector<std::string> out;
    std::string cur;
    bool in_token = false;
    S state = S::Normal;

    for (std::size_t i = 0; i < in.size(); ++i) {
        const char c = in[i];
        switch (state) {
            case S::Normal:
                if (std::isspace(static_cast<unsigned char>(c)) != 0) {
                    if (in_token) {
                        out.push_back(std::move(cur));
                        cur.clear();
                        in_token = false;
                    }
                } else if (c == '\'') {
                    state = S::Single;
                    in_token = true;
                } else if (c == '"') {
                    state = S::Double;
                    in_token = true;
                } else if (c == '\\') {
                    if (i + 1 >= in.size()) {
                        return std::nullopt;
                    }
                    cur.push_back(in[++i]);
                    in_token = true;
                } else {
                    cur.push_back(c);
                    in_token = true;
                }
                break;

            case S::Single:
                if (c == '\'') {
                    state = S::Normal;
                } else {
                    cur.push_back(c);
                }
                break;

            case S::Double:
                if (c == '"') {
                    state = S::Normal;
                } else if (c == '\\' && i + 1 < in.size() &&
                           (in[i + 1] == '"' || in[i + 1] == '\\')) {
                    cur.push_back(in[++i]);
                } else {
                    cur.push_back(c);
                }
                break;
        }
    }

    if (state != S::Normal) {
        return std::nullopt;
    }
    if (in_token) {
        out.push_back(std::move(cur));
    }
    return out;
}

const ArgSchema* lookup_schema(std::string_view command) {
    for (const auto& s : kSchemasArr) {
        if (s.command == command) {
            return &s;
        }
    }
    return nullptr;
}

std::string shell_quote(std::string_view s) {
    if (s.empty()) {
        return std::string{"\"\""};
    }
    const bool needs = std::ranges::any_of(s, [](char c) {
        return std::isspace(static_cast<unsigned char>(c)) != 0 || c == '\'' || c == '"' ||
               c == '\\';
    });
    if (!needs) {
        return std::string{s};
    }
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('"');
    for (char c : s) {
        if (c == '\\' || c == '"') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

namespace {

bool is_ws(char c) {
    return std::isspace(static_cast<unsigned char>(c)) != 0;
}

}  // namespace

std::vector<std::string> complete(
    std::string_view input,
    std::span<const std::string_view> command_names,
    const std::function<std::vector<std::string>(ArgKind)>& candidates) {
    auto tokens_opt = tokenize(input);
    const std::vector<std::string>& tokens =
        tokens_opt.has_value() ? *tokens_opt : *(tokens_opt = std::vector<std::string>{});

    const bool trailing_ws = !input.empty() && is_ws(input.back());

    const std::string_view head =
        tokens.empty() ? std::string_view{} : std::string_view{tokens.front()};
    const bool head_is_command = std::ranges::find(command_names, head) != command_names.end() &&
                                 (trailing_ws || tokens.size() > 1);

    std::vector<std::string> out;

    if (!head_is_command) {
        // Head fallback uses raw input, not the tokenized head: an
        // unbalanced quote yields tokens=[] but the user still sees
        // their literal input echoed back as the only suggestion.
        std::size_t pos = 0;
        while (pos < input.size() && is_ws(input[pos])) {
            ++pos;
        }
        const std::size_t head_start = pos;
        while (pos < input.size() && !is_ws(input[pos])) {
            ++pos;
        }
        const std::string_view head_str = input.substr(head_start, pos - head_start);
        const std::string_view tail = input.substr(pos);

        for (auto cmd : command_names) {
            if (cmd.starts_with(head_str)) {
                std::string s;
                s.reserve(cmd.size() + tail.size());
                s.append(cmd);
                s.append(tail);
                out.push_back(std::move(s));
            }
        }
        return out;
    }

    const auto* schema = lookup_schema(head);
    if (schema == nullptr) {
        return out;
    }

    const std::size_t arg_index =
        trailing_ws ? tokens.size() - 1 : (tokens.size() >= 2 ? tokens.size() - 2 : 0);
    if (arg_index >= schema->args.size()) {
        return out;
    }

    const ArgKind kind = schema->args[arg_index];
    if (kind == ArgKind::FreeText) {
        return out;
    }

    const std::string_view partial =
        trailing_ws ? std::string_view{} : std::string_view{tokens.back()};

    const std::size_t prefix_len = trailing_ws ? input.size() : input.size() - partial.size();
    const std::string_view prefix = input.substr(0, prefix_len);

    const auto cands = candidates(kind);
    for (const auto& c : cands) {
        if (!std::string_view{c}.starts_with(partial)) {
            continue;
        }
        std::string s;
        s.append(prefix);
        s.append(shell_quote(c));
        out.push_back(std::move(s));
    }
    return out;
}

}  // namespace lazytmux::cmdmode
