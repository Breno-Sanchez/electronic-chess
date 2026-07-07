#!/usr/bin/env bash
set -eu

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ACTION="${1:-all}"
PORT="${ESPPORT:-/dev/ttyACM0}"
IDF_EXPORT="${IDF_EXPORT:-$HOME/esp/esp-idf/export.sh}"

if [ ! -f "$IDF_EXPORT" ]; then
    echo "Error: ESP-IDF export script not found: $IDF_EXPORT"
    echo "Install ESP-IDF or run with IDF_EXPORT=/path/to/export.sh run chess"
    exit 1
fi

. "$IDF_EXPORT" >/dev/null

cd "$PROJECT_ROOT" || exit 1

if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    git switch Breno >/dev/null 2>&1 || git checkout Breno >/dev/null 2>&1 || true
fi

check_port()
{
    if [ ! -e "$PORT" ]; then
        echo "Error: serial port $PORT was not found"
        echo "Available serial ports:"
        find /dev -maxdepth 1 \( -name "ttyACM*" -o -name "ttyUSB*" \) -ls 2>/dev/null || true
        echo "Use ESPPORT=/dev/ttyUSB0 run chess if needed"
        exit 1
    fi
}

case "$ACTION" in
    build)
        idf.py set-target esp32s3
        idf.py build
        ;;
    clean-build)
        idf.py set-target esp32s3
        idf.py fullclean
        idf.py build
        ;;
    flash|all)
        check_port
        idf.py set-target esp32s3
        idf.py fullclean
        idf.py build
        idf.py -p "$PORT" flash monitor
        ;;
    monitor)
        check_port
        idf.py -p "$PORT" monitor
        ;;
    *)
        echo "Usage: run chess [build|clean-build|flash|monitor]"
        exit 2
        ;;
esac
