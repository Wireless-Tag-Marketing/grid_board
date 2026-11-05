# HTTP API

根地址 `http://192.168.4.1`。所有接口仅在设备 AP 上可用，不需要云端账号。写操作使用 `Content-Type: application/json`，正文上限 8192 字节；不提供 CORS。

## 获取状态

`GET /api/config` 返回以下结构，并带 `ETag: "37"` 形式的修订号：

```json
{
  "version": 2,
  "revision": 37,
  "current": {
    "text": "你好\nGrid Board",
    "font_size": 64,
    "fall_ms": 600,
    "show_grid": true,
    "brightness": 80,
    "scroll_speed": 40
  },
  "defaults": {
    "text": "欢迎使用\n中文网格看板\nhttps://wireless-tag.com",
    "font_size": 64,
    "fall_ms": 600,
    "show_grid": true,
    "brightness": 80,
    "scroll_speed": 40
  },
  "device": {
    "ssid": "WIRELESS-GridBoard-Web",
    "ip": "192.168.4.1",
    "firmware": "1.2.2",
    "display_ready": true,
    "persistence_ready": true,
    "recovered_defaults": false,
    "missing_glyphs": 0,
    "width": 1024,
    "height": 600
  },
  "limits": {
    "font_min": 24,
    "font_max": 128,
    "max_text_bytes": 2048,
    "fall_ms_max": 3000,
    "scroll_speed_min": 5,
    "scroll_speed_max": 240
  }
}
```

## 修改显示 / 默认配置

- `POST /api/config`：部分更新当前配置，成功持久化后立即显示。
- `POST /api/defaults`：部分更新默认配置，不改变当前显示。

```bash
curl http://192.168.4.1/api/config \
  -H 'Content-Type: application/json' \
  -H 'If-Match: "37"' \
  --data '{"text":"中文显示\nHello 😀","font_size":64,"show_grid":false,"fall_ms":300,"scroll_speed":60}'
```

支持 `text`、`font_size`、`fall_ms`、`show_grid`、`brightness`、`scroll_speed`。整数不能用字符串、浮点数或布尔值代替；拒绝未知字段、重复字段、无效 UTF-8、NUL、危险控制字符和超过 2048 字节的文字。空字符串清屏；`{}` 不修改当前配置。

内容超出一屏不算错误：服务端完整保存文字，按字号自动换行，显示时上下往返滚动。`scroll_speed` 为 5–240 像素/秒，默认 40，两端停留 1.5 秒；速度与 `fall_ms` 独立，且同样保存在当前配置和默认配置中。

`If-Match` 可选，值必须是最近 GET 返回的完整 ETag。网页总是发送此头；修订号不一致时返回 409，防止多个网页或设备触摸菜单互相覆盖。接收正文后，在同一个 LVGL 锁内核对修订号、准备画面并提交 NVS；未提供时按事务实际执行顺序处理。设备菜单也校验打开时的修订号；网页修改后需要返回并重新打开菜单。

屏幕上下拖动只改变当前浏览位置，不写 NVS，也不改变 `revision`；松手停留 5 秒后续播。双击进入设备菜单，菜单关闭后 3 秒续播。菜单使用同一配置模型和保存事务；文字仍只能通过网页/API 修改。存储格式继续使用版本 2，无需擦除或迁移已有 1.1 配置。

## 恢复

`POST /api/reset` 将 `defaults` 复制到 `current`，然后保存并显示。

`POST /api/factory-reset` 将 `current`、`defaults` 都恢复为固件内置值，并保存、显示。修订号继续递增，不重置为 0。

两个接口都必须发送正文 `{}`，不接受其他字段。成功返回更新后的完整状态。

## 错误

```json
{"error":"滚动速度必须是 5–240 像素/秒之间的整数"}
```

| 状态 | 含义 |
| --- | --- |
| 400 | JSON、字段、Unicode 或文字大小错误，当前配置不变；超过一屏不报错 |
| 403 | Host / Origin 不属于固定 IP 地址 |
| 408 | 请求读取不完整 |
| 409 | If-Match 与最新修订号不符 |
| 413 | 正文为空或超过限制 |
| 415 | Content-Type 不正确 |
| 500 | 字形/页面准备失败或 NVS 写入失败 |
| 503 | 存储或屏幕不可用，或获取显示锁超时 |

写操作只接受 `Host: 192.168.4.1`（可带 `:80`）；浏览器 Origin 如存在也必须匹配。请求超出大小、来源错误等未读取正文的情况，响应后关闭连接。网络断开时客户端无法仅凭超时判断保存是否完成，应重新 GET。

## 存储格式

`board_nvs` 分区，命名空间 `grid_board`，键 `state`，blob 内容为版本 2 JSON，字段为 `version`、`revision`、`current` 和 `defaults`；两个配置各有 6 个字段。可读取原版本 1 的 5 字段配置并保留全部旧值，自动补充 `scroll_speed=40`，下次保存写入版本 2。拒绝不完整或未知版本的记录。`device` 和 `limits` 不存储。NVS 提交成功前不返回写入成功，不记录用户文字或热点密码到运行日志。
