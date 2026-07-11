#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ACTION="${1:-all}"
PORT="${ESPPORT:-/dev/ttyACM0}"
IDF_EXPORT="${IDF_EXPORT:-$HOME/esp/esp-idf/export.sh}"
LOG_DIR="${XADREZ_LOG_DIR:-$PROJECT_ROOT/logs}"
SECRETS_DIR="$PROJECT_ROOT/secrets"
STAMP="$(date +%Y%m%d_%H%M%S)"
LOG_FILE="$LOG_DIR/xadrez_${ACTION}_${STAMP}.log"
LATEST_LOG="$LOG_DIR/latest_chess.log"
CLEANUP_CSV_FILE=""
CLEANUP_PLAIN_FILE=""
CLEANUP_BIN_FILE=""

mkdir -p "$LOG_DIR"
ln -sfn "$LOG_FILE" "$LATEST_LOG"

log_step()
{
    printf '\n[%s] %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*" | tee -a "$LOG_FILE"
}

run_logged()
{
    log_step "RUN: $*"
    "$@" 2>&1 | tee -a "$LOG_FILE"
}

check_port()
{
    if [ ! -e "$PORT" ]; then
        log_step "ERROR: serial port $PORT was not found"
        find /dev -maxdepth 1 \( -name 'ttyACM*' -o -name 'ttyUSB*' \) -ls 2>/dev/null | tee -a "$LOG_FILE" || true
        log_step "Use ESPPORT=/dev/ttyUSB0 run chess if needed"
        exit 1
    fi
}

csv_escape()
{
    local value="$1"
    value="${value//\"/\"\"}"
    printf '"%s"' "$value"
}

parse_partition_size()
{
    local raw="$1"
    local unit=""
    local number=""

    raw="${raw//[[:space:]]/}"

    case "$raw" in
        *K|*k)
            unit="K"
            number="${raw%?}"
            ;;
        *M|*m)
            unit="M"
            number="${raw%?}"
            ;;
        *)
            unit=""
            number="$raw"
            ;;
    esac

    case "$unit" in
        K)
            echo $((number * 1024))
            ;;
        M)
            echo $((number * 1024 * 1024))
            ;;
        *)
            echo "$number"
            ;;
    esac
}

ensure_credentials_for_flash()
{
    local answer

    if [ ! -f "$SECRETS_DIR/wifi_enterprise.env.enc" ]; then
        log_step "No encrypted local WPA2 Enterprise credential archive found."
        read -r -p "Provision institutional Wi-Fi credentials now? [y/N]: " answer

        case "$answer" in
            y|Y|yes|YES)
                provision_wifi_enterprise
                ;;
            *)
                log_step "Continuing without provisioned WPA2 Enterprise credentials. SoftAP will still be available."
                ;;
        esac
    fi
}

provision_wifi_enterprise()
{
    local ssid
    local identity
    local username
    local password
    local csv_file
    local plain_file
    local enc_file
    local bin_file
    local part_csv
    local nvs_line
    local nvs_offset
    local nvs_size
    local nvs_gen

    check_port

    if ! command -v openssl >/dev/null 2>&1; then
        log_step "ERROR: openssl was not found. Install it before provisioning encrypted local credential archives."
        exit 1
    fi

    mkdir -p "$SECRETS_DIR"
    chmod 700 "$SECRETS_DIR"

    csv_file="$SECRETS_DIR/wifi_enterprise_${STAMP}.csv"
    plain_file="$SECRETS_DIR/wifi_enterprise_${STAMP}.env"
    enc_file="$SECRETS_DIR/wifi_enterprise.env.enc"
    bin_file="$SECRETS_DIR/wifi_enterprise_${STAMP}.bin"
    part_csv="$LOG_DIR/partition_table_${STAMP}.csv"
    nvs_gen="$IDF_PATH/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py"

    CLEANUP_CSV_FILE="$csv_file"
    CLEANUP_PLAIN_FILE="$plain_file"
    CLEANUP_BIN_FILE="$bin_file"

    cleanup_plain()
    {
        rm -f "${CLEANUP_CSV_FILE:-}" "${CLEANUP_PLAIN_FILE:-}" "${CLEANUP_BIN_FILE:-}"
    }

    trap cleanup_plain EXIT

    log_step "Provisioning WPA2 Enterprise credentials into ESP32-S3 NVS"
    read -r -p "Institutional Wi-Fi SSID: " ssid
    read -r -p "EAP identity: " identity
    read -r -p "EAP username: " username
    read -r -s -p "EAP password: " password
    printf '\n'

    if [ -z "$ssid" ] || [ -z "$identity" ] || [ -z "$username" ] || [ -z "$password" ]; then
        log_step "ERROR: all WPA2 Enterprise fields are required"
        exit 1
    fi

    {
        printf 'SSID=%s\n' "$ssid"
        printf 'IDENTITY=%s\n' "$identity"
        printf 'USERNAME=%s\n' "$username"
        printf 'PASSWORD=%s\n' "$password"
    } > "$plain_file"
    chmod 600 "$plain_file"

    log_step "Creating encrypted local credential archive: $enc_file"
    openssl enc -aes-256-cbc -salt -pbkdf2 -iter 600000 -in "$plain_file" -out "$enc_file" 2>&1 | tee -a "$LOG_FILE"
    chmod 600 "$enc_file"

    {
        printf 'key,type,encoding,value\n'
        printf 'wifi_enter,namespace,,\n'
        printf 'ssid,data,string,%s\n' "$(csv_escape "$ssid")"
        printf 'identity,data,string,%s\n' "$(csv_escape "$identity")"
        printf 'username,data,string,%s\n' "$(csv_escape "$username")"
        printf 'password,data,string,%s\n' "$(csv_escape "$password")"
    } > "$csv_file"
    chmod 600 "$csv_file"

    run_logged idf.py set-target esp32s3
    run_logged idf.py partition-table

    python3 "$IDF_PATH/components/partition_table/gen_esp32part.py" \
        "$PROJECT_ROOT/build/partition_table/partition-table.bin" > "$part_csv"

    nvs_line="$(awk -F, '$1 ~ /^[[:space:]]*nvs[[:space:]]*$/ { print; exit }' "$part_csv")"

    if [ -z "$nvs_line" ]; then
        log_step "ERROR: could not find the nvs partition in the partition table"
        exit 1
    fi

    nvs_offset="$(printf '%s\n' "$nvs_line" | awk -F, '{ gsub(/[[:space:]]/, "", $4); print $4 }')"
    nvs_size="$(printf '%s\n' "$nvs_line" | awk -F, '{ gsub(/[[:space:]]/, "", $5); print $5 }')"
    nvs_size="$(parse_partition_size "$nvs_size")"

    if [ ! -f "$nvs_gen" ]; then
        log_step "ERROR: NVS partition generator not found: $nvs_gen"
        exit 1
    fi

    log_step "Generating temporary NVS image for partition nvs at offset $nvs_offset size $nvs_size bytes"
    python3 "$nvs_gen" generate "$csv_file" "$bin_file" "$nvs_size" 2>&1 | tee -a "$LOG_FILE"

    if [ ! -s "$bin_file" ]; then
        log_step "ERROR: NVS image was not generated: $bin_file"
        exit 1
    fi

    log_step "Flashing credential NVS image to ESP32-S3"
    run_logged esptool.py --chip esp32s3 -p "$PORT" write_flash "$nvs_offset" "$bin_file"

    log_step "Provisioning finished. Plaintext temporary files were deleted. Encrypted archive kept at $enc_file"
}

