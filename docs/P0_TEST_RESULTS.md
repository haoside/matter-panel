# P0 联调结果表

> 跑完直接填，汇总行自动计算
> **Mock ≠ Real，分开记录**

## 4 路主版

| 用例 | channelCount | envType | evidenceLevel | result | latencyMs | errorCode | boardType | transport | firmwareVersion | haVersion | 备注 |
|---|---:|---|---|---|---|---:|---|---|---|---|---|
| P0-01 首次配网成功 | 4 | real | hardware_real | | | | xiao_esp32c6 | wifi | | | 需物理设备 + Matter SDK |
| P0-02 HA 实体创建正确 | 4 | real | hardware_real | | | | xiao_esp32c6 | wifi | | | 需物理设备 + Matter SDK |
| P0-03 本地按键 -> HA 同步 | 4 | mock | integration_mock | pass | 403 | | — | — | | | mock 验证通过 |
| P0-04 HA 控制 -> 面板同步 | 4 | mock | integration_mock | pass | 403 | | — | — | | | mock 验证通过 |
| P0-05 连续快速操作稳定性 | 4 | mock | integration_mock | pass | 603 | | — | — | | | 10次操作0失败 |
| P0-06 断网状态识别 | 4 | mock | ui_flow | pass | 305 | | — | — | | | WS 断开模拟，仅状态流预演 |
| P0-07 网络恢复自动回连 | 4 | mock | ui_flow | pass | 1308 | | — | — | | | 重连+状态恢复预演，非真机恢复 |
| P0-08 设备重启恢复 | 4 | real | hardware_real | | | | xiao_esp32c6 | wifi | | | 需物理设备 |

## 2 路裁剪版

| 用例 | channelCount | envType | evidenceLevel | result | latencyMs | errorCode | boardType | transport | firmwareVersion | haVersion | 备注 |
|---|---:|---|---|---|---|---:|---|---|---|---|---|
| P0-01 首次配网成功 | 2 | real | hardware_real | | | | xiao_esp32c6 | wifi | | | 需物理设备 + Matter SDK |
| P0-03 本地按键 -> HA 同步 | 2 | mock | integration_mock | | | | — | — | | | 待裁剪版验证 |
| P0-04 HA 控制 -> 面板同步 | 2 | mock | integration_mock | | | | — | — | | | 待裁剪版验证 |
| P0-08 设备重启恢复 | 2 | real | hardware_real | | | | xiao_esp32c6 | wifi | | | 需物理设备 |

## Mock 正式通过

| 指标 | 结果 |
|---|---|
| Mock 控制成功率 | 100% |
| Mock P50 响应时延 | 403ms |
| Mock P90 响应时延 | 603ms |
| Mock 正式通过项 | P0-03 / P0-04 / P0-05 |
| Mock 阻塞数 | 0 |

## Mock Drill（状态流预演，不计入验收）

| 用例 | 结果 | 说明 |
|---|---|---|
| P0-06 断网状态识别 | pass | WS 断开模拟，仅 UI 状态流验证 |
| P0-07 网络恢复自动回连 | pass | 重连逻辑预演，非真实网络恢复 |

## 真机待验证项

| 用例 | channelCount | 阻塞原因 |
|---|---|---|
| P0-01 首次配网成功 | 4 / 2 | 无 esp-matter SDK / ESP32 开发板 |
| P0-02 HA 实体创建正确 | 4 / 2 | 无 esp-matter SDK / ESP32 开发板 |
| P0-06 断网状态识别 | 4 / 2 | 需真实网络断开 + 状态切换验证 |
| P0-07 网络恢复自动回连 | 4 / 2 | 需真实断网恢复 + 重连耗时测量 |
| P0-08 设备重启恢复 | 4 / 2 | 无 esp-matter SDK / ESP32 开发板 |

## 交付状态

| 阶段 | 状态 | 产物 |
|---|---|---|
| source_ready | ✅ 已交付 | 源码工程 + 编译脚本 + CI 配置 + 源码包 tar.gz |
| binary_ready | ⏳ GitHub Actions 构建中 | https://github.com/haoside/matter-panel/actions/runs/25833519288 |
| flashed_verified | ⏳ 未开始 | 真机刷写 + P0-01/02/06/07/08 验证 |

## 阻塞清单

| 阻塞项 | 影响用例 | 当前规避方案 | 需要谁配合 |
|---|---|---|---|
| esp-matter 编译环境 / XIAO ESP32-C6 未就绪 | P0-01 / P0-02 / P0-06 / P0-07 / P0-08 | mock 先验 P0-03/04/05 状态同步链路 | hao side 提供 GitHub 仓库 / 本地编译环境 / 开发板 |

## 说明

- **Mock integration_mock** = 前端/状态流/HA 指令链路可用，不计入正式验收
- **Mock ui_flow** = 仅 UI 状态切换逻辑验证，非真实网络/恢复行为
- **Real hardware_real** = 才能计入正式 P0 验收和真实性能数据

## 下一步

1. **产出 binary**：源码包推 GitHub Actions 或本地 ESP-IDF + esp-matter 编译
2. **出包顺序**：XIAO ESP32-C6 4 路 → XIAO ESP32-C6 2 路 → ESP32-S3-BOX-3 demo
3. **真机验证**：刷写后跑 P0-01/02/08，补齐断网/恢复真实数据
4. **裁剪验证**：`CHANNEL_COUNT=2` 编译 + 布局一致性检查
