#include <lazytmux/frecency_store.hpp>

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <unistd.h>

namespace lazytmux::frecency {
namespace {

class TempDir {
public:
    TempDir() {
        path_ = std::filesystem::temp_directory_path() /
                ("lazytmux-frecency-test-" + std::to_string(::getpid()));
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

tmux::SessionName name(std::string_view s) {
    auto r = tmux::SessionName::make(s);
    return std::move(r).value();
}

TEST(FrecencyStoreTest, MissingFileLoadsEmptyIndex) {
    TempDir tmp;
    auto loaded = load_index(tmp.path() / "missing.json");
    ASSERT_TRUE(loaded.has_value()) << loaded.error().display();
    EXPECT_TRUE(loaded->entries().empty());
}

TEST(FrecencyStoreTest, SaveThenLoadRoundTripsEntries) {
    TempDir tmp;
    Index index;
    auto dotfiles = name("dotfiles");
    index.visit(dotfiles, 100);
    index.visit(dotfiles, 200);

    const auto path = tmp.path() / "nested" / "frecency.json";
    auto saved = save_index(index, path);
    ASSERT_TRUE(saved.has_value()) << saved.error().display();

    auto loaded = load_index(path);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().display();
    const auto it = loaded->entries().find(dotfiles);
    ASSERT_NE(it, loaded->entries().end());
    EXPECT_EQ(it->second.visits, 2u);
    EXPECT_EQ(it->second.last_used, 200);
}

TEST(FrecencyStoreTest, InvalidJsonReturnsError) {
    TempDir tmp;
    const auto path = tmp.path() / "frecency.json";
    std::ofstream out(path);
    out << "{not json";
    out.close();

    auto loaded = load_index(path);
    ASSERT_FALSE(loaded.has_value());
    EXPECT_NE(loaded.error().message().find("parse"), std::string_view::npos);
}

TEST(FrecencyStoreTest, InvalidSessionNameReturnsError) {
    TempDir tmp;
    const auto path = tmp.path() / "frecency.json";
    std::ofstream out(path);
    out << R"({"sessions":[{"name":"","visits":1,"last_used":2}]})";
    out.close();

    auto loaded = load_index(path);
    ASSERT_FALSE(loaded.has_value());
    EXPECT_NE(loaded.error().message().find("empty"), std::string_view::npos);
}

}  // namespace
}  // namespace lazytmux::frecency
