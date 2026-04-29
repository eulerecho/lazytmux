#include <lazytmux/version.hpp>

#include <format>
#include <iostream>
#include <string_view>

namespace {

constexpr std::string_view kUsage = "usage: lazytmux [--version]\n";

}  // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view{argv[1]} == "--version") {
        std::cout << std::format("lazytmux {}\n", lazytmux::kVersion);
        return 0;
    }
    std::cout << kUsage;
    return argc == 1 ? 0 : 2;
}
