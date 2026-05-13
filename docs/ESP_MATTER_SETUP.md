# ESP-Matter SDK 编译环境搭建指南

## 依赖清单

| 组件 | 版本 | 说明 |
|------|------|------|
| ESP-IDF | v5.1+ | Espressif 开发框架 |
| esp-matter | v1.2+ | Matter SDK for ESP32 |
| Python | 3.8+ | 构建脚本依赖 |
| CMake | 3.16+ | 构建系统 |
| Ninja | 1.10+ | 构建工具 |
| 开发板 | ESP32-S3-DevKitC-1 | 推荐 8MB Flash |

## 快速搭建

```bash
# 1. 安装 ESP-IDF
git clone -b v5.1.2 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
. ./export.sh

# 2. 安装 esp-matter
cd ..
git clone --recursive https://github.com/espressif/esp-matter.git
cd esp-matter
./install.sh
. ./export.sh

# 3. 验证
idf.py --version
which esp-matter-mfg-tool
```

## 编译入口

```bash
cd projects/matter-panel/esp32-firmware
idf.py set-target esp32s3
idf.py build
```

## 烧录

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

## 2 路裁剪版

```bash
# 修改 main/app_main.cpp 顶部宏
# #define CHANNEL_COUNT 2
idf.py build
```

## 常见问题

| 问题 | 解决 |
|------|------|
| 编译体积超分区 | 精简 partition table，启用 `CONFIG_COMPILER_OPTIMIZATION_SIZE` |
| Matter 证书缺失 | 运行 `esp-matter-mfg-tool` 生成 DAC 证书 |
| BLE 配网失败 | 确保天线连接，检查 2.4GHz Wi-Fi 干扰 |
| HA 未发现设备 | 确认 HA Core ≥ 2024.1，Python Matter Server 运行中 |
