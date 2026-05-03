#include <lazytmux/exec.hpp>

#include <gtest/gtest.h>
#include <vector>

namespace lazytmux::exec {
namespace {

TEST(ExecTest, EmptyArgvReturnsInvalidInput) {
    const std::vector<std::string> argv;

    auto result = replace_process(argv);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind(), ErrorKind::kInvalidInput);
}

TEST(ExecTest, EmptyExecutableReturnsInvalidInput) {
    const std::vector<std::string> argv{""};

    auto result = replace_process(argv);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind(), ErrorKind::kInvalidInput);
}

}  // namespace
}  // namespace lazytmux::exec
