#!/usr/bin/env bash
set -euo pipefail
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
# PlatformIO package post-install scripts cannot quote a Python path containing
# spaces reliably. Keep the tool environment outside the project directory.
tool_env="${XDG_DATA_HOME:-$HOME/.local/share}/wxlink-connect/venv"
python3 -m venv "$tool_env"
"$tool_env/bin/python" -m pip install -r "$script_dir/firmware-requirements.txt"
echo 'Firmware tools installed. Build with: bash tools/pio.sh run -d firmware'
