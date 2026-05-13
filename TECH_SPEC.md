# Matter Panel 技术方案定稿 v1.0

> **P0 范围**：4 路 On/Off 灯控面板 + HA 接入 + 双向同步 + 异常恢复  
> **P1 扩展**：调光（Dimmable Light / LevelControl）、更多场景、命名映射优化

---

## 1. 设备模型

### 1.1 硬件抽象

```
┌─────────────────────────────────────────────┐
│  ESP32-S3 (推荐) / ESP32-C3                  │
│  ├─ Matter over Wi-Fi Controller            │
│  ├─ 4× GPIO 输出 → 继电器/可控硅              │
│  ├─ 1× 状态指示灯 (RGB LED / 单色)           │
│  └─ 可选：小屏 SPI/I2C / 蜂鸣器              │
└─────────────────────────────────────────────┘
```

### 1.2 Matter Endpoint 映射（P0：On/Off Light）

| 物理路 | Endpoint ID | Cluster | HA Entity ID | GPIO |
|--------|-------------|---------|--------------|------|
| CH1 | 动态分配 | OnOff | light.{room}_main | GPIO5 |
| CH2 | 动态分配 | OnOff | light.{room}_strip | GPIO18 |
| CH3 | 动态分配 | OnOff | light.{room}_wall | GPIO19 |
| CH4 | 动态分配 | OnOff | light.{room}_pendant | GPIO21 |

- **P0 Endpoint 类型**：`MAGN_ON_OFF_LIGHT = 0x0100`（仅 OnOff Cluster）
- **P1 预留**：升级为 `Dimmable Light (0x0101)`，增加 LevelControl Cluster
- 固件内 `CHANNEL_COUNT` 宏切 2/4 路，编译期决定，**不接受单独维护第二套逻辑**

### 1.3 SKU 规格

| SKU | channelCount | 布局 | 验证要求 |
|-----|--------------|------|----------|
| 主版 | 4 | 2×2 四宫格 | P0 8 条全跑 |
| 裁剪版 | 2 | 上下双卡 | 至少 P0-01/03/04/08 |

### 1.4 设备基础信息

```json
{
  "deviceId": "matter_panel_{{MAC_SUFFIX}}",
  "deviceName": "灯控面板",
  "roomName": "客厅",
  "panelType": "light_control_panel",
  "channelCount": 4,
  "connectType": "matter_wifi",
  "firmwareVersion": "0.1.0"
}
```

---

## 2. 配网与 HA 接入链路

### 2.1 配网流程

```
[上电] → [BLE 广播 Commissioning] → [HA Matter 集成发现]
         ↓
    [输入配对码 / QR] → [CASE 会话建立]
         ↓
    [Fabric 加入 HA] → [Operational Discovery]
         ↓
    [HA 自动生成实体] → [完成]
```

- 配对窗口：15 分钟（Matter 默认），超时后需重新上电或手动触发
- 配对码：设备出厂贴 QR，也可通过按键组合进入配对模式重新广播
- 固件端：`esp_matter::start()` 启动后自动进入 Commissioning 状态
- HA 端：内置 `matter:` 集成，通过 Python Matter Server 管理

### 2.2 HA 实体生成

接入成功后，HA 自动生成：
- `light.matter_panel_ch1` ~ `light.matter_panel_ch4`
- 可选：二进制传感器 `binary_sensor.matter_panel_connected`

### 2.3 面板配网状态机

```
unpaired → pairing → paired → sync_done → online
              ↓         ↓           ↓
            failed   failed     failed
              ↓         ↓           ↓
           [重试]    [重试]      [重试]
```

| 状态 | 面板显示 | 含义 |
|------|---------|------|
| unpaired | 未配网 | 等待 Commissioning |
| pairing | 配对中 | CASE 会话建立中 |
| paired | 已接入 HA | Fabric 已加入 |
| sync_done | 同步中 | 状态首次同步 |
| online | 在线 | 正常可用 |
| offline | 离线 | 网络/HA 不可达 |
| error | 异常 | 配对码错误/超时 |

