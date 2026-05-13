#!/bin/bash
set -e

BOARD="${1:-xiao_esp32c6}"
CH_COUNT="${2:-4}"
PORT="${3:-/dev/ttyACM0}"
BAUD="${4:-921600}"

DIST_DIR="$(cd "$(dirname "$0")" && pwd)/${BOARD}_${CH_COUNT}ch"

if [[ ! -d "$DIST_DIR" ]]; then
    echo "Error: Build output not found for $BOARD $CH_COUNT channel"
    echo "Please run build.sh first:"
    echo "  ./build.sh $BOARD $CH_COUNT"
    exit 1
fi

echo "=== Flashing Matter Panel ==="
echo "Board: $BOARD"
echo "Channels: $CH_COUNT"
echo "Port: $PORT"
echo "Baud: $BAUD"
echo ""

esptool.py \
    --chip auto \
    --port "$PORT" \
    --baud "$BAUD" \
    --before default_reset \
    --after hard_reset \
    write_flash \
    --flash_mode dio \
    --flash_freq 80m \
    --flash_size 4MB \
    0x0000 "$DIST_DIR/bootloader.bin" \
    0x8000 "$DIST_DIR/partition-table.bin" \
    0x10000 "$DIST_DIR/firmware.bin"

echo ""
echo "=== Flash complete ==="
echo "Reset the board and check serial output with:"
echo "  idf.py -p $PORT monitor"
