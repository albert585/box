# dlamPrinter 与 Paperang_N1 通信系统逆向分析

> 涉及组件：`ST03_app` (ARM Linux GUI), `dlamPrinter` (ARM Linux 打印服务), `Paperang_N1.bin` (Cortex-M0+ MCU 固件)

---

## 零、组件总览

| 组件 | 架构 | 角色 |
|------|------|------|
| **ST03_app** | ARMv7 Linux (4405 funcs) | GUI主程序，教育类APP |
| **dlamPrinter** | ARMv7 Linux (1653 funcs) | 打印服务守护进程 |
| **Paperang_N1** | Cortex-M0+ (255 funcs) | 喵宝热敏打印机 MCU 固件 |

## 整体通信架构

```
┌────────────────────────────────────────────────────────┐
│                     ST03_app                            │
│  ┌─[cURL/HTTPS]──→ 云端API（题库、VIP、音频下载）       │
│  └─[Unix Socket]──┐                                    │
└───────────────────┼────────────────────────────────────┘
                    │ (JSON 文本流)
┌───────────────────▼────────────────────────────────────┐
│                   dlamPrinter                           │
│  local_server_thread                                   │
│    ↓                                                   │
│  json_cmd_dispatcher                                   │
│    ↓                                                   │
│  pack_and_send_uart_cmd                                │
│    ↓                                                   │
│  [UART] /dev/ttySx  ←──────────────────────┐           │
└──────────────────────┼──────────────────────┼──────────┘
                       │                      │
┌──────────────────────▼──────────────────────┼──────────┐
│              Paperang_N1 (MCU)              │           │
│  UART ISR  →  协议解析  →  打印引擎         │           │
│                                      │      │           │
│                             SPI DMA ←┘      │           │
│                             GPIO ADC (纸检测)│           │
│                             BLE (Nordic)     │           │
└──────────────────────────────────────────────┘           │
```

---

## 一、UART 协议格式

### 完整帧结构 (`build_uart_packet` @ `0x18494`)

```
Offset  Size  Value      Field
─────────────────────────────────────────────
  0      1    0xA5       同步字节 (Sync)
  1      1    0x01       协议版本 (Version)，始终为1
   2      2    total_len  内部包总长 (= 5 + data_len, Little-Endian)
  ─────── CRC 计算范围 (offset 4 起, total_len 字节) ───────
  4      1    main_cmd   主命令码
  5      1    sub_cmd    子命令码
  6      1    ack_flag   应答标志 (1=需ACK, 2=是ACK应答)
  7      2    data_len   数据长度 (Little-Endian)
  9    data_len data     实际数据
  ─────── CRC 计算结束 ───────
  +0     4    CRC32      对 [offset+4 .. offset+4+total_len) 的 CRC32
  +4     1    0x5A       结束标记 ('Z')
```

**总帧长 = 14 + data_len 字节**

**CRC32 计算**: `calculate_crc32_with_seed(buf + offset + 4, total_len)`，其中 `total_len = *(uint16_le*)(buf + offset + 2)`

### 发送侧 (`pack_and_send_uart_cmd`)

```c
pack_and_send_uart_cmd(main_cmd, sub_cmd, ack_flag, data_ptr, data_len)
  → 内部构建 9 字节头: {main:1, sub:1, ack:1, datalen:2, data_ptr:4}
  → build_uart_packet(in, data_len+5, out)  // in包总长 = 5+datalen
      → 组装帧: [A5][01][total_len(LE)][in...][CRC32][5A]
  → uart_send_packet(out, total_len+9)      // 发送 14+datalen 字节
```

### 响应包解析 (`parse_uart_response`)

```c
parse_uart_response(buf, len):
  1. 搜索同步字节 0xA5
  2. 读取 offset+2 处的 total_len (Little-Endian)
  3. 验证 offset + total_len + 8 处在 buf 范围内且为 'Z'
  4. 验证 CRC32(buf + offset + 4, total_len) == *(uint*)(buf + offset + 4 + total_len)
  5. 返回同步字节偏移量，失败返回 -1
```

---

## 二、UART 命令注册表 (`uart_init` @ `0x18a10`)

系统启动时，`uart_init()` 将 34 条 `(main_cmd, sub_cmd)` 注册到 `dispatch_mcu_response` 的 64 项分发表中：

