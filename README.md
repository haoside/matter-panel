# Matter Panel 项目

ESP32 Matter 灯控面板，Home Assistant 接入。

## 冻结规格 v1

| 项 | 值 |
|---|---|
| 主 SKU | 4 路面板 |
| 裁剪 SKU | 2 路，`CHANNEL_COUNT=2` |
| 协议 | Matter over Wi-Fi |
| 接入 | 已有 Home Assistant 实例 |
| 形态 | 实体按键 + 状态指示 LED / 小屏 |
| P0 范围 | 4 路 On/Off 灯控 + HA 接入 + 双向同步 + 异常恢复 |
| P1 扩展 | 调光（Dimmable Light）、更多场景、命名映射优化 |

## 目录

- `esp32-firmware/` — ESP32 Matter SDK 固件（On/Off Light）
- `home-assistant/` — HA 配置、自动化、设备发现脚本
- `frontend/` — 面板前端（实体按键逻辑 + 小屏 + 状态 LED 映射）
- `TECH_SPEC.md` — 技术方案定稿（含状态源优先级、异常恢复、验收阈值、测试矩阵）

## 状态枚举（最终口径，代码只存英文，UI 层映射中文）

### panelStatus
| 枚举 | 文案 | LED 效果 |
|------|------|----------|
| `unpaired` | 未配网 | 蓝色慢闪 |
| `pairing` | 配对中 | 蓝色快闪 |
| `online` | 在线 | 蓝色常亮 |
| `offline` | 离线 | 灰态/熄灭 |
| `syncing` | 同步中 | 蓝色常亮 |
| `error` | 异常 | 橙色慢闪 |

### powerState（单路）
| 枚举 | 文案 | 指示灯 |
|------|------|--------|
| `off` | 关闭 | 常灭 |
| `on` | 开启 | 常亮（暖白） |
| `pending` | 执行中 | 快闪 2 次 |
| `no_response` | 无响应 | 红色慢闪 |

### pairingStep
| 枚举 | 文案 |
|------|------|
| `idle` | 等待配网 |
| `discovering` | 搜索中 |
| `code_required` | 输入配对码 |
| `pairing` | 配对中，请勿断电 |
| `paired` | 已接入 Home Assistant |
| `sync_done` | 已连接 |
| `failed` | 配对失败 |

### errorCode
`NETWORK_ERROR` / `HA_UNAVAILABLE` / `DEVICE_OFFLINE` / `PAIRING_CODE_INVALID` / `PAIRING_TIMEOUT` / `COMMAND_TIMEOUT` / `STATE_SYNC_FAILED`

## 设计规范 v1

- 背景 `#111317`，卡片 `#1A1D22`，圆角 `12px`
- 语义色：在线 `#3B82F6` / 开启 `#F4C542` / 成功 `#22C55E` / 警告 `#F59E0B` / 离线 `#667085` / 错误 `#EF4444`
- 4 路：`2×2` 四宫格，单卡宽高比 `1:0.9`
- 2 路：上下双卡，单卡宽高比 `1:0.42`
- 按下态：卡片亮度 `+8%`
- 无响应：红色描边脉冲动画
- 离线：整卡灰化降饱和
- **P0 无调光控件**，单路卡片只保留：名称 / 开关态 / 执行反馈

## 状态同步策略

- **状态源优先级**：设备最终回传 > HA 下发 > 本地按键
- UI 展示：`pending` → 收到 `state_changed` → 落最终态
- 3s 超时未确认 → 切 `no_response`

## 异常恢复策略

| 场景 | 本地可控 | 重连策略 |
|------|----------|----------|
| Wi-Fi 断开 | ✅ | 3s→10s→30s 指数退避 |
| HA 不可达 | ✅ | CASE Sigma1 自动重建 |
| 设备重启 | — | NVM 恢复 Fabric，自动重连 |
| 断电恢复 | — | 恢复上次状态（非安全态） |

## P0 联调 8 条

| 编号 | 测试项 | 关键指标 |
|------|--------|----------|
| P0-01 | 首次配网成功 | 配网耗时、errorCode |
| P0-02 | HA 实体创建正确 | 实体数、映射正确性 |
| P0-03 | 本地按键 → HA 同步 | latencyMs、success/fail |
| P0-04 | HA 控制 → 面板同步 | latencyMs、success/fail |
| P0-05 | 连续快速操作稳定性 | 丢指令次数、最终一致性 |
| P0-06 | 断网状态识别 | 状态切换耗时、errorCode |
| P0-07 | 网络恢复自动回连 | 恢复耗时（≤60s） |
| P0-08 | 设备重启恢复 | 启动耗时、实体不重复 |

**验收阈值**：
- 配网成功率 ≥ 90%
- HA 接入成功率 ≥ 95%
- 控制成功率 ≥ 99%
- P50 响应 ≤ 500ms，P90 ≤ 1.5s

## 架构

```
┌─────────────────┐     Matter over Wi-Fi      ┌─────────────────┐
│  ESP32 面板      │ ◄────────────────────────► │  Home Assistant │
│  实体按键+状态灯  │                            │  (Matter Hub)   │
│  4 路继电器输出   │                            └────────┬────────┘
└─────────────────┘                                     │
                                                        │ WebSocket
                                                        ▼
                                                ┌───────────────┐
                                                │  前端状态面板  │
                                                │  (小屏/Web)   │
                                                └───────────────┘
```

## 日志字段（统一）

```
deviceId, firmwareVersion, channelId, action, result, errorCode, latencyMs, haEntityId
```

## 状态

- [x] 设备模型：4 路 On/Off Light endpoint + GPIO
- [x] 配网链路：BLE Commissioning → CASE → Fabric → HA
- [x] 状态同步：本地按键/HA Web/面板显示 三条链路 + 状态源优先级
- [x] 异常恢复：断网本地可控、指数退避重连、断电恢复上次状态
- [x] 前端：设计规范 v1 CSS 变量 + 状态 LED 映射 + 2/4 路布局
- [x] 固件：状态 LED 任务 + 统一日志字段
- [x] 验收阈值：配网≥90%、接入≥95%、控制≥99%、P50≤500ms、P90≤1.5s
- [x] 测试矩阵：兼容性矩阵 + 指标-日志映射表
- [ ] 固件编译验证（等 esp-matter SDK 环境）
- [ ] 联调 P0 8 条
