#include <lazytmux/snapshots/timestamp.hpp>

#include <gtest/gtest.h>
#include <string>
#include <string_view>

namespace lazytmux::snapshots {
namespace {

TEST(SnapshotTimestampTest, ParsesValidFilename) {
    auto r = parse_snapshot_filename("tmux_resurrect_20260415T004643.txt");
    ASSERT_TRUE(r.has_value()) << r.error().display();
    EXPECT_EQ(r->year, 2026u);
    EXPECT_EQ(r->month, 4u);
    EXPECT_EQ(r->day, 15u);
    EXPECT_EQ(r->hour, 0u);
    EXPECT_EQ(r->minute, 46u);
    EXPECT_EQ(r->second, 43u);
}

TEST(SnapshotTimestampTest, ParsesMidnight) {
    auto r = parse_snapshot_filename("tmux_resurrect_20260101T000000.txt");
    ASSERT_TRUE(r.has_value()) << r.error().display();
    EXPECT_EQ(r->year, 2026u);
    EXPECT_EQ(r->month, 1u);
    EXPECT_EQ(r->day, 1u);
    EXPECT_EQ(r->hour, 0u);
    EXPECT_EQ(r->minute, 0u);
    EXPECT_EQ(r->second, 0u);
}

TEST(SnapshotTimestampTest, RejectsMissingPrefix) {
    auto r = parse_snapshot_filename("session-list.txt");
    ASSERT_FALSE(r.has_value());
    EXPECT_NE(r.error().message().find("missing prefix"), std::string_view::npos);
}

TEST(SnapshotTimestampTest, RejectsMissingTSeparator) {
    auto r = parse_snapshot_filename("tmux_resurrect_20260415X004643.txt");
    ASSERT_FALSE(r.has_value());
    EXPECT_NE(r.error().message().find("missing 'T'"), std::string_view::npos);
}

TEST(SnapshotTimestampTest, RejectsNonNumericField) {
    auto r = parse_snapshot_filename("tmux_resurrect_2026XX15T004643.txt");
    ASSERT_FALSE(r.has_value());
    EXPECT_NE(r.error().message().find("month"), std::string_view::npos);
}

TEST(SnapshotTimestampTest, DisplayFormatsZeroPadded) {
    Timestamp t{.year = 2026, .month = 4, .day = 15, .hour = 0, .minute = 46, .second = 43};
    EXPECT_EQ(t.display(), "2026-04-15 00:46:43");
}

TEST(SnapshotTimestampTest, OrdersChronologically) {
    auto a = parse_snapshot_filename("tmux_resurrect_20260101T000000.txt");
    auto b = parse_snapshot_filename("tmux_resurrect_20260415T004643.txt");
    ASSERT_TRUE(a.has_value() && b.has_value());
    EXPECT_LT(*a, *b);
}

}  // namespace
}  // namespace lazytmux::snapshots
