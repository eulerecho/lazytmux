#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd "${script_dir}/.." && pwd)"
cd "${repo_dir}"

build_type="${1:-Debug}"

if [[ "${LAZYTMUX_SKIP_UV_SYNC:-0}" != "1" ]]; then
    uv sync --group dev
fi

conan_extra=()
preset_prefix="conan-enable_tsan_false"
if [[ "${LAZYTMUX_TSAN:-0}" == "1" ]]; then
    conan_extra+=(-o "&:enable_tsan=True")
    preset_prefix="conan-enable_tsan_true"
fi

uv run conan install . -s build_type="${build_type}" -s compiler.cppstd=23 \
    "${conan_extra[@]}" --build=missing
cmake --preset "${preset_prefix}-${build_type,,}"
