#include <lazytmux/version.hpp>

#include <gtest/gtest.h>

TEST(VersionTest, IsNonEmpty) {
    EXPECT_FALSE(lazytmux::kVersion.empty());
}
