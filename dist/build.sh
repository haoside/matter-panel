#!/bin/bash
set -e

BOARD="${1:-xiao_esp32c6}"
CH_COUNT="${2:-4}"

if [[ "$BOARD" != "xiao_esp32c6" && "$BOARD" != "esp32s3_box3" ]]; then
    echo "Usage: $0 <xiao_esp32c6|esp32s3_box3> [2|4]"
    exit 1
fi

if [[ "$CH_COUNT" != "2" && "$CH_COUNT" != "4" ]]; then
    echo "Usage: $0 <board> [2|4]"
    exit 1
fi

TARGET_CHIP="esp32c6"
if [[ "$BOARD" == "esp32s3_box3" ]]; then
    TARGET_CHIP="esp32s3"
fi

# 检查环境
if [[ -z "$IDF_PATH" || -z "$ESP_MATTER_PATH" ]]; then
    echo "Error: IDF_PATH or ESP_MATTER_PATH not set."
    echo "Please install ESP-IDF v5.1.2 and esp-matter, then run:"
    echo "  . \$IDF_PATH/export.sh"
    echo "  . \$ESP_MATTER_PATH/export.sh"
    echo ""
    echo "Quick install:"
    echo "  git clone -b v5.1.2 --recursive https://github.com/espressif/esp-idf.git ~/esp-idf"
    echo "  ~/esp-idf/install.sh $TARGET_CHIP"
    echo "  git clone --recursive https://github.com/espressif/esp-matter.git ~/esp-matter"
    echo "  ~/esp-matter/install.sh"
    exit 1
fi

FIRMWARE_DIR="$(cd "$(dirname "$0")/.." && pwd)/esp32-firmware"
BUILD_DIR="$FIRMWARE_DIR/build"
DIST_DIR="$(cd "$(dirname "$0")" && pwd)/${BOARD}_${CH_COUNT}ch"

echo "=== Building Matter Panel ==="
echo "Board: $BOARD"
echo "Target: $TARGET_CHIP"
echo "Channels: $CH_COUNT"

# 修改 channel count
sed -i "s/#define CHANNEL_COUNT .*/#define CHANNEL_COUNT $CH_COUNT/" "$FIRMWARE_DIR/main/app_main.cpp"

# 编译
cd "$FIRMWARE_DIR"
rm -rf "$BUILD_DIR"
idf.py set-target "$TARGET_CHIP"
idf.py build

# 收集产物
mkdir -p "$DIST_DIR"
cp "$BUILD_DIR/bootloader/bootloader.bin" "$DIST_DIR/"
cp "$BUILD_DIR/partition_table/partition-table.bin" "$DIST_DIR/"
cp "$BUILD_DIR/matter_panel.bin" "$DIST_DIR/firmware.bin"

# 生成 manifest
cat > "$DIST_DIR/manifest.json" <<EOF
{
  "name": "matter-panel",
  "version": "0.1.0",
  "board": "$BOARD",
  "target_chip": "$TARGET_CHIP",
  "channel_count": $CH_COUNT,
  "idf_version": "v5.1.2",
  "esp_matter_version": "v1.2",
  "compiled_at": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "files": {
    "bootloader": "bootloader.bin",
    "partition_table": "partition-table.bin",
    "firmware": "firmware.bin"
  }
}
EOF

# 校验
cd "$DIST_DIR"
sha256sum *.bin > sha256sum.txt

echo ""
echo "=== Build complete ==="
echo "Output: $DIST_DIR"
echo "Files:"
ls -lh "$DIST_DIR"
