#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd "${script_dir}/.." && pwd)"
cd "${repo_dir}"

build_type="${1:-Debug}"
preset_prefix="conan-enable_tsan_false"
if [[ "${LAZYTMUX_TSAN:-0}" == "1" ]]; then
    preset_prefix="conan-enable_tsan_true"
fi
cmake --build --preset "${preset_prefix}-${build_type,,}"
