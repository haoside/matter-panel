# Matter Panel — Home Assistant 集成

## 文件说明

| 文件 | 用途 |
|------|------|
| `configuration.yaml` | HA 核心配置（Matter 集成、传感器、自动化、Lovelace 入口） |
| `ui-lovelace.yaml` | Lovelace Dashboard MVP（总览页 / 房间页 / 详情页 / 异常页） |
| `automations.yaml` | 自动化脚本备份 |
| `matter_discovery.py` | Matter 设备发现辅助脚本 |

## 快速部署

1. 将 `configuration.yaml` 合并到你的 HA `configuration.yaml`
2. 将 `ui-lovelace.yaml` 放到 HA 配置目录（如 `~/.homeassistant/`）
3. 重启 HA
4. 侧边栏会出现 **"Matter Panel 灯控"** 入口

## Lovelace Dashboard 结构

### 总览页
- 顶部状态区：面板状态 / 在线设备数 / 异常设备数
- 异常 Banner（条件显示）：未配网 / 离线 / 异常
- 4 路卡片网格（2x2）：主灯 / 辅灯 / 灯带 / 壁灯
- 底部快捷操作：全开 / 全关 / 刷新

### 房间页
- 客厅：主灯 + 辅灯
- 卧室：灯带 + 壁灯

### 设备详情页
- 面板信息（状态 / 最后响应 / 固件版本）
- 24 小时状态历史
- 刷新按钮

### 异常页
- 未配网 / 离线 / 异常 的条件卡片
- 24 小时事件日志

## 状态映射

| 英文枚举 | 中文展示 | 图标 |
|---------|---------|------|
| `unpaired` | 未配网 | bluetooth-off |
| `pairing` | 配对中 | bluetooth-connect |
| `online` | 在线 | lan-connect |
| `offline` | 离线 | lan-disconnect |
| `syncing` | 同步中 | sync |
| `error` | 异常 | alert-circle |

## 单路实体命名

```yaml
light.matter_panel_ch1  # 主灯
light.matter_panel_ch2  # 辅灯
light.matter_panel_ch3  # 灯带
light.matter_panel_ch4  # 壁灯
```

## 2 路裁剪版适配

将 `ui-lovelace.yaml` 中 `light.matter_panel_ch3` 和 `ch4` 的卡片注释掉即可。
