#include <lazytmux/snapshots/restore.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <thread>
#include <unistd.h>

namespace lazytmux::snapshots {
namespace {

class TempDir {
public:
    TempDir() {
        path_ = std::filesystem::temp_directory_path() /
                ("lazytmux-snapshot-restore-test-" + std::to_string(::getpid()));
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    ~TempDir() {
        std::filesystem::remove_all(path_);
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

std::filesystem::path write_script(const std::filesystem::path& dir, std::string_view body) {
    const auto script = dir / "restore.sh";
    std::ofstream out(script);
    out << body;
    out.close();
    std::filesystem::permissions(script,
                                 std::filesystem::perms::owner_exec |
                                     std::filesystem::perms::owner_read |
                                     std::filesystem::perms::owner_write);
    return script;
}

TEST(SnapshotRestoreTest, LocateRestoreScriptFindsFirstExistingCandidate) {
    TempDir tmp;
    const auto missing = tmp.path() / "missing.sh";
    const auto script = write_script(tmp.path(), "exit 0\n");

    auto found = locate_restore_script(RestoreConfig{
        .shell = "/bin/bash",
        .script_candidates = {missing, script},
    });
    ASSERT_TRUE(found.has_value()) << found.error().display();
    EXPECT_EQ(*found, script);
}

TEST(SnapshotRestoreTest, RestoreUpdatesLastSymlinkAndRunsScript) {
    TempDir tmp;
    const auto snapshot = tmp.path() / "tmux_resurrect_20260415T004643.txt";
    std::ofstream(snapshot) << "state\t2\n";
    const auto marker = tmp.path() / "marker";
    const auto script = write_script(tmp.path(), "printf restored > " + marker.string() + "\n");

    auto restored = restore_snapshot(snapshot,
                                     RestoreConfig{
                                         .shell = "/bin/bash",
                                         .script_candidates = {script},
                                     });
    ASSERT_TRUE(restored.has_value()) << restored.error().display();
    EXPECT_EQ(std::filesystem::read_symlink(tmp.path() / "last"), snapshot);
    std::ifstream in(marker);
    std::string text;
    in >> text;
    EXPECT_EQ(text, "restored");
}

TEST(SnapshotRestoreTest, NonzeroScriptExitReturnsError) {
    TempDir tmp;
    const auto snapshot = tmp.path() / "tmux_resurrect_20260415T004643.txt";
    std::ofstream(snapshot) << "state\t2\n";
    const auto script = write_script(tmp.path(), "echo nope >&2\nexit 7\n");

    auto restored = restore_snapshot(snapshot,
                                     RestoreConfig{
                                         .shell = "/bin/bash",
                                         .script_candidates = {script},
                                     });
    ASSERT_FALSE(restored.has_value());
    EXPECT_EQ(restored.error().kind(), ErrorKind::kExternalCommandFailure);
    EXPECT_NE(restored.error().message().find("code 7"), std::string_view::npos);
}

TEST(SnapshotRestoreTest, MissingSnapshotReturnsErrorBeforeRunningScript) {
    TempDir tmp;
    const auto snapshot = tmp.path() / "tmux_resurrect_20260415T004643.txt";
    const auto marker = tmp.path() / "marker";
    const auto script = write_script(tmp.path(), "printf restored > " + marker.string() + "\n");

    auto restored = restore_snapshot(snapshot,
                                     RestoreConfig{
                                         .shell = "/bin/bash",
                                         .script_candidates = {script},
                                     });
    ASSERT_FALSE(restored.has_value());
    EXPECT_NE(restored.error().message().find("not found"), std::string_view::npos);
    EXPECT_FALSE(std::filesystem::exists(marker));
    EXPECT_FALSE(std::filesystem::exists(tmp.path() / "last"));
}

TEST(SnapshotRestoreTest, MissingScriptDoesNotReplaceLastSymlink) {
    TempDir tmp;
    const auto previous = tmp.path() / "tmux_resurrect_20260101T000000.txt";
    const auto snapshot = tmp.path() / "tmux_resurrect_20260415T004643.txt";
    std::ofstream(previous) << "state\t2\n";
    std::ofstream(snapshot) << "state\t2\n";
    std::filesystem::create_symlink(previous, tmp.path() / "last");

    auto restored = restore_snapshot(snapshot,
                                     RestoreConfig{
                                         .shell = "/bin/bash",
                                         .script_candidates = {tmp.path() / "missing.sh"},
                                     });
    ASSERT_FALSE(restored.has_value());
    EXPECT_NE(restored.error().message().find("not found"), std::string_view::npos);
    EXPECT_EQ(std::filesystem::read_symlink(tmp.path() / "last"), previous);
}

TEST(SnapshotRestoreTest, AsyncWorkerDeliversResult) {
    TempDir tmp;
    const auto snapshot = tmp.path() / "tmux_resurrect_20260415T004643.txt";
    std::ofstream(snapshot) << "state\t2\n";
    const auto script = write_script(tmp.path(), "exit 0\n");

    auto worker = restore_async(snapshot,
                                RestoreConfig{
                                    .shell = "/bin/bash",
                                    .script_candidates = {script},
                                });

    std::optional<Result<void>> result;
    for (int i = 0; i < 100; ++i) {
        result = worker.try_take();
        if (result) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->has_value()) << result->error().display();
}

}  // namespace
}  // namespace lazytmux::snapshots
