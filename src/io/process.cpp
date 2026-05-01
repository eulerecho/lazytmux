#include <lazytmux/io/process.hpp>

#include <array>
#include <cerrno>
#include <cstring>
#include <format>
#include <span>
#include <string>
#include <sys/poll.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace lazytmux::io {
namespace {

class UniqueFd {
public:
    explicit UniqueFd(int fd = -1) noexcept : fd_(fd) {}

    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;

    UniqueFd(UniqueFd&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}

    UniqueFd& operator=(UniqueFd&& other) noexcept {
        if (this != &other) {
            reset(std::exchange(other.fd_, -1));
        }
        return *this;
    }

    ~UniqueFd() {
        reset();
    }

    [[nodiscard]] int get() const noexcept {
        return fd_;
    }

    [[nodiscard]] int release() noexcept {
        return std::exchange(fd_, -1);
    }

    void reset(int fd = -1) noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
        }
        fd_ = fd;
    }

private:
    int fd_;
};

std::string errno_message(std::string_view action) {
    return std::format("{}: {}", action, std::strerror(errno));
}

Result<std::array<UniqueFd, 2>> make_pipe(std::string_view label) {
    std::array<int, 2> raw{-1, -1};
    if (::pipe(raw.data()) != 0) {
        return std::unexpected(
            Error(errno_message(std::format("failed to create {} pipe", label))));
    }
    return std::array<UniqueFd, 2>{UniqueFd{raw[0]}, UniqueFd{raw[1]}};
}

[[noreturn]] void child_exec(std::span<const std::string> argv, int stdout_fd, int stderr_fd) {
    if (::dup2(stdout_fd, STDOUT_FILENO) == -1 || ::dup2(stderr_fd, STDERR_FILENO) == -1) {
        constexpr std::string_view kMsg = "dup2 failed\n";
        static_cast<void>(::write(STDERR_FILENO, kMsg.data(), kMsg.size()));
        ::_exit(127);
    }

    std::vector<char*> exec_argv;
    exec_argv.reserve(argv.size() + 1);
    for (const auto& arg : argv) {
        exec_argv.push_back(const_cast<char*>(arg.c_str()));
    }
    exec_argv.push_back(nullptr);

    ::execvp(exec_argv.front(), exec_argv.data());
    constexpr std::string_view kMsg = "execvp failed\n";
    static_cast<void>(::write(STDERR_FILENO, kMsg.data(), kMsg.size()));
    ::_exit(127);
}

Result<bool> read_once(int fd, std::string& out) {
    std::array<char, 4096> buffer{};
    for (;;) {
        const auto n = ::read(fd, buffer.data(), buffer.size());
        if (n > 0) {
            out.append(buffer.data(), static_cast<std::size_t>(n));
            return true;
        }
        if (n == 0) {
            return false;
        }
        if (errno == EINTR) {
            continue;
        }
        return std::unexpected(Error(errno_message("failed to read child output")));
    }
}

Result<void> capture_output(UniqueFd& stdout_read, UniqueFd& stderr_read, CommandResult& result) {
    std::array<pollfd, 2> fds{
        pollfd{.fd = stdout_read.get(), .events = POLLIN | POLLHUP | POLLERR, .revents = 0},
        pollfd{.fd = stderr_read.get(), .events = POLLIN | POLLHUP | POLLERR, .revents = 0},
    };
    int open = 2;
    while (open > 0) {
        const auto ready = ::poll(fds.data(), fds.size(), -1);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            return std::unexpected(Error(errno_message("failed to poll child output")));
        }

        for (std::size_t i = 0; i < fds.size(); ++i) {
            if (fds[i].fd < 0 || fds[i].revents == 0) {
                continue;
            }
            auto& text = (i == 0) ? result.stdout_text : result.stderr_text;
            auto still_open = read_once(fds[i].fd, text);
            if (!still_open) {
                return std::unexpected(std::move(still_open).error());
            }
            if (!*still_open) {
                if (i == 0) {
                    stdout_read.reset();
                } else {
                    stderr_read.reset();
                }
                fds[i].fd = -1;
                --open;
            }
            fds[i].revents = 0;
        }
    }
    return {};
}

Result<int> wait_for(pid_t pid) {
    int status = 0;
    for (;;) {
        const auto r = ::waitpid(pid, &status, 0);
        if (r == pid) {
            break;
        }
        if (r == -1 && errno == EINTR) {
            continue;
        }
        return std::unexpected(Error(errno_message("failed to wait for child process")));
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return 127;
}

}  // namespace

Result<CommandResult> run_command(std::span<const std::string> argv) {
    if (argv.empty() || argv.front().empty()) {
        return std::unexpected(Error("command argv must contain an executable"));
    }

    auto stdout_pipe = make_pipe("stdout");
    if (!stdout_pipe) {
        return std::unexpected(std::move(stdout_pipe).error());
    }
    auto stderr_pipe = make_pipe("stderr");
    if (!stderr_pipe) {
        return std::unexpected(std::move(stderr_pipe).error());
    }

    const pid_t pid = ::fork();
    if (pid < 0) {
        return std::unexpected(Error(errno_message("failed to fork child process")));
    }
    if (pid == 0) {
        (*stdout_pipe)[0].reset();
        (*stderr_pipe)[0].reset();
        child_exec(argv, (*stdout_pipe)[1].get(), (*stderr_pipe)[1].get());
    }

    (*stdout_pipe)[1].reset();
    (*stderr_pipe)[1].reset();

    CommandResult result;
    auto captured = capture_output((*stdout_pipe)[0], (*stderr_pipe)[0], result);
    auto exit_code = wait_for(pid);
    if (!captured) {
        return std::unexpected(std::move(captured).error());
    }
    if (!exit_code) {
        return std::unexpected(std::move(exit_code).error());
    }
    result.exit_code = *exit_code;
    return result;
}

Result<bool> run_command_status(std::span<const std::string> argv) {
    auto result = run_command(argv);
    if (!result) {
        return std::unexpected(std::move(result).error());
    }
    return result->exit_code == 0;
}

}  // namespace lazytmux::io
