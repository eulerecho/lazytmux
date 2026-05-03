#include <lazytmux/exec.hpp>

#include <cerrno>
#include <cstring>
#include <format>
#include <string_view>
#include <unistd.h>
#include <vector>

namespace lazytmux::exec {

Result<void> replace_process(std::span<const std::string> argv) {
    if (argv.empty() || argv.front().empty()) {
        return std::unexpected(
            Error{ErrorKind::kInvalidInput, "exec argv must contain an executable"});
    }

    std::vector<char*> exec_argv;
    exec_argv.reserve(argv.size() + 1U);
    for (const auto& arg : argv) {
        exec_argv.push_back(const_cast<char*>(arg.c_str()));
    }
    exec_argv.push_back(nullptr);

    ::execvp(exec_argv.front(), exec_argv.data());
    return std::unexpected(Error{
        ErrorKind::kIo,
        std::format("execvp {} failed: {}", argv.front(), std::strerror(errno)),
    });
}

}  // namespace lazytmux::exec