| main | sub | 处理器 | D-Bus 信号 | 功能 |
|------|-----|--------|-----------|------|
| 0x01 | 0x01 | cmd_get_event | — | MCU 事件上报 |
| 0x01 | 0x03 | cmd_ack_with_ack_str | `0x3C` | 字符串 ACK |
| 0x01 | 0x05 | cmd_ack_buf_common | `0x40` | 通用 ACK |
| 0x01 | 0x06 | cmd_get_dev_info | — | 查询设备信息 |
| 0x01 | 0x07 | cmd_ack_with_ack_int | `3` | 整数 ACK |
| 0x01 | 0x08 | cmd_get_dev_hardware_ver | — | 查询硬件版本 |
| 0x01 | 0x0B | cmd_ack_for_mcu_bat | — | 电池容量上报 |
| 0x01 | 0x0C | cmd_ack_for_mcu_bat | — | 电池电压上报 |
| 0x01 | 0x0E | cmd_get_dev_product_model | — | 查询产品型号 |
| 0x01 | 0x13 | cmd_ack_buf_common | `8` | 通用 ACK (OTA) |
| 0x01 | 0x14 | cmd_get_dev_max_len | — | 查询最大打印长度 |
| 0x01 | 0x15 | cmd_get_protocol_ver | — | 查询协议版本 |
| 0x01 | 0x17 | cmd_ack_buf_common | `0x6B` | 通用 ACK |
| 0x01 | 0x18 | cmd_ack_with_ack_str | `0x41` | 字符串 ACK |
| 0x01 | 0x1D | cmd_ack_for_mcu_bat | — | MCU 状态上报 |
| 0x01 | 0x20 | *未命名* | — | 保留 |
| 0x03 | 0x07 | cmd_ack_with_ack_int | `0x3D` | MAC地址 ACK |
| 0x04 | 0x03 | cmd_ack_with_ack_str | `0x3E` | 字符串 ACK |
| 0x05 | 0x01 | *未命名* | — | 保留查询 |
| 0x05 | 0x02 | *未命名* | — | 保留查询 |
| 0x05 | 0x04 | *未命名* | — | 保留查询 |
| **0x05** | **0x05** | **cmd_input_buf_common** | `0x6A/0x65/0x66` | **打印机事件输入** |
| 0x05 | 0x0C | cmd_ack_buf_common | `0x66` | 通用 ACK |
| 0x05 | 0x0D | cmd_ack_buf_common | `0x65` | 通用 ACK |
| **0x05** | **0x0F** | **cmd_input_buf_common** | `0x6A/0x65/0x66` | **打印机事件输入** |
| 0x05 | 0x10 | cmd_ack_with_ack_int | `6` | 整数 ACK |
| 0x05 | 0x11 | cmd_ack_with_ack_int | `0x3F` | 整数 ACK |
| 0x05 | 0x16 | cmd_ack_buf_common | `0x69` | 打印结束 ACK |
| 0x05 | 0x19 | cmd_ack_buf_common | `0x67` | 打印开始 ACK |
| 0x05 | 0x1A | cmd_ack_buf_common | `0x68` | 打印进度 ACK |
| 0x05 | 0x23 | cmd_ack_buf_common | `1` | 通用 ACK |
| 0x05 | 0x24 | cmd_ack_buf_common | `2` | 通用 ACK |
| 0x05 | 0x27 | *未命名* | — | 查询 |
| 0x05 | 0x28 | *未命名* | — | 保留 |

> **未注册的单向命令**（json_cmd_dispatcher 发送但无 handler）：
> - `(5, 0x17)` → cmd=7 自检页 / cmd=0 type=5 测试页，MCU 内置图案
> - `(5, 0x18)` → cmd=0 type=4 QR码页，MCU 打印二维码
> - `(5, 0x1B)` → 打印行数据包（MCU 直接处理，不通过 dispatch_mcu_response）

---

## 三、处理器函数详解

### 1. `cmd_input_buf_common` — 打印机事件输入接收

**注册**: `(0x05, 0x05)` 和 `(0x05, 0x0F)`

MCU **主动上报**事件（非查询响应）。Payload 是 4 字节结构体数组：

```
struct input_event {
    uint8_t  type;      // 事件类型
    uint8_t  padding;   // 填充
    uint16_t value;     // 事件值
};
```

| type | D-Bus 信号 | 含义 |
|------|-----------|------|
| 1 | `0x6A` | 缺纸 / 纸张状态变更 |
| 2 | `0x65` | 打印机错误/异常状态 |
| 3 | `0x66` | 打印任务完成/就绪 |

**使用场景**: MCU 检测到打印机物理状态变化时（如缺纸、卡纸、打印完成）主动上报。ST03_app 通过消息队列接收 D-Bus 信号后更新 UI。

---

### 2. `cmd_ack_buf_common` — 通用 ACK 处理器

**响应 payload**: `{ack_result: uint8_t (0=成功), padding: uint8_t[3]}`

| D-Bus 信号 | 特殊操作 | 场景 |
|-----------|----------|------|
| `0x67` | 成功后 `system("...")` 启动外部命令 | 打印任务开始 |
| `0x68` | 仅发送 D-Bus | 打印进度反馈 |
| `0x69` | 成功后 `system("...")` 执行清理命令 | 打印任务结束 |
| `0x65` | 仅发送 D-Bus | 打印机状态上报 |
| `0x66` | 仅发送 D-Bus | 打印机状态上报 |
| 其他 | 仅发送 D-Bus (ack_result, 0=成功) | 通用命令确认 |

**使用场景**: 执行**发送类**命令（如打印开始/结束、状态查询）后，等待 MCU 确认结果并反馈给上层。

---

### 3. `cmd_ack_with_ack_int` — 带整数的 ACK

**响应 payload**: `{ack_result: uint8_t, len: uint16_t, data: [value: uint16_t]}`

通过 D-Bus 发送：`dbus_emit_response(signal_id, ack_result, data_value)`

| UART (main, sub) | D-Bus | JSON cmd | 用途 |
|------------------|-------|----------|------|
| (0x01, 0x07) | `3` | 3 | 查询返回整数 |
| (0x05, 0x10) | `6` | 6 | 查询返回整数 |
| (0x03, 0x07) | `0x3D` | 0x3D | MAC地址查询结果 |
| (0x05, 0x11) | `0x3F` | 0x3F | 单字节参数查询结果 |

