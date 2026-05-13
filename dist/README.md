# Matter Panel 固件交付包

## 目录

```
dist/
├── build.sh              # 本地编译脚本
├── flash.sh              # 烧录脚本
├── xiao_esp32c6_4ch/     # 4 路主版产物（编译后生成）
├── xiao_esp32c6_2ch/     # 2 路裁剪版产物（编译后生成）
└── esp32s3_box3_4ch/     # S3-BOX-3 Demo 产物（编译后生成）
```

## 快速编译

### 方式 1：本地编译（需 ESP-IDF + esp-matter 环境）

```bash
# 安装依赖（首次）
git clone -b v5.1.2 --recursive https://github.com/espressif/esp-idf.git ~/esp-idf
~/esp-idf/install.sh esp32c6
git clone --recursive https://github.com/espressif/esp-matter.git ~/esp-matter
~/esp-matter/install.sh

# 编译
cd dist
./build.sh xiao_esp32c6 4   # 4 路主版
./build.sh xiao_esp32c6 2   # 2 路裁剪版
./build.sh esp32s3_box3 4   # S3-BOX-3 Demo
```

### 方式 2：GitHub Actions CI（推荐，无需本地环境）

1. 将代码推送到 GitHub 仓库
2. 进入 Actions → "Build Matter Panel Firmware"
3. 选择 board 和 channel_count，点击 Run workflow
4. 编译完成后自动下载产物 zip

## 烧录

```bash
cd dist

# XIAO ESP32-C6（串口通常为 /dev/ttyACM0）
./flash.sh xiao_esp32c6 4 /dev/ttyACM0

# ESP32-S3-BOX-3
./flash.sh esp32s3_box3 4 /dev/ttyUSB0
```

## 产物文件说明

每个构建产物目录包含：

| 文件 | 说明 |
|------|------|
| `bootloader.bin` | 二级引导程序 |
| `partition-table.bin` | 分区表 |
| `firmware.bin` | 主固件 |
| `manifest.json` | 版本、板卡、编译信息 |
| `sha256sum.txt` | 文件校验 |

## 验证

烧录后连接串口查看日志：

```bash
idf.py -p /dev/ttyACM0 monitor
```

预期输出：
```
I (0) matter_panel: Matter panel started (4 channels), waiting for commissioning...
```

## 板卡规格

| 板卡 | 芯片 | Flash | 用途 |
|------|------|-------|------|
| XIAO ESP32-C6 | ESP32-C6 (RISC-V) | 4MB | P0 主交付 |
| ESP32-S3-BOX-3 | ESP32-S3 (Xtensa) | 8MB | Demo / UI 验证 |