---

## 3. 状态同步机制

### 3.1 三条控制链路

```
① 本地按键 ──► ESP32 GPIO ──► 继电器 ──► 灯
                │
                ▼ (回写 Matter attribute)
              HA 实体状态同步
                ▲
② HA Web/App ─┘

③ 面板小屏/WebSocket ──► 读取 HA 状态 ──► 显示更新
```

### 3.2 状态源优先级（冲突时以设备最终回传为准）

| 优先级 | 来源 | 说明 |
|--------|------|------|
| 1 | 设备实际状态回传 | Matter attribute 上报 / HA `state_changed` 事件 |
| 2 | HA 下发指令 | InvokeCommand，设备执行后回传确认 |
| 3 | 本地按键触发 | 先本地 pending，等待设备回传确认后落最终态 |

**UI 展示规则**：
- 本地按键 / HA 操作后 → 先显示 `pending`（执行中…）
- 收到 `state_changed` 确认 → 更新为 `on/off`
- 3s 超时未收到确认 → 切 `no_response`（无响应）

### 3.3 双向同步策略

**本地 → HA**
- 按键触发 → ESP32 `app_attribute_update_cb` (PRE_UPDATE)
- 更新 GPIO 输出 → 同时通过 Matter 协议上报 Cluster attribute
- HA Matter 集成订阅 attribute 变化 → 更新 entity state

**HA → 本地**
- HA 操作 entity → 通过 Matter 发送 InvokeCommand
- ESP32 `app_attribute_update_cb` (PRE_UPDATE) 接收 → 更新 GPIO
- 无需单独回写，Matter 协议内建确认

**面板显示**
- 前端 WebSocket 订阅 HA `state_changed` 事件
- 收到事件 → 更新对应回路卡片状态
- 指令发送后：先本地显示 `执行中…` → 收到 state_changed 后更新为真实状态

### 3.4 状态字段对齐

```ts
// 单路通道状态（固件/HA/前端统一，P0 无 brightness）
interface Channel {
  channelId: string;      // "ch1" | "ch2" | "ch3" | "ch4"
  channelName: string;    // "主灯" | "灯带" | "壁灯" | "吊灯"
  powerState: "on" | "off" | "pending" | "no_response";
  available: boolean;     // 是否可用
  // P1: brightness?: number; // 0-255
}

// 面板全局状态
interface PanelState {
  panelStatus: "unpaired" | "pairing" | "online" | "offline" | "syncing" | "error";
  errorCode?: string;     // NETWORK_ERROR | HA_UNAVAILABLE | PAIRING_TIMEOUT | ...
  lastHeartbeat: number;  // 最后同步时间戳
}
```

### 3.5 异常恢复策略

| 场景 | 本地可控 | 重连策略 | 状态恢复 | 超时/重试 |
|------|----------|----------|----------|-----------|
| Wi-Fi 断开 | ✅ 本地按键仍可控灯 | 自动重连，间隔 3s → 10s → 30s 指数退避 | 网络恢复后自动同步状态 | 3s 判定离线，无重试上限 |
| HA 不可达 | ✅ 本地按键仍可控灯 | Matter CASE Sigma1 自动重建会话 | HA 重启后自动发现已有设备 | 5s 无心跳判定离线 |
| 指令超时 | — | — | — | 3s 切 `no_response`，不重试 |
| 设备重启 | — | Matter NVM 恢复 Fabric 信息，自动重连 HA | GPIO 恢复上次状态 | — |
| 断电恢复 | — | 自动上线 | 上次状态策略：**恢复上次状态**（非安全态） | — |

---

## 4. 风险清单 + 验收阈值