**使用场景**: 查询型命令的响应，数据为单个 16 位整数值。

---

### 4. `cmd_ack_with_ack_str` — 带字符串的 ACK

**响应 payload**: `{ack_result: uint8_t, len: uint16_t, data: [string]}`

通过 D-Bus 发送字符串数据（区别于 `cmd_ack_buf_common` 只发 ack 状态）。

| UART (main, sub) | D-Bus | JSON cmd | 用途 |
|------------------|-------|----------|------|
| (0x01, 0x03) | `0x3C` | 0x3C | WiFi SSID/密码等 |
| (0x04, 0x03) | `0x3E` | 0x3E | 蓝牙配置字符串 |
| (0x01, 0x18) | `0x41` | 0x41 | 网络配置字符串 |

**使用场景**: WiFi SSID/密码、蓝牙设备名等字符串类型配置或查询结果。

---

### 5. `cmd_ack_for_mcu_bat` — 电池/状态上报

**注册**: `(0x01, 0x0B)`, `(0x01, 0x0C)`, `(0x01, 0x1D)`

处理电池电压/容量及 MCU 状态：

| (main, sub) | 系统文件 | 操作 |
|-------------|----------|------|
| (0x01, 0x0C) | `/sys/class/power_supply/battery/voltage_now` | 读取电压(mV)→`/1000`→填充响应 |
| (0x01, 0x0B) | `/sys/class/power_supply/battery/capacity` | 读取百分比(%)→`*10`→填充响应 |
| (0x01, 0x1D) | *内部数据* | MCU 通用状态查询 |

使用 `send_raw_uart_cmd(1, 0x0C/0x0B, 2, &packet, 5)` 返回数据给 MCU。

**使用场景**: MCU 定期轮询或被查询时上报电池信息。

---

### 6. `cmd_get_*` 系列 — 被动查询处理器（仅日志）

这些处理器解析 ACK 响应包，**仅通过 printf 输出日志，不发送 D-Bus 信号**。

| 函数 | ACK 负载格式 | 查询内容 |
|------|-------------|----------|
| `cmd_get_dev_info` | `{result, len, [name_str]}` | 设备名称 |
| `cmd_get_dev_hardware_ver` | `{result, len, [ver_str]}` | 硬件版本号 |
| `cmd_get_dev_product_model` | `{result, len, [model_id, type, sn...]}` | 产品型号/序列号 |
| `cmd_get_dev_temp` | `{result, len, [temp: uint16]}` | 打印头温度（存全局变量） |
| `cmd_get_protocol_ver` | `{result, len, [ver_str]}` | UART 协议版本 |
| `cmd_tp_get_info` | `{result, len, [info_str]}` | 触摸屏信息 |
| `cmd_tp_get_dev_dpi` | `{result, len, [dpi: uint16]}` | 触摸屏 DPI |
| `cmd_tp_get_hot_spot_num` | `{result, len, [num: uint16]}` | 触摸屏热点数 |
| `cmd_tp_get_dev_support_size` | `{result, len, [size: uint16]}` | 触摸屏支持尺寸 |
| `cmd_tp_get_dev_support_size_info` | `{result, len, [info]}` | 触摸屏尺寸详情 |
| `cmd_get_dev_max_len` | `{result, len, [max_len]}` | 最大打印长度 |
| `cmd_get_dev_package_cache_max_cnt` | `{result, len, [cnt]}` | 包缓存最大数 |

**使用场景**: `cmd_mode` 调试模式 (`dlamPrinter -c <file>`) 下，或 `thread_uart_func` 主循环中处理 MCU 被动响应。

---

## 四、JSON 命令分发详解 (`json_cmd_dispatcher` @ `0x54d9c`)

### 协议说明

**输入**：Unix socket 接收的 JSON 文本，通过 `cJSON_ParseWithOpts` 解析。

**核心逻辑**：
```c
json = cJSON_ParseWithOpts(input);
cmd = cJSON_GetObjectItem(json, "cmd");   // 必须为整数(int)
switch (cmd->valueint) { ... }
```

**为何 cmd 值使用 hex**：开发者按功能块分组，用 hex 字面量在源码中区分命令类别：
- `0x00`~`0x0C` (0~12) — 系统基础命令
- `0x3C`~`0x41` (60~65) — 网络/蓝牙配置命令
- `0x118`~`0x122` (280~290) — 打印控制命令

编译后的 switch-case 没有间隔，各组 hex 值恰好落在不同区间，便于代码阅读。

### 详细 cmd 列表

> 格式：`{"cmd": N, "key1": value1, ...}`

