#include <lazytmux/nesting.hpp>

#include <cstdint>
#include <gtest/gtest.h>
#include <optional>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lazytmux::nesting {
namespace {

template <typename Range>
auto make_walker(const Range& rows) {
    std::unordered_map<std::uint32_t, std::pair<std::uint32_t, bool>> map;
    for (const auto& row : rows) {
        const auto [pid, parent, is_tmux] = row;
        map.try_emplace(pid, std::pair{parent, is_tmux});
    }
    return
        [m = std::move(map)](std::uint32_t pid) -> std::optional<std::pair<std::uint32_t, bool>> {
            if (auto it = m.find(pid); it != m.end()) {
                return it->second;
            }
            return std::nullopt;
        };
}

using Row = std::tuple<std::uint32_t, std::uint32_t, bool>;

TEST(NestingWalkTest, EmptyWalkerYieldsZero) {
    auto walker = [](std::uint32_t) -> std::optional<std::pair<std::uint32_t, bool>> {
        return std::nullopt;
    };
    EXPECT_EQ(count_tmux_in_walk(100, walker), 0u);
}

TEST(NestingWalkTest, SingleTmuxAncestorYieldsOne) {
    std::vector<Row> rows{{100, 99, false}, {99, 50, false}, {50, 1, true}};
    auto walker = make_walker(rows);
    EXPECT_EQ(count_tmux_in_walk(100, walker), 1u);
}

TEST(NestingWalkTest, TwoTmuxAncestorsYieldsTwo) {
    std::vector<Row> rows{
        {100, 99, false},
        {99, 50, false},
        {50, 40, true},
        {40, 30, false},
        {30, 1, true},
    };
    auto walker = make_walker(rows);
    EXPECT_EQ(count_tmux_in_walk(100, walker), 2u);
}

TEST(NestingWalkTest, NoTmuxAncestorsYieldsZero) {
    std::vector<Row> rows{{100, 99, false}, {99, 50, false}, {50, 1, false}};
    auto walker = make_walker(rows);
    EXPECT_EQ(count_tmux_in_walk(100, walker), 0u);
}

TEST(NestingWalkTest, SelfLoopHaltsAtLoopGuard) {
    std::vector<Row> rows{{100, 100, true}};
    auto walker = make_walker(rows);
    EXPECT_EQ(count_tmux_in_walk(100, walker), 1u);
}

TEST(NestingWalkTest, CycleInChainHaltsAtVisitedSet) {
    std::vector<Row> rows{{100, 99, true}, {99, 100, false}};
    auto walker = make_walker(rows);
    EXPECT_EQ(count_tmux_in_walk(100, walker), 1u);
}

TEST(NestingWalkTest, DeepChainHaltsAtSafetyCap) {
    std::vector<Row> rows;
    rows.reserve(100);
    for (std::uint32_t p = 1; p <= 100; ++p) {
        rows.emplace_back(p, p + 1, true);
    }
    auto walker = make_walker(rows);
    EXPECT_EQ(count_tmux_in_walk(1, walker), kMaxWalk);
}

TEST(NestingWalkTest, ParentZeroHaltsWalk) {
    std::vector<Row> rows{{100, 1, false}, {1, 0, true}};
    auto walker = make_walker(rows);
    EXPECT_EQ(count_tmux_in_walk(100, walker), 1u);
}

TEST(ParsePpidTest, FindsFieldInRealProcFormat) {
    constexpr std::string_view kBody =
        "Name:\tbash\n"
        "Umask:\t0022\n"
        "State:\tS (sleeping)\n"
        "Tgid:\t1234\n"
        "Ngid:\t0\n"
        "Pid:\t1234\n"
        "PPid:\t6789\n"
        "TracerPid:\t0\n";
    auto r = parse_ppid(kBody);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, 6789u);
}

TEST(ParsePpidTest, ReturnsNulloptWhenFieldAbsent) {
    EXPECT_FALSE(parse_ppid("Name:\tbash\nState:\tS\n").has_value());
}

TEST(ParsePpidTest, HandlesTabOrSpaceSeparator) {
    auto a = parse_ppid("PPid:\t42\n");
    auto b = parse_ppid("PPid: 42\n");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_EQ(*a, 42u);
    EXPECT_EQ(*b, 42u);
}

}  // namespace
}  // namespace lazytmux::nesting
