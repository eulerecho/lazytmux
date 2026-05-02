#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd "${script_dir}/.." && pwd)"
cd "${repo_dir}"
source "${script_dir}/presets.sh"

build_type="${1:-Debug}"
cmake --build --preset "$(lazytmux_preset_name "${build_type}")"
