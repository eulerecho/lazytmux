#include <lazytmux/frecency.hpp>
#include <lazytmux/tmux/ids.hpp>

#include <cstdint>
#include <gtest/gtest.h>
#include <string_view>

namespace lazytmux::frecency {
namespace {

tmux::SessionName name(std::string_view s) {
    auto r = tmux::SessionName::make(s);
    return std::move(r).value();
}

constexpr std::int64_t kSecondsPerDay = 86400;
constexpr std::int64_t kT = 1'700'000'000;

TEST(FrecencyIndexTest, EmptyIndexScoresZero) {
    Index idx;
    EXPECT_DOUBLE_EQ(idx.score(name("dotfiles"), kT), 0.0);
}

TEST(FrecencyIndexTest, FreshVisitScoresOneVisit) {
    Index idx;
    auto n = name("dotfiles");
    idx.visit(n, kT);
    EXPECT_DOUBLE_EQ(idx.score(n, kT), 1.0);
}

TEST(FrecencyIndexTest, AgedVisitDecaysBelowOneVisit) {
    Index idx;
    auto n = name("dotfiles");
    idx.visit(n, kT);
    const double s = idx.score(n, kT + 30 * kSecondsPerDay);
    EXPECT_GT(s, 0.05);
    EXPECT_LT(s, 0.10);
}

TEST(FrecencyIndexTest, OneHalfLifeHalvesASingleVisitScore) {
    Index idx;
    auto n = name("dotfiles");
    idx.visit(n, 0);
    EXPECT_NEAR(idx.score(n, 7 * kSecondsPerDay), 0.5, 1e-12);
}

TEST(FrecencyIndexTest, MultipleVisitsIncreaseScoreLinearlyAtZeroAge) {
    Index idx;
    auto n = name("dotfiles");
    idx.visit(n, kT);
    idx.visit(n, kT);
    idx.visit(n, kT);
    EXPECT_DOUBLE_EQ(idx.score(n, kT), 3.0);
}

TEST(FrecencyIndexTest, RevisitResetsLastUsedToLatest) {
    Index idx;
    auto n = name("dotfiles");
    idx.visit(n, 100);
    idx.visit(n, kT);
    const auto& es = idx.entries();
    auto it = es.find(n);
    ASSERT_NE(it, es.end());
    EXPECT_EQ(it->second.visits, 2U);
    EXPECT_EQ(it->second.last_used, kT);
}

TEST(FrecencyIndexTest, UnknownNameScoresZeroEvenWhenOthersPresent) {
    Index idx;
    idx.visit(name("dotfiles"), kT);
    EXPECT_DOUBLE_EQ(idx.score(name("workspace"), kT), 0.0);
}

TEST(FrecencyIndexTest, FutureNowIsClampedToZeroAgeNotNegative) {
    Index idx;
    auto n = name("dotfiles");
    idx.visit(n, kT);
    EXPECT_DOUBLE_EQ(idx.score(n, kT - 100'000'000), 1.0);
}

}  // namespace
}  // namespace lazytmux::frecency