| cmd | JSON 字段 | 类型 | UART (m,s) | 说明 |
|-----|-----------|------|------------|------|
| **0** | `"type"`, `"timeout"`, `"data"` | int, int, string | 见下 | **初始化/打印** |
| **1** | 无 | — | (5, 0x23) | 查询设备状态，返回 D-Bus `1` |
| **2** | 无 | — | (5, 0x24) | 查询设备状态，返回 D-Bus `2` |
| **3** | 无 | — | (1, 7) | 发送配置命令，返回 D-Bus `3`(int) |
| **4** | 无 | — | (5, 0x0C) | 查询打印机状态，返回 D-Bus `0x66` |
| **5** | 无 | — | (5, 0x0D) | 查询打印机状态，返回 D-Bus `0x65` |
| **6** | 无 | — | (5, 0x10) | 查询，返回 D-Bus `6`(int) |
| **7** | 无 | — | (5, 0x17) | **打印自检页**，MCU内置固件图案，日志 `MM_PRINT_SELF_TEST_PAGE` |
| **8** | `"ota_url"` | string | — | OTA升级，调用 `enter_ota_mode(url)` |
| **9** | 无 | — | — | 蓝牙/GATT栈复位 |
| **0xA** | 无 | — | (5, 0x27) | 查询 |
| **0xB** | `"val"` | int (≤2) | raw(1,0x16) | 发送 raw 4字节数据到MCU |
| **0xC** | 无 | — | — | 查询蓝牙/GATT活跃状态，返回 D-Bus `0x6F` |
| **0x3C** | `"wifi_cfg"` | string | (1, 3) | WiFi配置字符串(WiFi名+密码) |
| **0x3D** | `"mac_str"` | string | (3, 7) | MAC地址(格式: "XX:XX:XX:XX:XX:XX")，sscanf解析为6字节 |
| **0x3E** | `"bt_cfg"` | string | (4, 3) | 蓝牙配置字符串 |
| **0x3F** | `"param"` | int | (5, 0x11) | 单字节参数，默认值0x4B(75) |
| **0x40** | `"wifi_ssid"` | string | (1, 5) | WiFi SSID/密码字符串 |
| **0x41** | `"net_cfg"` | string | (1, 0x18) | 网络配置字符串 |
| **0x118** | 无 | — | — | **开始打印**，失败重试1次，返回 D-Bus 0xDC/0xDD |
| **0x119** | 无 | — | — | **停止打印**，返回 D-Bus 0xDE/0xDF |
| **0x11A** | 无 | — | — | printer_action(10) |
| **0x11B** | 无 | — | — | printer_action(11) |
| **0x11C** | 无 | — | — | printer_action(16) |
| **0x11D** | 无 | — | — | printer_action(8) |
| **0x11E** | 无 | — | — | printer_action(10) |
| **0x11F** | 无 | — | — | printer_action(31) |
| **0x120** | 无 | — | — | printer_action(10) |
| **0x121** | 无 | — | — | printer_action(12) |
| **0x122** | 无 | — | — | printer_action(13) |
| **剩余** | — | — | — | 保留/未实现（fallthrough到 break） |

### `cmd_init_handler` 子命令 (cmd=0) — 完整分析

JSON 需包含 `type`, `timeout`, `data` 三个字段：

| 字段 | 类型 | 默认 | 说明 |
|------|------|------|------|
| `type` | int | 必填 | 子操作类型 (0~5) |
| `timeout` | int | 1 | type=0: 输出位深度(1/4/8/24/32)；其余保留 |
| `data` | string | 必填 | type=0/3: BMP文件路径；type=4/5: 可为空 |

#### type=0 — 打印图片（完整流程）

`printer_process_image_file(filepath, timeout)`

```
1. bmp_get_color_depth(filepath)
   → 解析 BMP 头，获取位深度: 1/4/8/24/32

2. 格式判断:
   1/4/8 位 → 跳过灰度转换，走直接路径 (local_14=1)
   24/32位 → 走灰度转换路径 (convert_rgb_to_gray / convert_32bit_to_gray)

3. BMP 加载:
   fopen(filepath, "rb") → fread(bmp_header, 0x36=54字节)
   验证 "BM" 魔数 (0x4D42)

4. 像素提取 (宽度限制 ≤0x240=576像素):
   24/32位: fseek→fread→逐像素灰度转换→1位/像素位图
   1/4/8位: 直接 fread 原始像素

5. 位图压缩:
   timeout=4 → mbImg2GrayscaleData(buf, w, h, 0, 0x10, &result)
   timeout≠4 → MMJ_PrinterImgBin(params, ..., 4, 0)
   → 生成打印行数据(每行 1728 字节)

6. 分包发送:
   pack_and_send_uart_cmd(5, 0x19, 1)          ← 打印开始→MCU加热
   for each 1728B line:
     pack_and_send_uart_cmd(5, 0x1B, 1, pkt, 0x6CC) ← 发送行数据
   pack_and_send_uart_cmd(5, 0x1A, 1)          ← 打印结束→走纸
   pack_and_send_uart_cmd(5, 0x16, 1, &res, 2) ← 确认/清理
```

**行数据包结构** (0x6CC = 1740 字节):

```
Offset  Size  字段
  0      2    packet_id   (递增)
  2      2    0x6C8       (固定magic)
  4      1    bit_depth   (timeout参数)
  5      2    line_width  (≤576)
  7      3    0           (reserved)
 10      2    0x6C0       (data_len = 1728)
 12    0x6C0  grayscale   (1bpp 位图数据)
```

**UART 命令与 D-Bus 回调**:

