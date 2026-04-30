cmake_minimum_required(VERSION 3.24)

project(lazytmux
    VERSION 0.1.0
    DESCRIPTION "Modern-C++ TUI for tmux"
    LANGUAGES CXX
)

option(LAZYTMUX_BUILD_TESTS "Build tests" ON)
option(LAZYTMUX_BUILD_EXAMPLES "Build examples" OFF)
option(LAZYTMUX_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(LAZYTMUX_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(LAZYTMUX_ENABLE_TSAN "Enable ThreadSanitizer" OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(LazytmuxWarnings)
include(LazytmuxSanitizers)

find_package(nlohmann_json CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)

add_library(lazytmux_core STATIC)
add_library(lazytmux::core ALIAS lazytmux_core)

target_sources(lazytmux_core
    PRIVATE
        src/error.cpp
        src/tmux/ids.cpp
        src/tmux/parse.cpp
        src/version.cpp
)

target_include_directories(lazytmux_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(lazytmux_core PUBLIC cxx_std_23)
lazytmux_apply_warnings(lazytmux_core)
lazytmux_apply_sanitizers(lazytmux_core)

add_executable(lazytmux src/main.cpp)
target_link_libraries(lazytmux PRIVATE lazytmux::core)
target_compile_features(lazytmux PRIVATE cxx_std_23)
lazytmux_apply_warnings(lazytmux)
lazytmux_apply_sanitizers(lazytmux)

if(LAZYTMUX_BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)
    enable_testing()
    include(GoogleTest)

    add_executable(test_scaffold
        tests/test_error.cpp
        tests/test_ids.cpp
        tests/test_parse.cpp
        tests/test_smoke.cpp
        tests/test_types.cpp
    )
    target_link_libraries(test_scaffold
        PRIVATE
            lazytmux::core
            GTest::gtest
            GTest::gtest_main
    )
    target_compile_features(test_scaffold PRIVATE cxx_std_23)
    lazytmux_apply_warnings(test_scaffold)
    lazytmux_apply_sanitizers(test_scaffold)

    gtest_discover_tests(test_scaffold
        DISCOVERY_MODE PRE_TEST
    )
endif()

if(LAZYTMUX_BUILD_EXAMPLES)
    add_custom_target(examples
        COMMAND ${CMAKE_COMMAND} -E echo "Examples are not configured yet."
    )
endif()
