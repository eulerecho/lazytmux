#include <lazytmux/proc.hpp>

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <unistd.h>

namespace lazytmux::proc {
namespace {

class TempDir {
public:
    TempDir() {
        path_ = std::filesystem::temp_directory_path() /
                ("lazytmux-proc-test-" + std::to_string(::getpid()));
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

void write_process(const std::filesystem::path& root,
                   std::uint32_t pid,
                   std::uint32_t ppid,
                   const std::filesystem::path& exe) {
    const auto dir = root / std::to_string(pid);
    std::filesystem::create_directories(dir);
    std::ofstream status(dir / "status");
    status << "Name:\ttmux\n"
           << "Pid:\t" << pid << '\n'
           << "PPid:\t" << ppid << '\n';
    std::filesystem::create_symlink(exe, dir / "exe");
}

TEST(ProcReaderTest, ReadsParentPidAndTmuxExecutable) {
    TempDir tmp;
    write_process(tmp.path(), 123, 45, "/usr/bin/tmux");

    auto info = read_process_info(123, tmp.path());
    ASSERT_TRUE(info.has_value()) << info.error().display();
    EXPECT_EQ(info->parent_pid, 45u);
    EXPECT_TRUE(info->is_tmux);
}

TEST(ProcReaderTest, MarksNonTmuxExecutableFalse) {
    TempDir tmp;
    write_process(tmp.path(), 123, 45, "/usr/bin/bash");

    auto info = read_process_info(123, tmp.path());
    ASSERT_TRUE(info.has_value()) << info.error().display();
    EXPECT_FALSE(info->is_tmux);
}

TEST(ProcReaderTest, ReturnsErrorWhenStatusIsMissing) {
    TempDir tmp;
    std::filesystem::create_directories(tmp.path() / "123");
    std::filesystem::create_symlink("/usr/bin/tmux", tmp.path() / "123" / "exe");

    auto info = read_process_info(123, tmp.path());
    ASSERT_FALSE(info.has_value());
    EXPECT_NE(info.error().message().find("status"), std::string_view::npos);
}

TEST(ProcReaderTest, WalkerFeedsNestingCount) {
    TempDir tmp;
    write_process(tmp.path(), 100, 50, "/usr/bin/bash");
    write_process(tmp.path(), 50, 1, "/usr/bin/tmux");

    auto walker = make_proc_walker(tmp.path());
    EXPECT_EQ(nesting::count_tmux_in_walk(100, walker), 1u);
}

}  // namespace
}  // namespace lazytmux::proc
