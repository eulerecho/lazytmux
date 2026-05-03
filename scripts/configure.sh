#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd "${script_dir}/.." && pwd)"
cd "${repo_dir}"
source "${script_dir}/presets.sh"

build_type="${1:-Debug}"

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

    echo "Missing required compiler. Install clang-19, or set CC and CXX." >&2
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

    find_tool "${CC:-}" clang-19 clang
}

if [[ -z "${LAZYTMUX_CONAN_COMPILER:-}" ]]; then
    clangxx="$(find_tool "${CXX:-}" clang++-19 clang++)"
    clangcc="$(derive_clang_cc "${clangxx}")"
    clang_major="$(tool_major_version "${clangxx}")"
    if [[ -z "${clang_major}" ]]; then
        echo "Could not determine Clang major version from: ${clangxx}" >&2
        exit 2
    fi

    export CC="${clangcc}"
    export CXX="${clangxx}"
    export LAZYTMUX_CONAN_COMPILER=clang
    export LAZYTMUX_CONAN_COMPILER_VERSION="${LAZYTMUX_CONAN_COMPILER_VERSION:-${clang_major}}"
    export LAZYTMUX_CONAN_COMPILER_LIBCXX="${LAZYTMUX_CONAN_COMPILER_LIBCXX:-libstdc++11}"
fi

if [[ "${LAZYTMUX_SKIP_UV_SYNC:-0}" != "1" ]]; then
    uv sync --group dev
fi

conan_settings=(-s build_type="${build_type}" -s compiler.cppstd=23)
if [[ -n "${LAZYTMUX_CONAN_COMPILER:-}" ]]; then
    conan_settings+=(-s compiler="${LAZYTMUX_CONAN_COMPILER}")
fi
if [[ -n "${LAZYTMUX_CONAN_COMPILER_VERSION:-}" ]]; then
    conan_settings+=(-s compiler.version="${LAZYTMUX_CONAN_COMPILER_VERSION}")
fi
if [[ -n "${LAZYTMUX_CONAN_COMPILER_LIBCXX:-}" ]]; then
    conan_settings+=(-s compiler.libcxx="${LAZYTMUX_CONAN_COMPILER_LIBCXX}")
fi

conan_extra=()
if lazytmux_enabled "${LAZYTMUX_TSAN:-0}"; then
    conan_extra+=(-o "&:enable_tsan=True")
fi
if lazytmux_enabled "${LAZYTMUX_COVERAGE:-0}"; then
    conan_extra+=(-o "&:enable_coverage=True")
fi

build_dir="${repo_dir}/build/$(lazytmux_variant_dir)/${build_type}"
cache_path="${build_dir}/CMakeCache.txt"
if [[ -f "${cache_path}" && -n "${CXX:-}" ]]; then
    cached_cxx="$(sed -nE 's/^CMAKE_CXX_COMPILER:FILEPATH=(.*)$/\1/p' "${cache_path}" | head -n 1)"
    if [[ -n "${cached_cxx}" && "${cached_cxx}" != "${CXX}" ]]; then
        cmake -E rm -rf "${build_dir}"
    fi
fi

uv run conan install . "${conan_settings[@]}" "${conan_extra[@]}" --build=missing
cmake --preset "$(lazytmux_preset_name "${build_type}")"
