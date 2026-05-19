# Matter Panel — HA 面板字段映射表 v1

> 冻结口径：P0 MVP On/Off Light，4路主版 / 2路裁剪

---

## 面板级字段 (Panel)

| 字段名 | 类型 | 来源 | 中文展示 | 图标 | 说明 |
|--------|------|------|---------|------|------|
| `panel_name` | string | 配置 | 面板名称 | — | 如"客厅灯控面板" |
| `panel_status` | enum | 固件上报 | 见下表 | 见下表 | 面板全局状态 |
| `ha_status` | enum | HA 系统 | `已连接` / `未连接` | lan-connect / lan-disconnect | HA 与面板连接状态 |
| `online_count` | int | 计算 | `X 路在线` | check-circle | 在线回路数 |
| `error_count` | int | 计算 | `X 路异常` | alert-circle | 异常回路数 |
| `last_response_at` | datetime | 固件上报 | `最后响应: XX:XX` | clock-outline | 最后心跳时间 |
| `firmware_version` | string | manifest | `vX.Y.Z` | chip | 固件版本 |

---

## 面板状态映射 (panel_status)

| 英文枚举 | 中文展示 | 图标 | Banner 显示 | 操作可用 |
|---------|---------|------|------------|---------|
| `unpaired` | 未配网 | `mdi:bluetooth-off` | ✅ 显示 | ❌ 禁用 |
| `pairing` | 配对中 | `mdi:bluetooth-connect` | ✅ 显示 | ❌ 禁用 |
| `online` | 在线 | `mdi:lan-connect` | ❌ 隐藏 | ✅ 可用 |
| `offline` | 离线 | `mdi:lan-disconnect` | ✅ 显示 | ❌ 禁用 |
| `syncing` | 同步中 | `mdi:sync` | ❌ 隐藏 | ⚠️ 只读 |
| `error` | 异常 | `mdi:alert-circle` | ✅ 显示 | ❌ 禁用 |

---

## 回路级字段 (Channel)

| 字段名 | 类型 | 来源 | 中文展示 | 说明 |
|--------|------|------|---------|------|
| `channel_id` | string | 配置 | — | ch1 ~ ch4 |
| `entity_id` | string | HA 实体 | — | light.matter_panel_ch1 |
| `channel_name` | string | 配置 | 回路名 | 主灯/辅灯/灯带/壁灯 |
| `power_state` | enum | 固件/HA | 见下表 | 开关状态 |
| `availability` | enum | HA 实体 | `在线` / `离线` | 实体可用性 |
| `last_response_at` | datetime | 固件上报 | — | 最后响应时间 |

---

## 回路状态映射 (power_state)

| 英文枚举 | 中文展示 | 卡片样式 | 按钮状态 | 说明 |
|---------|---------|---------|---------|------|
| `on` | 开启 | 高亮/暖色 | 显示"关" | 灯已开启 |
| `off` | 关闭 | 常规底色 | 显示"开" | 灯已关闭 |
| `pending` | 执行中 | 轻微高亮/闪烁 | 禁用 | 指令已发，等待回写 |
| `no_response` | 无响应 | 红色描边/红点 | 禁用 | 超时未回写 |

---

## HA 实体命名规范

```yaml
# 4 路主版
light.matter_panel_ch1    # 主灯
light.matter_panel_ch2    # 辅灯
light.matter_panel_ch3    # 灯带
light.matter_panel_ch4    # 壁灯

# 2 路裁剪版
light.matter_panel_ch1    # 主灯
light.matter_panel_ch2    # 辅灯
# ch3/ch4 不存在
```

---

## 状态流逻辑

### 点击开/关
```
用户点击 → 本地置 pending → 发 HA service → 等待实体回写
                                          ↓
                              ┌─ 成功 → 落 on/off
                              └─ 超时(3s) → 置 no_response
```

### 按钮禁用规则
- `panel_status` in [unpaired, offline, error]
- `ha_status` = disconnected
- `availability` = offline
- `power_state` = pending

---

## 异常 Banner 映射

| 触发条件 | 标题 | 说明 | 动作 |
|---------|------|------|------|
| `panel_status = unpaired` | 设备未配网 | 请先完成 Matter 配网 | 去配网 |
| `panel_status = offline` | 设备离线 | 请检查电源或网络连接 | 刷新状态 |
| `panel_status = error` | 设备异常 | 当前设备状态异常，请稍后重试 | 重试 |
| `ha_status = disconnected` | Home Assistant 未连接 | 当前无法同步设备状态 | 重新连接 |
| `power_state = no_response` | 设备无响应 | 指令已发送，但设备暂未返回结果 | 重试 |

---

## 埋点字段

| 事件名 | 字段 | 说明 |
|--------|------|------|
| `ha_dashboard_view` | `panelId`, `channelCount` | 面板浏览 |
| `ha_card_click` | `panelId`, `channelId`, `channelCount` | 卡片点击 |
| `ha_control_sent` | `panelId`, `channelId`, `action`, `channelCount` | 控制指令发送 |
| `ha_control_success` | `panelId`, `channelId`, `latencyMs`, `channelCount` | 控制成功 |
| `ha_control_fail` | `panelId`, `channelId`, `errorCode`, `channelCount` | 控制失败 |
| `ha_status_sync` | `panelId`, `channelId`, `powerState`, `channelCount` | 状态同步 |
| `ha_error_exposed` | `panelId`, `errorCode`, `channelCount` | 异常暴露 |
