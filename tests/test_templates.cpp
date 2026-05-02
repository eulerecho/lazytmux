#include <lazytmux/templates.hpp>

#include <deque>
#include <expected>
#include <filesystem>
#include <gtest/gtest.h>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lazytmux::templates {
namespace {

enum class CallKind {
    kRun,
    kStatus,
};

struct RecordedCall {
    CallKind kind;
    std::vector<std::string> args;
};

struct FakeRunner {
    std::vector<RecordedCall> calls;
    std::deque<Result<std::string>> outputs;
    std::deque<Result<bool>> statuses;

    [[nodiscard]] tmux::CommandRunner command_runner() {
        return tmux::CommandRunner{
            .run = [this](std::span<const std::string> args,
                          const tmux::RunConfig&) -> Result<std::string> {
                calls.push_back(RecordedCall{
                    .kind = CallKind::kRun,
                    .args = std::vector<std::string>{args.begin(), args.end()},
                });
                if (outputs.empty()) {
                    return std::unexpected(Error{ErrorKind::kInternal, "unexpected run call"});
                }
                auto result = std::move(outputs.front());
                outputs.pop_front();
                return result;
            },
            .run_status = [this](std::span<const std::string> args,
                                 const tmux::RunConfig&) -> Result<bool> {
                calls.push_back(RecordedCall{
                    .kind = CallKind::kStatus,
                    .args = std::vector<std::string>{args.begin(), args.end()},
                });
                if (statuses.empty()) {
                    return std::unexpected(Error{ErrorKind::kInternal, "unexpected status call"});
                }
                auto result = std::move(statuses.front());
                statuses.pop_front();
                return result;
            },
        };
    }
};

config::Template make_template(std::vector<config::TemplateWindow> windows) {
    return config::Template{.windows = std::move(windows)};
}

config::TemplateWindow window(std::string name, std::string command = "") {
    return config::TemplateWindow{.name = std::move(name), .command = std::move(command)};
}

void expect_call(const FakeRunner& fake,
                 std::size_t index,
                 CallKind kind,
                 const std::vector<std::string>& args) {
    ASSERT_LT(index, fake.calls.size());
    EXPECT_EQ(fake.calls[index].kind, kind);
    EXPECT_EQ(fake.calls[index].args, args);
}

TEST(TemplateMaterializeTest, ExistingTargetSessionRejectsBeforeMutation) {
    FakeRunner fake;
    fake.statuses.push_back(true);
    const auto session = tmux::SessionName::from_validated("dev");
    const auto tmpl = make_template({window("editor")});

    auto result =
        materialize_template(session, tmpl, std::filesystem::path{"/tmp"}, fake.command_runner());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind(), ErrorKind::kInvalidInput);
    ASSERT_EQ(fake.calls.size(), 1U);
    expect_call(fake, 0, CallKind::kStatus, {"has-session", "-t", "dev"});
}

TEST(TemplateMaterializeTest, EmptyTemplateRejectsBeforeTmuxCalls) {
    FakeRunner fake;
    const auto session = tmux::SessionName::from_validated("dev");
    const config::Template tmpl;

    auto result =
        materialize_template(session, tmpl, std::filesystem::path{"/tmp"}, fake.command_runner());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind(), ErrorKind::kInvalidInput);
    EXPECT_TRUE(fake.calls.empty());
}

TEST(TemplateMaterializeTest, OneWindowTemplateCreatesSessionOnly) {
    FakeRunner fake;
    fake.statuses.push_back(false);
    fake.outputs.push_back(std::string{"@5|||%12\n"});
    const auto session = tmux::SessionName::from_validated("dev");
    const auto tmpl = make_template({window("editor")});

    auto result = materialize_template(
        session, tmpl, std::filesystem::path{"/home/p/project one"}, fake.command_runner());

    ASSERT_TRUE(result.has_value()) << result.error().display();
    ASSERT_EQ(fake.calls.size(), 2U);
    expect_call(fake, 0, CallKind::kStatus, {"has-session", "-t", "dev"});
    expect_call(fake,
                1,
                CallKind::kRun,
                {"new-session",
                 "-d",
                 "-s",
                 "dev",
                 "-n",
                 "editor",
                 "-c",
                 "/home/p/project one",
                 "-P",
                 "-F",
                 std::string{tmux::kCreatedPaneFormat}});
}