| 步骤 | UART (m,s) | 处理器 | D-Bus | 含义 |
|------|------------|--------|-------|------|
| 开始 | (5, 0x19) | cmd_ack_buf_common | `0x67`→`system()` | 通知 MCU 开始打印，触发 system() 启动服务 |
| 数据 | (5, 0x1B) | cmd_ack_buf_common | —(无注册) | 每包 1740 字节，循环发送所有行 |
| 结束 | (5, 0x1A) | cmd_ack_buf_common | `0x68` | 通知 MCU 打印结束 |
| 确认 | (5, 0x16) | cmd_ack_buf_common | `0x69`→`system()` | 确认完成，触发 system() 清理 |

#### type=3 — 直接 BMP 发送

`printer_send_bmp_data(filepath)`

**与 type=0 的区别**：跳过灰度转换，只支持 1/4 位 BMP，直接发送原始像素。

```
1. bmp_get_color_depth → 只接受 1或4
2. fopen + fread 像素数据（跳过 BMP 头）
3. 按行分包 pack_and_send_uart_cmd(5, 0x1B, ...)
4. pack_and_send_uart_cmd(5, 0x1A, 1) 结束
5. pack_and_send_uart_cmd(5, 0x16, 1, &zero, 2) 确认
```

#### type=1, type=2 — 预留

仅打印日志，不执行任何操作。

#### type=4 — 打印 QR 码页

```
pack_and_send_uart_cmd(5, 0x18, 1, 0, 0)  → MCU
日志: "print QR PAGE"
MCU 根据内置数据打印 QR 码测试页（可能是设备二维码）
```

#### type=5 — 打印测试页（另一入口）

```
pack_and_send_uart_cmd(5, 0x17, 1, 0, 0)  → MCU
日志: "print TEST PAGE"
效果同 cmd=7（自检页），UART 命令完全一致 (5, 0x17)
区别: 日志前缀不同，猜测 board_test 区分测试来源
```

**注意**：`(5, 0x17)` 和 `(5, 0x18)` 都没有注册 handler，是一对一单向命令。

### 打印控制类 cmd (0x11A~0x122) 的实现

这些 cmd **不走 UART 协议帧**，而是通过 `printer_action()` **直接 write() 到打印机设备 fd**：

```c
printer_action(data_ptr, data_len):
    if (printer_fd < 0) return -1;
    write_loop(printer_fd, data_ptr, data_len);  // 确保全部写入
    usleep(100000);   // 100ms 等待
    return 0;
```

| cmd | 数据 | 含义推测 |
|-----|------|----------|
| 0x11A | 10字节 | 暂停打印 |
| 0x11B | 11字节 | 恢复打印 |
| 0x11C | 16字节 | 继续进纸 |
| 0x11D | 8字节 | 取消/终止 |
| 0x11E | 10字节 | 打印动作(重复) |
| 0x11F | 31字节 | 特殊打印动作 |
| 0x120 | 10字节 | 打印动作 |
| 0x121 | 12字节 | 走纸/切纸 |
| 0x122 | 13字节 | 走纸/切纸 |

这些是**私有打印控制码**，直接写到打印机驱动 fd（`/dev/ttySx` 或类似），不经过 UART 命令帧 `[A5][...][5A]` 封装。

### printer_start_print / printer_stop_print

```
printer_start_print:
  mutex_lock
  if 未启动:
    system("启动打印服务进程")
    access("/dev/ttySx", F_OK) ×2 等待设备文件
    open("/dev/ttySx", O_RDWR|O_CREAT)
    get_config_value("density")          读取打印浓度
    config_serial(fd, density)           配置串口参数
    init_print_params(fd, 8, 1, 78, 78)  初始化(波特率/数据位/停止位等?)
  mutex_unlock

printer_stop_print:
  mutex_lock
  close(printer_fd)
  system("杀掉打印服务进程")
  等待设备释放
  mutex_unlock
```

### UART 数据流向 (具体字节级)

以 `cmd=0x3C` (WiFi配置) 为例，完整数据流：

```
1. ST03_app → Unix Socket:
   {"cmd":60, "wifi_cfg": "myssid:mypassword"}
   
2. json_cmd_dispatcher 解析 → 提取字符串 "myssid:mypassword"
   
3. pack_and_send_uart_cmd(1, 3, 1, "myssid:mypassword", 19):
   ┌──────┬──────┬──────┬──────┬──────────────┬─────────────────────┐
   │ 0x01 │ 0x03 │ 0x01 │ 0x13 │ 0x00 │ ptr  │ "myssid:mypassword" │
   │main  │sub   │ack   │len=19(LE) │data ptr │   19 bytes          │
   └──────┴──────┴──────┴───────────┴─────────┴─────────────────────┘
   
4. build_uart_packet → UART帧:
   ┌──────┬──────┬────────┬──────┬──────┬──────┬────────┬─────────────────┬──────────┬──────┐
   │ 0xA5 │ 0x01 │ 0x0018 │ 0x01 │ 0x03 │ 0x01 │ 0x0013 │ "myssid:..."(19)│ CRC32    │ 0x5A │
   │sync  │ver=1 │len=24  │main  │sub   │ack   │datalen │                 │          │ 'Z'  │
   └──────┴──────┴────────┴──────┴──────┴──────┴────────┴─────────────────┴──────────┴──────┘
   
5. MCU 执行后返回ACK:
   [A5][01][00 07][01][03][02][00 00][03][CRC][5A]
   main=0x01, sub=0x03, ack=0x02(应答包), datalen=0, result=0x03
   
6. dispatch_mcu_response → cmd_ack_with_ack_str(..., 0x3C=60):
   dbus_emit_response(60, ack_result=3, data_str)
```

