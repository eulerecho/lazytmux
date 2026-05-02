#include <lazytmux/tmux/discovery.hpp>

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <optional>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <system_error>
#include <unistd.h>

namespace lazytmux::tmux {
namespace {

class TempDir {
public:
    TempDir() {
        path_ = std::filesystem::temp_directory_path() /
                ("lazytmux-socket-test-" + std::to_string(::getpid()));
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

class BoundSocket {
public:
    explicit BoundSocket(const std::filesystem::path& path)
        : fd_(::socket(AF_UNIX, SOCK_STREAM, 0)) {
        if (fd_ < 0) {
            throw std::system_error(errno, std::generic_category(), "socket");
        }

        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        const auto text = path.string();
        if (text.size() >= sizeof(addr.sun_path)) {
            throw std::runtime_error("socket path too long");
        }
        std::strncpy(addr.sun_path, text.c_str(), sizeof(addr.sun_path) - 1);
        if (::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            throw std::system_error(errno, std::generic_category(), "bind");
        }
    }

    BoundSocket(const BoundSocket&) = delete;
    BoundSocket& operator=(const BoundSocket&) = delete;

    ~BoundSocket() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }

private:
    int fd_;
};

TEST(SocketDiscoveryTest, ListsOnlyUnixSocketsSortedByPath) {
    TempDir tmp;
    std::optional<BoundSocket> zed;
    std::optional<BoundSocket> alpha;
    try {
        zed.emplace(tmp.path() / "z");
        alpha.emplace(tmp.path() / "alpha");
    } catch (const std::system_error& e) {
        if (e.code() == std::errc::operation_not_permitted ||
            e.code() == std::errc::permission_denied) {
            GTEST_SKIP() << "Unix socket bind is not permitted in this environment";
        }
        throw;
    }
    std::ofstream regular(tmp.path() / "regular");
    std::filesystem::create_directory(tmp.path() / "subdir");

    auto sockets = list_sockets_in(tmp.path());
    ASSERT_TRUE(sockets.has_value()) << sockets.error().display();
    ASSERT_EQ(sockets->size(), 2u);
    EXPECT_EQ((*sockets)[0].name, "alpha");
    EXPECT_EQ((*sockets)[1].name, "z");
}

TEST(SocketDiscoveryTest, MissingDirectoryReturnsError) {
    TempDir tmp;
    auto sockets = list_sockets_in(tmp.path() / "missing");
    ASSERT_FALSE(sockets.has_value());
    EXPECT_NE(sockets.error().message().find("socket directory"), std::string_view::npos);
}

TEST(SocketDiscoveryTest, FormatsNamedSocketPath) {
    EXPECT_EQ(socket_path_for_name(1000, "default"),
              std::filesystem::path("/tmp/tmux-1000/default"));
}

}  // namespace
}  // namespace lazytmux::tmux
