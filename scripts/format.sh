#!/usr/bin/env bash
set -euo pipefail

mode="${1:-fix}"

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd "${script_dir}/.." && pwd)"
cd "${repo_dir}"

if [[ -x ".venv/bin/clang-format" ]]; then
    formatter=(".venv/bin/clang-format")
elif command -v clang-format >/dev/null 2>&1; then
    formatter=("clang-format")
else
    echo "clang-format not found. Run ./scripts/configure.sh to install dev tools with uv." >&2
    exit 127
fi

mapfile -t files < <(find include src tests -type f \
    \( -name '*.hpp' -o -name '*.cpp' -o -name '*.h' -o -name '*.cc' \) 2>/dev/null | sort)

if [[ "${#files[@]}" -eq 0 ]]; then
    exit 0
fi

if [[ "${mode}" == "--check" || "${mode}" == "check" ]]; then
    "${formatter[@]}" --dry-run --Werror "${files[@]}"
else
    "${formatter[@]}" -i "${files[@]}"
fi
