#include <lazytmux/snapshots/ops.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unistd.h>

namespace lazytmux::snapshots {
namespace {

class TempDir {
public:
    TempDir() {
        path_ = std::filesystem::temp_directory_path() /
                ("lazytmux-snapshot-ops-test-" + std::to_string(::getpid()) + "-" +
                 std::to_string(next_id_++));
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
    inline static int next_id_{0};
    std::filesystem::path path_;
};

std::filesystem::path write_snapshot(const std::filesystem::path& dir, std::string_view name) {
    const auto path = dir / name;
    std::ofstream(path) << "state\t2\n";
    return path;
}

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

TEST(SnapshotOpsTest, DeleteExistingSnapshot) {
    TempDir tmp;
    const auto snapshot = write_snapshot(tmp.path(), "tmux_resurrect_20260415T004643.txt");

    auto deleted = delete_snapshot(snapshot);

    ASSERT_TRUE(deleted.has_value()) << deleted.error().display();
    EXPECT_FALSE(std::filesystem::exists(snapshot));
}

TEST(SnapshotOpsTest, DeleteMissingSnapshotIsSuccess) {
    TempDir tmp;
    const auto snapshot = tmp.path() / "tmux_resurrect_20260415T004643.txt";

    auto deleted = delete_snapshot(snapshot);

    EXPECT_TRUE(deleted.has_value()) << deleted.error().display();
}

TEST(SnapshotOpsTest, DeleteCurrentLastTargetRemovesLastSymlink) {
    TempDir tmp;
    const auto snapshot = write_snapshot(tmp.path(), "tmux_resurrect_20260415T004643.txt");
    std::filesystem::create_symlink(snapshot, tmp.path() / "last");

    auto deleted = delete_snapshot(snapshot);

    ASSERT_TRUE(deleted.has_value()) << deleted.error().display();
    EXPECT_FALSE(std::filesystem::exists(snapshot));
    EXPECT_EQ(std::filesystem::symlink_status(tmp.path() / "last").type(),
              std::filesystem::file_type::not_found);
}

TEST(SnapshotOpsTest, DeleteLastPathIsRejected) {
    TempDir tmp;
    const auto last = tmp.path() / "last";
    std::ofstream(last) << "not a symlink\n";

    auto deleted = delete_snapshot(last);

    ASSERT_FALSE(deleted.has_value());
    EXPECT_EQ(deleted.error().kind(), ErrorKind::kInvalidInput);
    EXPECT_TRUE(std::filesystem::exists(last));
}

TEST(SnapshotOpsTest, DeleteWithMalformedLastReturnsErrorBeforeDeletingSnapshot) {
    TempDir tmp;
    const auto snapshot = write_snapshot(tmp.path(), "tmux_resurrect_20260415T004643.txt");
    std::ofstream(tmp.path() / "last") << "not a symlink\n";

    auto deleted = delete_snapshot(snapshot);

    ASSERT_FALSE(deleted.has_value());
    EXPECT_EQ(deleted.error().kind(), ErrorKind::kInvalidInput);
    EXPECT_TRUE(std::filesystem::exists(snapshot));
}

TEST(SnapshotOpsTest, RestoreFailureRollsBackPreviousLastSymlink) {
    TempDir tmp;
    const auto previous = write_snapshot(tmp.path(), "tmux_resurrect_20260101T000000.txt");
    const auto snapshot = write_snapshot(tmp.path(), "tmux_resurrect_20260415T004643.txt");
    std::filesystem::create_symlink(previous, tmp.path() / "last");
    const auto script = write_script(tmp.path(), "echo nope >&2\nexit 7\n");

    auto restored = restore_snapshot(snapshot,
                                     RestoreConfig{
                                         .shell = "/bin/bash",
                                         .script_candidates = {script},
                                     });

    ASSERT_FALSE(restored.has_value());
    EXPECT_EQ(restored.error().kind(), ErrorKind::kExternalCommandFailure);
    EXPECT_EQ(std::filesystem::read_symlink(tmp.path() / "last"), previous);
}

TEST(SnapshotOpsTest, RestoreFailureWithoutPreviousLastRemovesNewLastSymlink) {
    TempDir tmp;
    const auto snapshot = write_snapshot(tmp.path(), "tmux_resurrect_20260415T004643.txt");
    const auto script = write_script(tmp.path(), "exit 7\n");

    auto restored = restore_snapshot(snapshot,
                                     RestoreConfig{
                                         .shell = "/bin/bash",
                                         .script_candidates = {script},
                                     });

    ASSERT_FALSE(restored.has_value());
    EXPECT_EQ(restored.error().kind(), ErrorKind::kExternalCommandFailure);
    EXPECT_EQ(std::filesystem::symlink_status(tmp.path() / "last").type(),
              std::filesystem::file_type::not_found);
}

TEST(SnapshotOpsTest, RestoreRollbackFailureIsIncludedInError) {
    TempDir tmp;
    const auto previous = write_snapshot(tmp.path(), "tmux_resurrect_20260101T000000.txt");
    const auto snapshot = write_snapshot(tmp.path(), "tmux_resurrect_20260415T004643.txt");
    const auto last = tmp.path() / "last";
    std::filesystem::create_symlink(previous, last);
    const auto script = write_script(tmp.path(),
                                     "rm " + last.string() + "\nmkdir " + last.string() +
                                         "\ntouch " + (last / "block").string() + "\nexit 7\n");

    auto restored = restore_snapshot(snapshot,
                                     RestoreConfig{
                                         .shell = "/bin/bash",
                                         .script_candidates = {script},
                                     });

    ASSERT_FALSE(restored.has_value());
    EXPECT_EQ(restored.error().kind(), ErrorKind::kExternalCommandFailure);
    EXPECT_NE(restored.error().display().find("rollback failed"), std::string::npos);
}

TEST(SnapshotOpsTest, RestoreSnapshotAsyncUsesRestoreWorker) {
    TempDir tmp;
    const auto snapshot = write_snapshot(tmp.path(), "tmux_resurrect_20260415T004643.txt");
    const auto script = write_script(tmp.path(), "exit 0\n");

    auto worker = restore_snapshot_async(snapshot,
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

TEST(SnapshotOpsTest, SnapshotLabelContainsTimestampAndCounts) {
    auto snapshot = parse_snapshot_text("/tmp/tmux_resurrect_20260415T004643.txt",
                                        "pane\talpha\t0\t1\t:*\t0\talpha\t:/tmp\t0\tzsh\t:\n"
                                        "window\talpha\t0\t:editor\t1\t:*\tabcd\t:\n"
                                        "window\talpha\t1\t:shell\t1\t:*\tabcd\t:\n");
    ASSERT_TRUE(snapshot.has_value()) << snapshot.error().display();

    const auto label = snapshot_label(*snapshot);

    EXPECT_NE(label.find("2026-04-15 00:46:43"), std::string::npos);
    EXPECT_NE(label.find("1 session"), std::string::npos);
    EXPECT_NE(label.find("2 windows"), std::string::npos);
}

}  // namespace
}  // namespace lazytmux::snapshots
