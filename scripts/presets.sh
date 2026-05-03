#!/usr/bin/env bash

lazytmux_enabled() {
    case "${1:-0}" in
        1 | ON | On | on | TRUE | True | true | YES | Yes | yes)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

lazytmux_variant_dir() {
    local tsan="false"
    local coverage="false"

    if lazytmux_enabled "${LAZYTMUX_TSAN:-0}"; then
        tsan="true"
    fi
    if lazytmux_enabled "${LAZYTMUX_COVERAGE:-0}"; then
        coverage="true"
    fi

    printf "enable_tsan_%s-enable_coverage_%s" "${tsan}" "${coverage}"
}

lazytmux_preset_prefix() {
    local variant_dir
    variant_dir="$(lazytmux_variant_dir)"
    printf "conan-%s" "${variant_dir}"
}

lazytmux_preset_name() {
    local build_type="${1:-Debug}"
    printf "%s-%s" "$(lazytmux_preset_prefix)" "${build_type,,}"
}
