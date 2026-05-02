#include <lazytmux/frecency_rank.hpp>
#include <lazytmux/frecency_store.hpp>

#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <string>
#include <string_view>
#include <sys/file.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace lazytmux::frecency {
namespace {

class TempDir {
public:
    TempDir() {
        path_ = std::filesystem::temp_directory_path() /
                ("lazytmux-frecency-rank-test-" + std::to_string(::getpid()) + "-" +
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

tmux::SessionName name(std::string_view s) {
    auto r = tmux::SessionName::make(s);
    return std::move(r).value();
}

tmux::Session session(std::string_view session_name, std::string session_id) {
    return tmux::Session{
        .id = tmux::SessionId::from_validated(std::move(session_id)),
        .name = name(session_name),
        .windows = 1,
        .attached = false,
        .last_activity_unix = 0,
    };
}

std::vector<std::string> names(const std::vector<RankedSession>& ranked) {
    std::vector<std::string> out;
    out.reserve(ranked.size());
    for (const auto& row : ranked) {
        out.push_back(std::string{row.session.name.as_str()});
    }
    return out;
}

std::string read_all(const std::filesystem::path& path) {
    std::ifstream in(path);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

TEST(FrecencyRankTest, UnknownSessionsScoreZeroAndKeepInputOrder) {
    const auto sessions = std::vector<tmux::Session>{
        session("zeta", "$1"),
        session("alpha", "$2"),
        session("beta", "$3"),
    };

    const auto ranked = rank_sessions(Index{}, sessions, 1'700'000'000);

    EXPECT_EQ(names(ranked), (std::vector<std::string>{"zeta", "alpha", "beta"}));
    ASSERT_EQ(ranked.size(), 3U);
    EXPECT_DOUBLE_EQ(ranked[0].score, 0.0);
    EXPECT_DOUBLE_EQ(ranked[1].score, 0.0);
    EXPECT_DOUBLE_EQ(ranked[2].score, 0.0);
}

TEST(FrecencyRankTest, SortsByDescendingScore) {
    constexpr std::int64_t now = 1'700'000'000;
    Index index;
    index.visit(name("alpha"), now);
    index.visit(name("alpha"), now);
    index.visit(name("beta"), now);
    const auto sessions = std::vector<tmux::Session>{
        session("beta", "$1"),
        session("gamma", "$2"),
        session("alpha", "$3"),
    };

    const auto ranked = rank_sessions(index, sessions, now);

    EXPECT_EQ(names(ranked), (std::vector<std::string>{"alpha", "beta", "gamma"}));
    EXPECT_DOUBLE_EQ(ranked[0].score, 2.0);
    EXPECT_DOUBLE_EQ(ranked[1].score, 1.0);
    EXPECT_DOUBLE_EQ(ranked[2].score, 0.0);
}

TEST(FrecencyRankTest, LoadRankedSessionsUsesPersistedIndex) {
    constexpr std::int64_t now = 1'700'000'000;
    TempDir tmp;
    const auto store = tmp.path() / "frecency.json";
    Index index;
    index.visit(name("beta"), now);
    ASSERT_TRUE(save_index(index, store).has_value());
    const auto sessions = std::vector<tmux::Session>{
        session("alpha", "$1"),
        session("beta", "$2"),
    };

    auto ranked = load_ranked_sessions(store, sessions, now);

    ASSERT_TRUE(ranked.has_value()) << ranked.error().display();
    EXPECT_EQ(names(*ranked), (std::vector<std::string>{"beta", "alpha"}));
}

TEST(FrecencyRankTest, RecordSessionVisitCreatesStore) {
    constexpr std::int64_t now = 1'700'000'000;
    TempDir tmp;
    const auto store = tmp.path() / "nested" / "frecency.json";
    const auto session_name = name("dotfiles");

    auto recorded = record_session_visit(store, session_name, now);

    ASSERT_TRUE(recorded.has_value()) << recorded.error().display();
    auto loaded = load_index(store);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().display();
    const auto it = loaded->entries().find(session_name);
    ASSERT_NE(it, loaded->entries().end());
    EXPECT_EQ(it->second.visits, 1U);
    EXPECT_EQ(it->second.last_used, now);
}

TEST(FrecencyRankTest, RecordSessionVisitUpdatesExistingStoreUnderLock) {
    constexpr std::int64_t now = 1'700'000'000;
    TempDir tmp;
    const auto store = tmp.path() / "frecency.json";
    const auto session_name = name("dotfiles");
    Index index;
    index.visit(session_name, now - 10);
    ASSERT_TRUE(save_index(index, store).has_value());

    auto recorded = record_session_visit(store, session_name, now);

    ASSERT_TRUE(recorded.has_value()) << recorded.error().display();
    auto loaded = load_index(store);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().display();
    const auto it = loaded->entries().find(session_name);
    ASSERT_NE(it, loaded->entries().end());
    EXPECT_EQ(it->second.visits, 2U);
    EXPECT_EQ(it->second.last_used, now);
}

TEST(FrecencyRankTest, RecordSessionVisitLockFailureReturnsErrorWithoutWriting) {
    TempDir tmp;
    const auto store = tmp.path() / "frecency.json";
    auto lock_path = store;
    lock_path += ".lock";
    const int fd = ::open(lock_path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
    ASSERT_NE(fd, -1);
    ASSERT_EQ(::flock(fd, LOCK_EX | LOCK_NB), 0);

    auto recorded = record_session_visit(store, name("dotfiles"), 1'700'000'000);

    ::flock(fd, LOCK_UN);
    ::close(fd);
    ASSERT_FALSE(recorded.has_value());
    EXPECT_EQ(recorded.error().kind(), ErrorKind::kIo);
    EXPECT_FALSE(std::filesystem::exists(store));
}

TEST(FrecencyRankTest, RecordSessionVisitMalformedStoreDoesNotDiscardFile) {
    TempDir tmp;
    const auto store = tmp.path() / "frecency.json";
    {
        std::ofstream out(store);
        out << "{not json";
    }
    const auto before = read_all(store);

    auto recorded = record_session_visit(store, name("dotfiles"), 1'700'000'000);

    ASSERT_FALSE(recorded.has_value());
    EXPECT_EQ(recorded.error().kind(), ErrorKind::kParse);
    EXPECT_EQ(read_all(store), before);
}

}  // namespace
}  // namespace lazytmux::frecency