---

## 六、`cmd_mode` — 调试模式

命令行调试入口，用法：`dlamPrinter -c <hex_command_file>`

流程：
1. 打开文件，读取 hex 文本
2. `strtol(hex, 16)` 逐5字符转换
3. `uart_send()` 发送
4. `uart_init()` + `uart_read_data()` 读回响应
5. raw 数据写入 `/tmp/cmd_mode_rec.txt`
6. `parse_uart_response()` 校验 CRC
7. `dispatch_mcu_response()` 分发处理

---

## 七、D-Bus 信号汇总

| 信号ID | 方向 | 含义 | 触发条件 |
|--------|------|------|----------|
| `0x65` | MCU→APP | 打印机错误/缺纸 | cmd_input_buf_common type=3 或 cmd_ack_buf_common |
| `0x66` | MCU→APP | 打印机就绪/完成 | cmd_input_buf_common type=2 或 cmd_ack_buf_common |
| `0x67` | MCU→APP | 打印开始确认 | cmd_ack_buf_common (5,0x19)→触发`system()` |
| `0x68` | MCU→APP | 打印进度 | cmd_ack_buf_common (5,0x1A) |
| `0x69` | MCU→APP | 打印完成确认 | cmd_ack_buf_common (5,0x16)→触发`system()` |
| `0x6A` | MCU→APP | 缺纸/纸张状态 | cmd_input_buf_common type=1 |
| `0x6B` | MCU→APP | 通用状态反馈 | cmd_ack_buf_common (1,0x17) |
| `0x6F` | MCU→APP | 蓝牙/GATT活跃状态 | is_bluetooth_active() 查询结果 |
| `0xDC` | APP→? | 打印开始成功 | printer_start_print() 成功 |
| `0xDD` | APP→? | 打印开始失败 | printer_start_print() 失败 |
| `0xDE` | APP→? | 打印停止成功 | printer_stop_print() 成功 |
| `0xDF` | APP→? | 打印停止失败 | printer_stop_print() 失败 |

---

## 八、完整通信流程图

```
┌──────────────────────────────────────────────────────┐
│                    ST03_app (GUI)                     │
│                                                       │
│  ┌─[cURL/HTTPS]──→ 云端API（题库、VIP、音频下载）    │
│  └─[Unix Socket]──┐                                  │
└───────────────────┼──────────────────────────────────┘
                    │ JSON: {"cmd":N, ...}
┌───────────────────▼──────────────────────────────────┐
│                  dlamPrinter (打印服务)                │
│                                                       │
│  local_server_thread (单连接, accept→recv→dispatch)  │
│         │                                             │
│    json_cmd_dispatcher                                │
│         │                                             │
│    ┌────┴────┬────────────┬────────────┐              │
│    │         │            │            │              │
│  打印控制   查询配置    系统操作     OTA升级          │
│  cmd=       cmd=        cmd=        cmd=8             │
│  0x118-     1-7,0xA,    9,0xB,0xC                    │
│  0x122      0x3C-0x41                                 │
│    │         │            │            │              │
│    └────┬────┴────────────┴────────────┘              │
│         │                                             │
│   pack_and_send_uart_cmd / send_raw_uart_cmd         │
│         │                                             │
│    UART /dev/ttySx  ←───────────────┐                │
└────────┼────────────────────────────┼────────────────┘
         │                            │
┌────────▼────────────────────────────┼────────────────┐
│           Paperang_N1 MCU           │                │
│                                     │                │
│  UART ISR → [A5][...][CRC][5A] → 命令解析            │
│         │                            │                │
│    ┌────┴────┬────────────┬─────────┴───┐            │
│    │         │            │             │            │
│  SPI DMA    GPIO ADC    BLE Nordic   系统管理        │
│  (打印头)   (纸检测)    (无线打印)   (电池/温度)     │
│    │         │            │             │            │
│    └────┬────┴────────────┴─────────────┘            │
│         │                                            │
│    响应包: [A5][DATA][CRC][5A]                       │
│         │                                            │
└─────────┼────────────────────────────────────────────┘
          │
┌─────────▼────────────────────────────────────────────┐
│                  dlamPrinter                          │
│                                                       │
│  thread_uart_func (接收→解析→dispatch)               │
│         │                                             │
│  dispatch_mcu_response (64项表匹配)                  │
│         │                                             │
│    ┌────┴────────┬──────────┬──────────┐              │
│    │             │          │          │              │
│  cmd_input_   cmd_ack_   cmd_ack_   cmd_get_*        │
│  buf_common   buf_common with_*_int/str (仅日志)     │
│    │             │          │                         │
│    ▼             ▼          ▼                         │
│  D-Bus 信号    D-Bus信号  D-Bus信号                  │
│  0x65/66/6A   0x67/68/69  3/6/0x3C-0x41              │
│    │             │          │                         │
│    └─────────────┴──────────┘                         │
│                  │                                    │
│            Unix Socket 回传                           │
│            (或 D-Bus 广播)                            │
└──────────────────┼────────────────────────────────────┘
                   │
┌──────────────────▼────────────────────────────────────┐
│                   ST03_app                            │
│                                                       │
│  recv() 收到 JSON 响应 → 反序列化 → 更新 UI           │
│  或通过 msg 队列接收 D-Bus 信号 → 状态栏更新          │
└───────────────────────────────────────────────────────┘
```

