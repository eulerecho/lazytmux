#include <lazytmux/cli.hpp>

#include <gtest/gtest.h>
#include <initializer_list>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace lazytmux::cli {
namespace {

Result<CliRequest> parse(std::initializer_list<std::string_view> args) {
    const std::vector<std::string_view> v{args};
    return parse_args(v);
}

TEST(CliParseTest, NoArgsShowsHelp) {
    auto request = parse({});
    ASSERT_TRUE(request.has_value()) << request.error().display();
    EXPECT_EQ(command_kind(request->command), CommandKind::kHelp);
    EXPECT_EQ(request->socket.kind, tmux::SocketSelectionKind::kDefault);
}

TEST(CliParseTest, HelpFlagShowsHelp) {
    auto request = parse({"--help"});
    ASSERT_TRUE(request.has_value()) << request.error().display();
    EXPECT_EQ(command_kind(request->command), CommandKind::kHelp);
}

TEST(CliParseTest, VersionFlagPrintsVersion) {
    auto request = parse({"--version"});
    ASSERT_TRUE(request.has_value()) << request.error().display();
    EXPECT_EQ(command_kind(request->command), CommandKind::kVersion);
}

TEST(CliParseTest, AttachParsesSessionName) {
    auto request = parse({"attach", "dev"});
    ASSERT_TRUE(request.has_value()) << request.error().display();

    const auto& attach = std::get<AttachCommand>(request->command);
    EXPECT_EQ(attach.session.as_str(), "dev");
}

TEST(CliParseTest, SaveParsesWithoutArgs) {
    auto request = parse({"save"});
    ASSERT_TRUE(request.has_value()) << request.error().display();
    EXPECT_EQ(command_kind(request->command), CommandKind::kSave);
}

TEST(CliParseTest, ListTemplatesParsesWithoutArgs) {
    auto request = parse({"list-templates"});
    ASSERT_TRUE(request.has_value()) << request.error().display();
    EXPECT_EQ(command_kind(request->command), CommandKind::kListTemplates);
}

TEST(CliParseTest, MaterializeParsesTemplateAndDirectory) {
    auto request = parse({"materialize", "dev", "/tmp/project"});
    ASSERT_TRUE(request.has_value()) << request.error().display();

    const auto& materialize = std::get<MaterializeCommand>(request->command);
    EXPECT_EQ(materialize.template_name.as_str(), "dev");
    EXPECT_EQ(materialize.directory, "/tmp/project");
}

TEST(CliParseTest, NamedSocketSelectionPrecedesCommand) {
    auto request = parse({"-L", "work", "attach", "dev"});
    ASSERT_TRUE(request.has_value()) << request.error().display();

    EXPECT_EQ(request->socket.kind, tmux::SocketSelectionKind::kNamed);
    EXPECT_EQ(request->socket.value, "work");
    EXPECT_EQ(std::get<AttachCommand>(request->command).session.as_str(), "dev");
}

TEST(CliParseTest, PathSocketSelectionPrecedesCommand) {
    auto request = parse({"-S", "/tmp/tmux-test", "attach", "dev"});
    ASSERT_TRUE(request.has_value()) << request.error().display();

    EXPECT_EQ(request->socket.kind, tmux::SocketSelectionKind::kPath);
    EXPECT_EQ(request->socket.value, "/tmp/tmux-test");
}

TEST(CliParseTest, SocketOptionsConflict) {
    auto request = parse({"-L", "work", "-S", "/tmp/tmux-test", "attach", "dev"});
    ASSERT_FALSE(request.has_value());
    EXPECT_EQ(request.error().kind(), ErrorKind::kInvalidInput);
}

TEST(CliParseTest, MissingSocketValueFails) {
    auto request = parse({"-S"});
    ASSERT_FALSE(request.has_value());
    EXPECT_EQ(request.error().kind(), ErrorKind::kInvalidInput);
    EXPECT_NE(request.error().message().find("requires a value"), std::string_view::npos);
}

TEST(CliParseTest, UnknownOptionFails) {
    auto request = parse({"--bogus"});
    ASSERT_FALSE(request.has_value());
    EXPECT_EQ(request.error().kind(), ErrorKind::kInvalidInput);
    EXPECT_NE(request.error().message().find("unknown option"), std::string_view::npos);
}

TEST(CliParseTest, UnknownSubcommandFails) {
    auto request = parse({"bogus"});
    ASSERT_FALSE(request.has_value());
    EXPECT_EQ(request.error().kind(), ErrorKind::kInvalidInput);
    EXPECT_NE(request.error().message().find("unknown subcommand"), std::string_view::npos);
}

TEST(CliParseTest, MissingAttachArgFails) {
    auto request = parse({"attach"});
    ASSERT_FALSE(request.has_value());
    EXPECT_EQ(request.error().kind(), ErrorKind::kInvalidInput);
}

TEST(CliParseTest, ExtraAttachArgFails) {
    auto request = parse({"attach", "dev", "extra"});
    ASSERT_FALSE(request.has_value());
    EXPECT_EQ(request.error().kind(), ErrorKind::kInvalidInput);
}

TEST(CliParseTest, GlobalOptionAfterCommandFails) {
    auto request = parse({"attach", "dev", "-S", "/tmp/tmux-test"});
    ASSERT_FALSE(request.has_value());
    EXPECT_EQ(request.error().kind(), ErrorKind::kInvalidInput);
    EXPECT_NE(request.error().message().find("before the subcommand"), std::string_view::npos);
}

TEST(CliParseTest, InvalidSessionNameFailsAsUsageError) {
    auto request = parse({"attach", "bad:name"});
    ASSERT_FALSE(request.has_value());
    EXPECT_EQ(request.error().kind(), ErrorKind::kInvalidInput);
    EXPECT_NE(request.error().message().find("invalid session name"), std::string_view::npos);
}

TEST(CliParseTest, MaterializeRequiresTemplateAndDirectory) {
    auto request = parse({"materialize", "dev"});
    ASSERT_FALSE(request.has_value());
    EXPECT_EQ(request.error().kind(), ErrorKind::kInvalidInput);
}

TEST(CliUsageTest, TopLevelUsageListsCommands) {
    const std::string text = usage();
    EXPECT_NE(text.find("attach <session>"), std::string::npos);
    EXPECT_NE(text.find("list-templates"), std::string::npos);
    EXPECT_NE(text.find("materialize <template> <dir>"), std::string::npos);
}

TEST(CliUsageTest, CommandUsageDescribesSpecificCommand) {
    EXPECT_NE(command_usage(CommandKind::kAttach).find("attach <session>"), std::string::npos);
    EXPECT_NE(command_usage(CommandKind::kSave).find("save"), std::string::npos);
    EXPECT_NE(command_usage(CommandKind::kMaterialize).find("materialize <template> <dir>"),
              std::string::npos);
}

}  // namespace
}  // namespace lazytmux::cli