| # | 风险 | 影响 | 应对 | 验收阈值 |
|---|------|------|------|----------|
| R1 | Matter SDK 编译体积大，ESP32 分区紧张 | 固件刷写失败 | 使用 ESP32-S3（8MB Flash），精简 partition table | 固件 ≤ 4MB |
| R2 | 连续快速按键导致 Matter 指令堆积 | 状态错乱 | 前端/固件做 200ms 防抖，pending 状态锁 | 连续 10 次操作不丢指令，最终一致 |
| R3 | HA Matter 集成版本差异 | 实体不生成或控制失败 | 锁定 HA Core ≥ 2024.1，Matter Server ≥ 5.0 | — |
| R4 | Wi-Fi 弱信号下 CASE 会话频繁重建 | 延迟高/离线误报 | 增加离线判定阈值（5s 无心跳），本地控制保持可用 | — |
| R5 | 多路同时操作，某一路无响应 | 用户感知"部分失败" | 每路独立超时检测（3s），独立反馈 `无响应` | — |
| R6 | 配对码暴露导致未授权接入 | 安全隐患 | 配对窗口限时 15min，支持 Factory Reset 清除 Fabric | — |
| R7 | 小屏/Web 前端与 HA WebSocket 跨域 | 连接失败 | 使用 HA 自带 `api/websocket`（同域）或配置 CORS | — |
| R8 | 2 路与 4 路固件不兼容 | 维护两份代码 | 固件用 `CHANNEL_COUNT` 宏统一，一套代码编译配置切换 | 2 路裁剪版至少通过 P0-01/03/04/08 |

**核心验收指标**：
- 配网成功率 ≥ 90%
- HA 接入成功率 ≥ 95%
- 控制成功率 ≥ 99%
- P50 响应 ≤ 500ms
- P90 响应 ≤ 1.5s

---

## 5. 指标-日志映射表

| 验收指标 | 计算方式 | 对应日志事件 | 字段 |
|----------|----------|--------------|------|
| 配网成功率 | pairing_success / pairing_start | `pairing_start`, `pairing_success`, `pairing_fail` | `result`, `errorCode`, `latencyMs` |
| HA 接入成功率 | 实体创建成功 / 配网成功 | `pairing_success` → `state_sync` | `haEntityId`, `channelId`, `result` |
| 控制成功率 | control_success / control_sent | `control_sent`, `control_success`, `control_fail` | `channelId`, `action`, `result`, `errorCode` |
| 响应时延 | control_success.ts - control_sent.ts | `control_sent`, `control_success` | `latencyMs` |
| 恢复时间 | device_recovered.ts - device_offline.ts | `device_offline`, `device_recovered` | `latencyMs` |
| 启动耗时 | boot completed timestamp | `boot` | `bootTimeMs` |

---

## 6. 兼容性测试矩阵

| 测试项 | channelCount | HA 版本 | 网络环境 | 控制来源 |
|--------|--------------|---------|----------|----------|
| P0-01 配网 | 4 / 2 | ≥2024.1 | 正常 | — |
| P0-02 实体创建 | 4 / 2 | ≥2024.1 | 正常 | — |
| P0-03 本地→HA | 4 / 2 | ≥2024.1 | 正常 | 本地按键 |
| P0-04 HA→面板 | 4 / 2 | ≥2024.1 | 正常 | HA Web |
| P0-05 快速操作 | 4 | ≥2024.1 | 正常 | 本地按键 |
| P0-06 断网识别 | 4 | ≥2024.1 | 断网 | 本地按键 |
| P0-07 网络恢复 | 4 | ≥2024.1 | 断网→恢复 | — |
| P0-08 重启恢复 | 4 / 2 | ≥2024.1 | 正常 | — |
| 弱网延迟 | 4 | ≥2024.1 | 弱网（-70dBm） | 本地+HA |
| 2 路裁剪 | 2 | ≥2024.1 | 正常 | 本地按键 |

---

## 7. 下一步

1. 固件编译验证（等 `esp-matter` SDK 环境就绪）
2. 前端字段接入文案运营最终枚举
3. HA 实体命名按 `light.{room}_{name}` 调整
4. 联调：按 P0 8 条跑通，4 路全量 + 2 路裁剪验证
