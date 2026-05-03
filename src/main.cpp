#include <lazytmux/cli_commands.hpp>

#include <iostream>
#include <string_view>
#include <vector>

int main(int argc, char** argv) {
    std::vector<std::string_view> args;
    args.reserve(argc > 0 ? static_cast<std::size_t>(argc - 1) : 0U);
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    auto dependencies = lazytmux::cli::default_command_dependencies();
    const lazytmux::cli::OutputSinks sinks{
        .out = [](std::string_view text) { std::cout << text; },
        .err = [](std::string_view text) { std::cerr << text; },
    };
    return lazytmux::cli::run_args(args, dependencies, sinks);
}