TEST(TemplateMaterializeTest, MultiWindowTemplatePreservesOrderAndSendsCommands) {
    FakeRunner fake;
    fake.statuses.push_back(false);
    fake.statuses.push_back(true);
    fake.statuses.push_back(true);
    fake.outputs.push_back(std::string{"@5|||%12\n"});
    fake.outputs.push_back(std::string{"@6|||%13\n"});
    fake.outputs.push_back(std::string{"@7|||%14\n"});
    const auto session = tmux::SessionName::from_validated("dev");
    const auto tmpl = make_template({
        window("editor", "nvim"),
        window("shell"),
        window("logs", "tail -f app.log"),
    });

    auto result =
        materialize_template(session, tmpl, std::filesystem::path{"/repo"}, fake.command_runner());

    ASSERT_TRUE(result.has_value()) << result.error().display();
    ASSERT_EQ(fake.calls.size(), 6U);
    expect_call(fake, 0, CallKind::kStatus, {"has-session", "-t", "dev"});
    expect_call(fake,
                1,
                CallKind::kRun,
                {"new-session",
                 "-d",
                 "-s",
                 "dev",
                 "-n",
                 "editor",
                 "-c",
                 "/repo",
                 "-P",
                 "-F",
                 std::string{tmux::kCreatedPaneFormat}});
    expect_call(fake, 2, CallKind::kStatus, {"send-keys", "-t", "%12", "--", "nvim", "Enter"});
    expect_call(fake,
                3,
                CallKind::kRun,
                {"new-window",
                 "-t",
                 "dev",
                 "-n",
                 "shell",
                 "-c",
                 "/repo",
                 "-P",
                 "-F",
                 std::string{tmux::kCreatedPaneFormat}});
    expect_call(fake,
                4,
                CallKind::kRun,
                {"new-window",
                 "-t",
                 "dev",
                 "-n",
                 "logs",
                 "-c",
                 "/repo",
                 "-P",
                 "-F",
                 std::string{tmux::kCreatedPaneFormat}});
    expect_call(
        fake, 5, CallKind::kStatus, {"send-keys", "-t", "%14", "--", "tail -f app.log", "Enter"});
}

TEST(TemplateMaterializeTest, CommandFailureStopsLaterWindows) {
    FakeRunner fake;
    fake.statuses.push_back(false);
    fake.statuses.push_back(false);
    fake.outputs.push_back(std::string{"@5|||%12\n"});
    fake.outputs.push_back(std::string{"@6|||%13\n"});
    const auto session = tmux::SessionName::from_validated("dev");
    const auto tmpl = make_template({
        window("editor"),
        window("build", "make"),
        window("logs"),
    });

    auto result =
        materialize_template(session, tmpl, std::filesystem::path{"/repo"}, fake.command_runner());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind(), ErrorKind::kExternalCommandFailure);
    ASSERT_EQ(fake.calls.size(), 4U);
    expect_call(fake, 3, CallKind::kStatus, {"send-keys", "-t", "%13", "--", "make", "Enter"});
}

TEST(TemplateMaterializeTest, CreateFailureIncludesWindowContext) {
    FakeRunner fake;
    fake.statuses.push_back(false);
    fake.outputs.push_back(std::unexpected(Error{ErrorKind::kTimeout, "tmux timed out"}));
    const auto session = tmux::SessionName::from_validated("dev");
    const auto tmpl = make_template({window("editor")});

    auto result =
        materialize_template(session, tmpl, std::filesystem::path{"/repo"}, fake.command_runner());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind(), ErrorKind::kTimeout);
    ASSERT_FALSE(result.error().contexts().empty());
    EXPECT_EQ(result.error().contexts().back(), "template window 0 (editor)");
}

}  // namespace
}  // namespace lazytmux::templates
