#!/usr/bin/env bash
set -euo pipefail
tool_env="${XDG_DATA_HOME:-$HOME/.local/share}/wxlink-connect/venv"
if [[ ! -x "$tool_env/bin/pio" ]]; then
    echo 'First run: bash tools/setup-firmware.sh' >&2
    exit 1
fi
exec "$tool_env/bin/pio" "$@"
