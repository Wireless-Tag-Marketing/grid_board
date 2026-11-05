#!/usr/bin/env bash
# Uses an isolated shell environment; does not change the IDE's selected SDK.
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
original_path="$PATH"
if [[ -n "${GRID_BOARD_IDF_PATH:-}" ]]; then
    export IDF_PATH="$GRID_BOARD_IDF_PATH"
elif [[ -f "$HOME/.espressif/v6.1.0/esp-idf/export.sh" ]]; then
    export IDF_PATH="$HOME/.espressif/v6.1.0/esp-idf"
elif [[ -z "${IDF_PATH:-}" ]]; then
    echo 'Set GRID_BOARD_IDF_PATH to an ESP-IDF 6.1.x installation.' >&2
    exit 1
fi
# Read EIM's installed tool inventory without invoking `eim select` or changing
# global IDE state. Its -e mode prints environment pairs and has no side effects.
activation="$HOME/.espressif/tools/activate_idf_v6.1.0.sh"
if [[ -f "$activation" && -z "${GRID_BOARD_IDF_PATH:-}" ]]; then
    while IFS= read -r pair; do
        case "$pair" in
            PATH=*|IDF_*=*|ESP_IDF_VERSION=*|ESP_ROM_ELF_DIR=*|OPENOCD_SCRIPTS=*) export "$pair" ;;
        esac
    done < <(bash "$activation" -e)
    export PATH="$PATH:$original_path"
else
    # shellcheck disable=SC1091
    source "$IDF_PATH/export.sh" >/dev/null
fi
export ESP_IDF_VERSION=6.1
cd "$project_dir"
if [[ "${1:-}" == 'package' ]]; then
    if [[ $# -ne 1 ]]; then
        echo 'Usage: bash tools/idf.sh package' >&2
        exit 1
    fi
    python "$IDF_PATH/tools/idf.py" build
    exec python "$project_dir/tools/package_firmware.py"
fi
exec python "$IDF_PATH/tools/idf.py" "$@"