if [ ! -f "$IDF_EXPORT" ]; then
    echo "Error: ESP-IDF export script not found: $IDF_EXPORT" | tee -a "$LOG_FILE"
    echo "Install ESP-IDF or run with IDF_EXPORT=/path/to/export.sh run chess" | tee -a "$LOG_FILE"
    exit 1
fi

. "$IDF_EXPORT" >/dev/null
cd "$PROJECT_ROOT"

if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    git switch Breno >/dev/null 2>&1 || git checkout Breno >/dev/null 2>&1 || true
fi

log_step "Xadrez command started"
{
    echo "Project: $PROJECT_ROOT"
    echo "Action:  $ACTION"
    echo "Port:    $PORT"
    echo "Log:     $LOG_FILE"
    echo "Branch:  $(git branch --show-current 2>/dev/null || echo unknown)"
    echo "IDF:     ${IDF_PATH:-unknown}"
} | tee -a "$LOG_FILE"

case "$ACTION" in
    build)
        run_logged idf.py set-target esp32s3
        run_logged idf.py fullclean
        run_logged idf.py build
        log_step "Build finished. Log saved at $LOG_FILE"
        ;;
    quick-build)
        run_logged idf.py set-target esp32s3
        run_logged idf.py build
        log_step "Quick build finished. Log saved at $LOG_FILE"
        ;;
    clean-build)
        run_logged idf.py set-target esp32s3
        run_logged idf.py fullclean
        run_logged idf.py build
        log_step "Clean build finished. Log saved at $LOG_FILE"
        ;;
    provision)
        provision_wifi_enterprise
        ;;
    flash|all)
        ensure_credentials_for_flash
        check_port
        run_logged idf.py set-target esp32s3
        run_logged idf.py fullclean
        run_logged idf.py build
        run_logged idf.py -p "$PORT" flash
        log_step "Opening monitor. Press Ctrl+] to exit. Full log: $LOG_FILE"
        if command -v script >/dev/null 2>&1; then
            script -q -f -a "$LOG_FILE" -c "idf.py -p '$PORT' monitor"
        else
            idf.py -p "$PORT" monitor 2>&1 | tee -a "$LOG_FILE"
        fi
        ;;
    monitor)
        check_port
        log_step "Opening monitor only. Press Ctrl+] to exit. Full log: $LOG_FILE"
        if command -v script >/dev/null 2>&1; then
            script -q -f -a "$LOG_FILE" -c "idf.py -p '$PORT' monitor"
        else
            idf.py -p "$PORT" monitor 2>&1 | tee -a "$LOG_FILE"
        fi
        ;;
    log)
        echo "$LATEST_LOG"
        ;;
    *)
        echo "Usage: run chess [build|quick-build|clean-build|provision|flash|monitor|log]"
        exit 2
        ;;
esac
