#!/bin/bash
set -e

PORT="${1:-/dev/ttyUSB0}"
CHIP="esp32c6"
BAUD="460800"

echo "=== Matter Panel Flash Tool ==="
echo "Port: $PORT"
echo "Chip: $CHIP"
echo ""

# Verify files exist
for f in bootloader.bin partition-table.bin firmware.bin; do
    if [ ! -f "$f" ]; then
        echo "ERROR: $f not found in current directory"
        echo "Please run this script from the directory containing the .bin files"
        exit 1
    fi
done

echo "Files found:"
ls -lh bootloader.bin partition-table.bin firmware.bin
echo ""

# Erase and flash
echo "Erasing flash..."
esptool.py --chip $CHIP --port $PORT --baud $BAUD erase_flash

echo ""
echo "Flashing bootloader -> 0x0..."
esptool.py --chip $CHIP --port $PORT --baud $BAUD write_flash 0x0 bootloader.bin

echo "Flashing partition-table -> 0x8000..."
esptool.py --chip $CHIP --port $PORT --baud $BAUD write_flash 0x8000 partition-table.bin

echo "Flashing firmware -> 0x10000..."
esptool.py --chip $CHIP --port $PORT --baud $BAUD write_flash 0x10000 firmware.bin

echo ""
echo "=== Flash complete ==="
echo "Reset the board to boot."
echo "Monitor with: idf.py -p $PORT monitor"
