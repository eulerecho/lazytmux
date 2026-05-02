#include <lazytmux/snapshots/list.hpp>

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <unistd.h>

namespace lazytmux::snapshots {
namespace {

class TempDir {
public:
    TempDir() {
        path_ = std::filesystem::temp_directory_path() /
                ("lazytmux-snapshot-list-test-" + std::to_string(::getpid()));
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

constexpr std::string_view kSnapshotText =
    "pane\t0\t0\t1\t:*\t0\teigenspace\t:/home/pratik\t0\tzsh\t:\n"
    "pane\t2\t0\t1\t:*\t0\teigenspace\t:/home/pratik\t1\tzsh\t:\n"
    "pane\tpersonal\t0\t1\t:*\t0\teigenspace\t:/home/pratik/personal\t1\tzsh\t:\n"
    "window\t0\t0\t:test\t1\t:*\t3503,136x38,0,0\toff\n"
    "window\t2\t0\t:zsh\t1\t:*\tbee1,66x36,0,0,4\t:\n"
    "window\tpersonal\t0\t:zsh\t1\t:*\tbedf,66x36,0,0,2\t:\n"
    "state\t2\n";

TEST(SnapshotParseTest, ParsesSnapshotBody) {
    auto snapshot = parse_snapshot_text("/tmp/tmux_resurrect_20260415T004643.txt", kSnapshotText);
    ASSERT_TRUE(snapshot.has_value()) << snapshot.error().display();
    EXPECT_EQ(snapshot->session_count, 3u);
    EXPECT_EQ(snapshot->window_count, 3u);
    EXPECT_EQ(snapshot->session_names, std::vector<std::string>({"0", "2", "personal"}));
    ASSERT_GE(snapshot->windows.size(), 2u);
    EXPECT_EQ(snapshot->windows[0], (std::pair<std::string, std::string>{"0", "test"}));
    EXPECT_EQ(snapshot->windows[1], (std::pair<std::string, std::string>{"2", "zsh"}));
}

TEST(SnapshotParseTest, EmptyFileHasZeroCounts) {
    auto snapshot = parse_snapshot_text("/tmp/tmux_resurrect_20260101T000000.txt", "");
    ASSERT_TRUE(snapshot.has_value()) << snapshot.error().display();
    EXPECT_EQ(snapshot->session_count, 0u);
    EXPECT_EQ(snapshot->window_count, 0u);
}

TEST(SnapshotListTest, ResolvesXdgBeforeHome) {
    auto dir = resolve_resurrect_dir("/xdg", "/home/u");
    ASSERT_TRUE(dir.has_value());
    EXPECT_EQ(*dir, std::filesystem::path("/xdg/tmux/resurrect"));
}

TEST(SnapshotListTest, ListsSnapshotsNewestFirstAndSkipsNoise) {
    TempDir tmp;
    {
        std::ofstream out(tmp.path() / "tmux_resurrect_20260101T000000.txt");
        out << "";
    }
    {
        std::ofstream out(tmp.path() / "tmux_resurrect_20260415T004643.txt");
        out << kSnapshotText;
    }
    {
        std::ofstream out(tmp.path() / "last");
        out << kSnapshotText;
    }
    {
        std::ofstream out(tmp.path() / "not-a-snapshot.txt");
        out << kSnapshotText;
    }

    auto snapshots = list_snapshots_in(tmp.path());
    ASSERT_TRUE(snapshots.has_value()) << snapshots.error().display();
    ASSERT_EQ(snapshots->size(), 2u);
    EXPECT_EQ((*snapshots)[0].timestamp.display(), "2026-04-15 00:46:43");
    EXPECT_EQ((*snapshots)[1].timestamp.display(), "2026-01-01 00:00:00");
}

TEST(SnapshotListTest, MissingDirectoryReturnsEmpty) {
    TempDir tmp;
    auto snapshots = list_snapshots_in(tmp.path() / "missing");
    ASSERT_TRUE(snapshots.has_value()) << snapshots.error().display();
    EXPECT_TRUE(snapshots->empty());
}

}  // namespace
}  // namespace lazytmux::snapshots