---

---

## 九、Unix Socket 直连通道

`dlamPrinter` 除了 D-Bus 外，还提供一条 **Unix Domain Socket** 直连通道，供 ST03_app 等客户端直接发送 JSON 命令。

### 服务端 (`local_server_thread` @ `0x55b90`)

```c
void local_server_thread() {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    unlink(socket_path);           // 清理旧 socket 文件
    bind(fd, &addr, sizeof(addr)); // 绑定 Unix socket 路径
    listen(fd, 40);                // 最大 40 个等待连接

    while (1) {
        // ⚠️ 关键：单连接阻塞模式
        client_fd = accept(fd, ...);  // 阻塞等待客户端

        while (client_fd != 0) {
            n = recv(client_fd, buf, 512, 0);

            if (n <= 0) {             // 客户端断开或出错
                close(client_fd);
                client_fd = 0;        // 跳出内层循环
                break;
            }

            // 从字节流中提取完整 JSON 对象
            // 通过计数 '{' 和 '}' 配对来分割
            int brace_depth = 0;
            for (i = 0; i < strlen(buf); i++) {
                if (buf[i] == '{') {
                    json_start = i;
                    brace_depth++;
                } else if (buf[i] == '}') {
                    brace_depth--;
                    if (brace_depth == 0) {
                        json_cmd_dispatcher(json_buf);  // 分发处理
                        memset(json_buf, 0, 512);
                    }
                }
            }
        }
        poll(NULL, 0, 1000);    // 断开后等 1 秒再 accept
    }
}
```

### 关键特征与限制

| 特征 | 说明 |
|------|------|
| **协议** | Unix Domain Socket (SOCK_STREAM) |
| **数据格式** | 纯文本 JSON，`{...}` 对象 |
| **并发模型** | **单连接，串行处理** |
| **断连处理** | 客户端断开后自动回到 accept 等待 |
| **缓冲区** | 512 字节固定缓冲 |
| **超时** | recv 无超时，阻塞等待 |

### 单连接限制的影响

由于 `local_server_thread` 的实现是 **阻塞式单连接模型**：

```
accept() ──→ recv() ←→ json_cmd_dispatcher() ──→ recv() ←→ ...
              │                                        │
              └── 客户端断开 ──→ close() ──→ poll(1s) ──→ accept()
```

- **同一时间只能有一个客户端连接**。如果 ST03_app 已连接，其他进程的 connect 会被 backlog 缓存（最多 40 个），但不会处理
- 客户端断开后，必须等待 1 秒的 poll 间隔才能接受新连接
- 这限制了系统的扩展性：不能同时有多个程序直接控制打印机

### 为什么用 Unix Socket 而不是只用 D-Bus？

D-Bus 需要 GLib 主循环驱动，引入较重依赖。Unix Socket 是更轻量的替代方案：
- ST03_app 作为**主要客户端**，通过 Unix Socket 发送 JSON 命令
- D-Bus 用于**系统级事件广播**（网络状态、蓝牙事件等）

---

## 十、Paperang_N1 MCU 固件分析

### 基本信息

| 属性 | 值 |
|------|-----|
| **文件名** | `/files/Paperang_N1.bin` |
| **设备** | 喵宝 Paperang N1 热敏打印机 |
| **CPU** | Cortex-M0+ (ARMv6-M) |
| **固件大小** | 67,180 字节 (~66KB) |
| **函数数量** | 255 |
| **构建日期** | Jul 17 2023 19:15:13 |
| **初始 SP** | `0x2000CED8` (RAM: ~52KB) |
| **向量表** | 16 个标准 Cortex-M 向量（Reset, NMI, HardFault, ..., SysTick, IRQ） |

### 硬件特性

| 特性 | 证据 | 用途 |
|------|------|------|
| **双通信通道** | `send_data()` 两条路径：256B 和 8KB | UART(256B) + BLE(8KB) |
| **SPI DMA** | `"spi dma send/recv complete irq"` | 热敏打印头数据传输 |
| **纸检测** | `gpad.*` 变量组 + `"out of paper"` | GPIO ADC 检测纸张状态 |
| **蓝牙** | BT MAC 地址, Nordic 风格 UART Service | BLE 无线打印 |
| **WiFi** | WiFi MAC 地址 | WiFi 连接（通过主控） |
| **NVM 存储** | `SysSave.*` 变量 | 打印参数持久化（电压/速度/密度） |

### 纸检测系统 (GPAD = GPIO + ADC)

固件使用模拟信号检测纸张状态，通过 ADC 读取光传感器：

