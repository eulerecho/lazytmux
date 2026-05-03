#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd "${script_dir}/.." && pwd)"
cd "${repo_dir}"
source "${script_dir}/presets.sh"

find_tool() {
    local requested="${1:-}"
    shift

    if [[ -n "${requested}" ]]; then
        if [[ -x "${requested}" ]]; then
            printf "%s\n" "${requested}"
            return 0
        fi
        if command -v "${requested}" >/dev/null 2>&1; then
            command -v "${requested}"
            return 0
        fi
        echo "Requested tool is not executable or on PATH: ${requested}" >&2
        exit 2
    fi

    for candidate in "$@"; do
        if command -v "${candidate}" >/dev/null 2>&1; then
            command -v "${candidate}"
            return 0
        fi
    done

    echo "Missing required LLVM coverage tool. Install clang-19/llvm-19 or set CXX, LLVM_COV, and LLVM_PROFDATA." >&2
    exit 2
}

tool_major_version() {
    local tool="$1"
    "${tool}" --version | sed -nE 's/.*version ([0-9]+).*/\1/p' | head -n 1
}

derive_clang_cc() {
    local clangxx="$1"
    local clangcc="${clangxx/clang++/clang}"

    if [[ "${clangcc}" != "${clangxx}" && -x "${clangcc}" ]]; then
        printf "%s\n" "${clangcc}"
        return 0
    fi

    find_tool "${CC:-}" clang-19 clang clang-18 clang-17 clang-16
}

stdlib_flag() {
    case "$1" in
        libc++)
            printf "%s\n" "-stdlib=libc++"
            ;;
        libstdc++ | libstdc++11)
            printf "%s\n" "-stdlib=libstdc++"
            ;;
        *)
            echo "Unsupported Conan compiler.libcxx value for Clang coverage: $1" >&2
            exit 2
            ;;
    esac
}

check_expected_support() {
    local clangxx="$1"
    local libcxx="$2"
    local output="${TMPDIR:-/tmp}/lazytmux-coverage-expected-check"

    if ! printf '#include <charconv>\n#include <expected>\n#include <string_view>\n#include <system_error>\n#include <thread>\nint main(){std::expected<int,int> x=1; std::jthread t([](std::stop_token){}); double d=0; auto s=std::string_view{"1.25"}; auto r=std::from_chars(s.data(), s.data()+s.size(), d); return r.ec == std::errc{} && r.ptr != s.data() && *x == 1 ? 0 : 1;}\n' \
        | "${clangxx}" -std=c++23 "$(stdlib_flag "${libcxx}")" -x c++ - -o "${output}" \
            >/dev/null 2>&1; then
        cat >&2 <<EOF
Clang coverage requires a C++23 standard library with std::expected, std::jthread,
std::stop_token, and floating-point std::from_chars.
Install Clang/LLVM 19 with a compatible libstdc++ toolchain, or set
LAZYTMUX_CONAN_COMPILER_LIBCXX to a compatible library.
On Ubuntu 24.04: sudo apt-get install clang-19 llvm-19 g++
EOF
        exit 2
    fi
}

select_coverage_libcxx() {
    local clangxx="$1"

    if [[ -n "${LAZYTMUX_CONAN_COMPILER_LIBCXX:-}" ]]; then
        check_expected_support "${clangxx}" "${LAZYTMUX_CONAN_COMPILER_LIBCXX}"
        printf "%s\n" "${LAZYTMUX_CONAN_COMPILER_LIBCXX}"
        return 0
    fi

    for candidate in libstdc++11 libstdc++ libc++; do
        if printf '#include <charconv>\n#include <expected>\n#include <string_view>\n#include <system_error>\n#include <thread>\nint main(){std::expected<int,int> x=1; std::jthread t([](std::stop_token){}); double d=0; auto s=std::string_view{"1.25"}; auto r=std::from_chars(s.data(), s.data()+s.size(), d); return r.ec == std::errc{} && r.ptr != s.data() && *x == 1 ? 0 : 1;}\n' \
            | "${clangxx}" -std=c++23 "$(stdlib_flag "${candidate}")" -x c++ - \
                -o "${TMPDIR:-/tmp}/lazytmux-coverage-expected-check" \
                >/dev/null 2>&1; then
            printf "%s\n" "${candidate}"
            return 0
        fi
    done

    check_expected_support "${clangxx}" libc++
}

if lazytmux_enabled "${LAZYTMUX_TSAN:-0}"; then
    echo "Coverage and TSan must use separate builds. Unset LAZYTMUX_TSAN for coverage."
    exit 2
fi

build_type="${1:-Debug}"
shift || true
ctest_args=("$@")

