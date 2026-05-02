#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd "${script_dir}/.." && pwd)"
cd "${repo_dir}"

build_type="${1:-Debug}"
shift || true
ctest_args=("$@")
preset_prefix="conan-enable_tsan_false"
if [[ "${LAZYTMUX_TSAN:-0}" == "1" ]]; then
    preset_prefix="conan-enable_tsan_true"
fi

has_label_filter=0
for arg in "${ctest_args[@]}"; do
    case "${arg}" in
        -L | -LE | --label-regex | --label-exclude | --label-regex=* | --label-exclude=*)
            has_label_filter=1
            ;;
    esac
done

if [[ "${LAZYTMUX_E2E:-0}" != "1" && "${has_label_filter}" == "0" ]]; then
    ctest_args+=("-LE" "live")
fi

ctest --preset "${preset_prefix}-${build_type,,}" --output-on-failure "${ctest_args[@]}"
