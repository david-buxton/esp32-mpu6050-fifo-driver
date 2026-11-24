#!/usr/bin/env bash
set -e
set -o pipefail

# Run from the project directory, regardless of where invoked.
PROJECT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_DIR"

# Optional first argument: serial port.
PORT="${1:-/dev/ttyUSB0}"

# Activate your ESP-IDF installation if necessary.
if ! command -v idf.py >/dev/null 2>&1; then
    IDF_EXPORT="$HOME/esp/esp-idf/export.sh"

    if [[ ! -f "$IDF_EXPORT" ]]; then
        echo "ESP-IDF export script not found: $IDF_EXPORT" >&2
        exit 1
    fi

    source "$IDF_EXPORT"
fi

# Configure a new project for your ESP32 WROOM.
if [[ ! -f sdkconfig ]]; then
    idf.py set-target esp32
fi

# Build, flash, then display serial output.
idf.py build
idf.py -p "$PORT" flash monitor