```
gpad 变量组:
  wave_add[3]      → 波形累加值
  wave_maxAd[n]    → 最大 ADC 值
  wave_pulse_maxad  → 脉冲最大 ADC
  firstCheckNum     → 首次检测计数
  adctnum          → ADC 采样数
  paperad          → 纸张 ADC 阈值
  judgead          → 判定 ADC 值
  gapadby / gapadHl → 间隙检测阈值
  paperadby / paperadHl → 纸张检测阈值
  maxad / minad     → ADC 范围

纸张类型: Black mark(黑标纸) / Label(标签纸) / Receipt(普通纸)
```

### 打印参数

```
型号:     P100R20A000001 / Paperang_N1 / Paperang_P3
打印电压:  Print Voltage:%d.%d%d   (可调)
打印速度:  Print Speed:%d mm/s     (可调)
打印浓度:  Print Density:%d        (可调)
切割参数:  Values for start cutting:%d
打印温度:  Temperature:%d
```

### 数据发送函数 (`FUN_00010178` / `send_data`)

MCU 固件中有两条独立的数据发送路径：

```c
void send_data(uint8_t *data, uint32_t len) {
    if (is_uart_mode()) {
        // 路径1: UART TX (256B 环形缓冲)
        // 用于与 dlamPrinter 的有线通信
        *status = 0x32;
        for (i = 0; i < len; i++)
            uart_tx_buf[write_idx++ % 256] = data[i];
        uart_tx_timer = 100;       // 100ms 发送超时
    } else {
        // 路径2: BLE TX (8KB 环形缓冲，DMA 传输)
        // 用于无线蓝牙打印
        if (mode != 3) set_mode(3);
        ctrl_reg |= 0x10;          // 使能 DMA
        dma_timeout = 2000;        // 2s DMA 超时
        for (i = 0; i < len; i++)
            ble_buf[write_idx++ % 8192] = data[i];
        trigger_dma_send();
        ble_tx_timer = 100;
    }
}
```

### UART 协议 (MCU 侧视角)

基于 dlamPrinter 的 `parse_uart_response` 分析，从 MCU 发出的响应包格式：

```
MCU → dlamPrinter:
  [A5] [01] [LEN_L] [LEN_H] [DATA...] [CRC32] [5A]
   ↑                         ↑            ↑      ↑
  起始(0xA5)                数据体       CRC32   结束('Z')

dlamPrinter → MCU:
  pack_and_send_uart_cmd(main_cmd, sub_cmd, ack_flag, data, len)
  → 9字节头 + 数据
```

### 与 dlamPrinter 的对应关系

| dlamPrinter (Linux) | Paperang_N1 (MCU) |
|---------------------|-------------------|
| `pack_and_send_uart_cmd(5,0x19,...)` | 打印开始命令 → 启动加热/走纸 |
| `pack_and_send_uart_cmd(5,0x1B,...)` | 打印行数据 → SPI DMA 到打印头 |
| `pack_and_send_uart_cmd(5,0x1A,...)` | 打印结束命令 → 切纸/停止 |
| `cmd_input_buf_common` 收到 type=1 | MCU 检测到缺纸 → 上报 `0x6A` |
| `cmd_input_buf_common` 收到 type=2 | MCU 检测到错误 → 上报 `0x65` |
| `cmd_input_buf_common` 收到 type=3 | MCU 打印完成 → 上报 `0x66` |
| `(0x01,0x0C)` 电池电压查询 | MCU 读取 ADC → 返回电压值 |
| `(0x01,0x0B)` 电池容量查询 | MCU 返回电量百分比 |
| `thread_uart_func` | UART RX ISR → 协议解析 → 命令执行 |
| BLE GATT 线程 | send_data() 路径2 → BLE 通知 |

---

## 十一、线程架构

### dlamPrinter

`dlamPrinter::main()` 在正常模式下启动 **6 个线程**（SCHED_FIFO, 优先级=2）：

| 线程 | 创建时机 | 函数 | 职责 |
|------|---------|------|------|
| 1 | 立即 | *(app+thread1) | 网络/服务监听 |
| 2 | 立即 | *(app+thread2) | 消息处理 |
| 3 | 立即 | *(app+thread3) | 后台任务 |
| 4 | 立即 | *(app+thread4) | 后台任务 |
| 5 | sleep(2)后 | *(app+thread5) | 延迟启动服务 |
| 6 | sleep(2)后 | *(app+thread6) | 主事件循环 |

已知线程函数：
- `thread_uart_func` — UART 串口收发线程
- `bt_spp_thread` / `spp_receive_thread` / `spp_bluetoothd_thread` — 蓝牙 SPP 通信
- `gatt_receive_thread` / `gatt_notify_thread` / `gatt_server_thread` — BLE GATT 通信
- **`local_server_thread`** — **Unix Socket 服务端（单连接）**
- `lte_server_thread` / `thread_lte_func` — 4G 网络服务
- `signal_exit_handler` — SIGINT 信号处理（清理→退出）

### ST03_app

`ST03_app::main()` 启动 **5 个线程**（stack=512KB each）：

| 线程 | 偏移 | 职责 |
|------|------|------|
| Thread 1 | app+0x80 | 网络/通讯线程 |
| Thread 2 | app+0x88 | 消息队列接收线程 |
| Thread 3 | app+0x8C | 服务处理线程 |
| Thread 4 | app+0x94 | 后台任务线程 |
| Event | app+0x7C | `cevent_thread_proc` — UI 事件处理 |
