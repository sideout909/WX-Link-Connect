#!/usr/bin/env bash
set -euo pipefail
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
project_dir=$(dirname -- "$script_dir")
test_binary=$(mktemp /tmp/wxlink-connect-test.XXXXXX)
trap 'rm -f -- "$test_binary" "$test_binary.data.o" "$test_binary.abuf.o"' EXIT
g++ -std=c++17 -Wall -Wextra -Werror \
    -I"$project_dir/firmware/include" \
    "$project_dir/firmware/tests/capture_session_test.cpp" -o "$test_binary"
"$test_binary"
g++ -std=c++17 -Wall -Wextra -Werror \
    -I"$project_dir/firmware/include" \
    "$project_dir/firmware/tests/connect_migration_policy_test.cpp" -o "$test_binary"
"$test_binary"
"$script_dir/check-decoder-profile.py"
g++ -std=c++17 -Wall -Wextra -Werror \
    -I"$project_dir/firmware/include" \
    "$project_dir/firmware/tests/connect_decoder_profile_test.cpp" -o "$test_binary"
"$test_binary"
g++ -std=c++17 -Wall -Wextra -Werror \
    -I"$project_dir/firmware/include" \
    "$project_dir/firmware/tests/capture_writer_test.cpp" -o "$test_binary"
"$test_binary"
g++ -std=c++17 -Wall -Wextra -Werror \
    -I"$project_dir/firmware/include" \
    "$project_dir/firmware/tests/capture_lease_test.cpp" -o "$test_binary"
"$test_binary"
g++ -std=c++17 -Wall -Wextra -Werror \
    -I"$project_dir/firmware/include" \
    "$project_dir/firmware/tests/connect_web_security_test.cpp" -o "$test_binary"
"$test_binary"
g++ -std=c++17 -Wall -Wextra -Werror \
    -I"$project_dir/firmware/include" \
    "$project_dir/firmware/tests/capture_pulse_test.cpp" -o "$test_binary"
"$test_binary"
g++ -std=c++17 -Wall -Wextra -Werror \
    -I"$project_dir/firmware/include" -I"$project_dir/vendor/ArduinoJson/src" \
    "$project_dir/firmware/tests/preview_adapter_test.cpp" -o "$test_binary"
"$test_binary"
g++ -std=c++17 -Wall -Wextra -Werror \
    -I"$project_dir/firmware/include" \
    "$project_dir/firmware/tests/capture_storage_format_test.cpp" -o "$test_binary"
"$test_binary"
g++ -std=c++17 -Wall -Wextra -Werror \
    -I"$project_dir/firmware/include" \
    "$project_dir/firmware/tests/setup_restart_test.cpp" -o "$test_binary"
"$test_binary"
g++ -std=c++17 -Wall -Wextra -Werror \
    -I"$project_dir/firmware/include" -I"$project_dir/vendor/ArduinoJson/src" \
    "$project_dir/firmware/tests/connect_preview_pipeline_test.cpp" -o "$test_binary"
"$test_binary"
rtl_dir="$project_dir/vendor/rtl_433_ESP/src/rtl_433"
cc -D_POSIX_C_SOURCE=200809L -std=c11 -Wall -Wextra -Werror -I"$project_dir/vendor/rtl_433_ESP/include" -I"$rtl_dir" \
    -c "$rtl_dir/data.c" -o "$test_binary.data.o"
cc -D_POSIX_C_SOURCE=200809L -std=c11 -Wall -Wextra -Werror -I"$project_dir/vendor/rtl_433_ESP/include" -I"$rtl_dir" \
    -c "$rtl_dir/abuf.c" -o "$test_binary.abuf.o"
g++ -std=c++17 -Wall -Wextra -Werror -I"$project_dir/firmware/include" \
    -I"$project_dir/vendor/rtl_433_ESP/include" -I"$rtl_dir" \
    "$project_dir/firmware/tests/decoder_formatter_test.cpp" \
    "$test_binary.data.o" "$test_binary.abuf.o" -o "$test_binary"
"$test_binary"
rm -f "$test_binary.data.o" "$test_binary.abuf.o"
g++ -std=c++17 -Wall -Wextra -Werror \
    -I"$project_dir/firmware/include" \
    "$project_dir/firmware/tests/capture_devices_test.cpp" -o "$test_binary"
"$test_binary"
g++ -std=c++17 -Wall -Wextra -Werror \
    -I"$project_dir/firmware/include" \
    "$project_dir/firmware/tests/display_preferences_test.cpp" -o "$test_binary"
"$test_binary"
g++ -std=c++17 -Wall -Wextra -Werror \
    -I"$project_dir/firmware/include" \
    "$project_dir/firmware/tests/nws_pressure_test.cpp" -o "$test_binary"
"$test_binary"
g++ -std=c++17 -Wall -Wextra -Werror \
    -I"$project_dir/firmware/include" \
    "$project_dir/firmware/tests/startup_health_test.cpp" -o "$test_binary"
"$test_binary"
g++ -std=c++17 -Wall -Wextra -Werror \
    -I"$project_dir/firmware/include" \
    "$project_dir/firmware/tests/weather_store_test.cpp" -o "$test_binary"
"$test_binary"
g++ -std=c++17 -Wall -Wextra -Werror \
    -I"$project_dir/firmware/include" \
    "$project_dir/firmware/tests/history_archive_test.cpp" -o "$test_binary"
"$test_binary"
g++ -std=c++17 -Wall -Wextra -Werror \
    -I"$project_dir/firmware/include" \
    "$project_dir/firmware/tests/sensor_health_test.cpp" -o "$test_binary"
"$test_binary"
g++ -std=c++17 -Wall -Wextra -Werror \
    -I"$project_dir/firmware/include" \
    "$project_dir/firmware/tests/setup_credentials_test.cpp" -o "$test_binary"
"$test_binary"
g++ -std=c++17 -Wall -Wextra -Werror -Wno-misleading-indentation \
    -I"$project_dir/firmware/include" \
    "$project_dir/firmware/tests/reporting_test.cpp" -o "$test_binary"
"$test_binary"
json_headers="$project_dir/vendor/ArduinoJson/src"
g++ -std=c++17 -Wall -Wextra -Werror -Wno-misleading-indentation \
    -I"$project_dir/firmware/include" -I"$json_headers" \
    "$project_dir/firmware/tests/station_discovery_test.cpp" -o "$test_binary"
"$test_binary"
g++ -std=c++17 -Wall -Wextra -Werror \
    -I"$project_dir/firmware/include" -I"$json_headers" \
    "$project_dir/firmware/tests/web_history_test.cpp" -o "$test_binary"
"$test_binary"
g++ -std=c++17 -Wall -Wextra -Werror -Wno-misleading-indentation \
    -I"$project_dir/firmware/include" -I"$json_headers" \
    "$project_dir/firmware/tests/atlas_packet_test.cpp" -o "$test_binary"
"$test_binary"
python3 -m unittest discover -s "$script_dir" -p 'test_firmware_console.py' -v
python3 -m unittest discover -s "$script_dir" -p 'test_connect_capture.py' -v
python3 -m unittest discover -s "$script_dir" -p 'test_connect_page.py' -v
