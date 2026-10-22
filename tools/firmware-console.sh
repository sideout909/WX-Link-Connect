#!/usr/bin/env bash
set -euo pipefail
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
tool_env="${XDG_DATA_HOME:-$HOME/.local/share}/wxlink-connect/venv"
if [[ ! -x "$tool_env/bin/python" ]]; then
    echo 'First run: bash tools/setup-firmware.sh' >&2
    exit 1
fi
exec "$tool_env/bin/python" "$script_dir/firmware_console.py" "$@"