clangxx="$(find_tool "${CXX:-}" clang++-19 clang++ clang++-18 clang++-17 clang++-16)"
clangcc="$(derive_clang_cc "${clangxx}")"
clang_major="$(tool_major_version "${clangxx}")"
if [[ -z "${clang_major}" ]]; then
    echo "Could not determine Clang major version from: ${clangxx}" >&2
    exit 2
fi
llvm_cov="$(find_tool "${LLVM_COV:-}" "llvm-cov-${clang_major}" llvm-cov llvm-cov-19 llvm-cov-18 llvm-cov-17 llvm-cov-16)"
llvm_profdata="$(find_tool "${LLVM_PROFDATA:-}" "llvm-profdata-${clang_major}" llvm-profdata llvm-profdata-19 llvm-profdata-18 llvm-profdata-17 llvm-profdata-16)"
llvm_cov_major="$(tool_major_version "${llvm_cov}")"
llvm_profdata_major="$(tool_major_version "${llvm_profdata}")"
if [[ "${llvm_cov_major}" != "${clang_major}" || "${llvm_profdata_major}" != "${clang_major}" ]]; then
    echo "Coverage requires matching Clang/LLVM tools. Found Clang ${clang_major}, llvm-cov ${llvm_cov_major}, llvm-profdata ${llvm_profdata_major}." >&2
    exit 2
fi
coverage_libcxx="$(select_coverage_libcxx "${clangxx}")"
echo "Using coverage toolchain: CXX=${clangxx}, llvm-cov=${llvm_cov}, stdlib=${coverage_libcxx}"

export CC="${clangcc}"
export CXX="${clangxx}"
export LAZYTMUX_COVERAGE=1
export LAZYTMUX_CONAN_COMPILER=clang
export LAZYTMUX_CONAN_COMPILER_VERSION="${clang_major}"
export LAZYTMUX_CONAN_COMPILER_LIBCXX="${coverage_libcxx}"

coverage_build_dir="${repo_dir}/build/$(lazytmux_variant_dir)/${build_type}"
if [[ -f "${coverage_build_dir}/CMakeCache.txt" ]]; then
    cmake -E rm -rf "${coverage_build_dir}"
fi

"${script_dir}/configure.sh" "${build_type}"
"${script_dir}/build.sh" "${build_type}"

coverage_output_dir="${repo_dir}/build/coverage"
html_output_dir="${coverage_output_dir}/html"
profile_output_dir="${coverage_output_dir}/profiles"
profile_data_path="${coverage_output_dir}/coverage.profdata"
lcov_output_path="${coverage_output_dir}/lcov.info"
summary_output_path="${coverage_output_dir}/summary.txt"

rm -rf "${html_output_dir}" "${profile_output_dir}"
mkdir -p "${html_output_dir}" "${profile_output_dir}"

export LLVM_PROFILE_FILE="${profile_output_dir}/%p-%m.profraw"
"${script_dir}/test.sh" "${build_type}" "${ctest_args[@]}"

shopt -s nullglob
profraw_files=("${profile_output_dir}"/*.profraw)
shopt -u nullglob
if [[ "${#profraw_files[@]}" -eq 0 ]]; then
    echo "No raw coverage profiles were produced under ${profile_output_dir}" >&2
    exit 2
fi

"${llvm_profdata}" merge -sparse "${profraw_files[@]}" -o "${profile_data_path}"

objects=()
for object in \
    "${coverage_build_dir}/test_scaffold" \
    "${coverage_build_dir}/test_live_tmux" \
    "${coverage_build_dir}/lazytmux"; do
    if [[ -x "${object}" ]]; then
        objects+=("${object}")
    fi
done

if [[ "${#objects[@]}" -eq 0 ]]; then
    echo "No coverage objects were found under ${coverage_build_dir}" >&2
    exit 2
fi

first_object="${objects[0]}"
object_args=()
for object in "${objects[@]:1}"; do
    object_args+=("-object=${object}")
done

ignore_regex="(^/usr/|/.conan2/|/tests/|/build/)"
"${llvm_cov}" report "${first_object}" "${object_args[@]}" \
    -instr-profile="${profile_data_path}" \
    -ignore-filename-regex="${ignore_regex}"
"${llvm_cov}" report "${first_object}" "${object_args[@]}" \
    -instr-profile="${profile_data_path}" \
    -ignore-filename-regex="${ignore_regex}" \
    > "${summary_output_path}"
"${llvm_cov}" show "${first_object}" "${object_args[@]}" \
    -instr-profile="${profile_data_path}" \
    -ignore-filename-regex="${ignore_regex}" \
    -format=html \
    -output-dir="${html_output_dir}"
"${llvm_cov}" export "${first_object}" "${object_args[@]}" \
    -instr-profile="${profile_data_path}" \
    -ignore-filename-regex="${ignore_regex}" \
    -format=lcov \
    > "${lcov_output_path}"

echo "HTML coverage report: ${html_output_dir}/index.html"
echo "LCOV coverage report: ${lcov_output_path}"
