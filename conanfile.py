from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain, cmake_layout


class LazytmuxRecipe(ConanFile):
    name = "lazytmux"
    version = "0.1.0"
    description = "Modern-C++ TUI for tmux"
    license = "MIT"

    settings = "os", "compiler", "build_type", "arch"

    options = {
        "build_tests": [True, False],
        "build_examples": [True, False],
        "enable_asan": [True, False],
        "enable_ubsan": [True, False],
        "enable_tsan": [True, False],
    }

    default_options = {
        "build_tests": True,
        "build_examples": False,
        "enable_asan": False,
        "enable_ubsan": False,
        "enable_tsan": False,
    }

    def requirements(self):
        self.requires("nlohmann_json/3.11.3")
        self.requires("tomlplusplus/3.4.0")

    def build_requirements(self):
        if self.options.build_tests:
            self.test_requires("gtest/1.17.0")

    def layout(self):
        self.folders.build_folder_vars = ["options.enable_tsan"]
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["LAZYTMUX_BUILD_TESTS"] = bool(self.options.build_tests)
        tc.variables["LAZYTMUX_BUILD_EXAMPLES"] = bool(self.options.build_examples)
        tc.variables["LAZYTMUX_ENABLE_ASAN"] = bool(self.options.enable_asan)
        tc.variables["LAZYTMUX_ENABLE_UBSAN"] = bool(self.options.enable_ubsan)
        tc.variables["LAZYTMUX_ENABLE_TSAN"] = bool(self.options.enable_tsan)
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()
