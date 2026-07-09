# N2 (Paperang v853) 逆向分析文档

> 基于 `~/image/zyb/N2/` rootfs + OTA固件包，2025-06-05 构建，SoC Allwinner V853 (sun8iw21p1)

---

## 一、组件总览

| 文件 | 路径 | 大小 | 说明 |
|------|------|------|------|
| **dlamInit** 🆕 | `/usr/bin/dlamInit` | 86KB | 服务管理器，fork/exec 拉起子进程 |
| **dlamPrinter** | `/usr/bin/dlamPrinter` | 412KB | 打印服务 ELF，ARMv7，~1680 函数 |
| **ST03_app** | `/usr/bin/ST03_app` | 7.3MB | 主控应用（stripped） |
| **ST03_app (dbg)** | `/data/dlam/ST03_app` | 32MB | 主控应用（含 DWARF debug_info, 5746 命名函数） |
| **Paperang_N1.bin** | `/data/res/Paperang_N1.bin` | 122KB | MCU 固件（N1版 67KB → N2版翻倍） |
| **board_test** | `/usr/bin/board_test` | 186KB | 工厂测试 |
| **libdlamMisc.so** 🆕 | `/usr/lib/dlam/` | — | 新公共库（接口 dlamMisc.h） |
| **DTB** | `ota/uboot` 内嵌 | 112KB | sun8iw21p1，UART/GPIO/SPI 配置 |

### 启动流程

```
/etc/init.d/S60ST03
  ├── insmod aic8800 (WiFi)
  ├── insmod focaltech_ts (触摸)
  ├── /etc/lte/USB_4G_LTE.sh init
  ├── dbus start
  └── dlamInit &          ← 🆕 统一服务管理器（替代 N1 独立 S60dlamPrinter 脚本）
        ├── fork/exec dlamPrinter
        ├── fork/exec ST03_app
        └── ...
```

**关键变化**: N2 比 N1 少了独立 `S60dlamPrinter` 启动脚本，所有子进程由 `dlamInit` 统一管理。

---

## 二、硬件平台

| 项 | 值 |
|----|-----|
| SoC | Allwinner V853 (sun8iw21p1) |
| CPU | ARMv7, Cortex-A7 单核 |
| 内存 | 241MB total, ~152MB available |
| 显示 | 1280×720@60, 32bpp, stride=5120 |
| 固件构建 | 2025-06-05 |
| 源码路径 | `/home/wy/v853/N2/...` (ST03_app) / `/home/sunchip/v853_N1/lzl_n1/...` (dlamPrinter) |

### DTB 信息 (sun8iw21p1, version 17, 112KB)

#### UART 分配

| 端口 | 地址 | 状态 | 引脚 | 用途 |
|------|------|------|------|------|
| uart0 | 0x2500000 | okay | PH9, PH10 | 调试 console (`console=ttyS0`) |
| uart1 | 0x2500400 | **disabled** | PG6, PG7 | 未使用 |
| uart2 | 0x2500800 | okay | PE12/13/10/11 (4线 RTS/CTS) | DTB enabled 但 N2 代码未引用 |
| uart3 | 0x2500c00 | okay | PE0, PE1, PE2 (3线) | **dlamPrinter 使用此端口** (`/dev/ttyS3`) |

> ⚠ **N1→N2 变化**: N1 打印机 UART 为 `/dev/ttyS2`，N2 切换到 `/dev/ttyS3`。

#### GPIO (sysfs 导出)

| GPIO | 方向 | 内核标签 |
|------|------|---------|
| 109 | out lo | sysfs |
| 110 | out hi | sysfs |
| 111 | out lo | sysfs |
| 112 | out lo | sysfs |
| 114 | out hi | sysfs |
| 134 | out lo | audio_switch_control |
| 135 | out lo | sysfs |
| 137 | out hi | audio_switch_detect_ |
| 142 | in lo | bt_hostwake |
| 143 | out hi | bt_wake |
| 199 | in lo | wlan_hostwake |
| 226 | out lo | usb-vbus |
| 231 | in hi | fts_irq_gpio (触摸中断) |
| 232 | out hi | fts_reset_gpio (触摸复位) |
| 236 | out lo | card-pwr-gpios |
| 237 | in lo | sysfs |
| 238 | out hi | sysfs (S60ST03 启动时额外 `echo 1`) |
| 239 | out hi | wlan_regon |

---

## 三、UART 协议（不变）

UART 帧结构：

```
[A5][ver][total_len_LE][main][sub][ack][datalen_LE][data...][CRC32][5A]
  总长 = 9 + total_len = 14 + datalen
  CRC32 计算范围: [offset+4 .. offset+4+total_len]（从 main 开始 total_len 字节）
```

- `ver` = `0x01`（dlamPrinter 发送）/ `0x00`（**MCU 响应**），MCU 状态机不校验 ver 字节
- **datalen = total_len - 5**（total_len 含 main+sub+ack+datalen字段2B+data）
- **字节序**: `total_len` **小端**（MCU 响应侧 2026-07-08 实测确认: `0x09 0x00`=9），`datalen` 小端
- CRC32 从 `main` 字段开始计算，包含 `main+sub+ack+datalen+data` 全部
- **N2 波特率**: 921600 (`cfg_get("buadrate")=6`, config index 6)
- **USB-serial**: `/dev/ttyUSB1`，921600，LTE modem 用 (`system()` 初始化 + 重试机制)

### CRC32 算法详情

- **算法**: 标准 reflected CRC32，多项式 `0xEDB88320`
- **初始值**: `0x35769521`（非标准 `0xFFFFFFFF`），对应 pre-inverted 值 `0xCA896ADE`
- **运算**: `c = ~init; loop table; return ~c`
- **校验范围**: 从 `main` 字段开始，含 `main+sub+ack+datalen+data` 全部
- MCU 固件中 crcKey 位于 flash 地址 `0x8C90`，dlamPrinter 侧同一算法

**验证通过的 MCU 响应帧**:
| 帧 | CRC32 |
|----|-------|
| (1,0x0C) -> 响应 | `0x3424CA54` |
| (5,0x0F) -> 响应 | `0xEFFEB933` |

### MCU 接收状态机 (`FUN_0000895c`)

```
state 0: 等 0xA5 → state 3
state 3: 读 ver(1B) → state 6       (不校验版本号)
state 6: 读 total_len(2B BE)        (>0x509 拒收) → state 8
state 8: 读 payload(total_len B) → state 9
state 9: 读 CRC32(4B) → state 0xB
state 0xB: 等 0x5A                  CRC 通过 → dispatch handler 表
                                    CRC 失败 → 静默丢弃，无响应
```
- **1 秒帧间超时**：任意状态超时自动复位到 state 0
- CRC 通过后 `FUN_00008790` 按 `(main, sub, ack)` 三元组查找 handler 表分派

### MCU handler 表

位于 flash `0x08021600`（文件偏移 `0x21600`=136KB），但 MCU 固件二进制文件仅 122KB，OTA 增量包不包含该表数据 → 无法从静态文件中解析。

### UART 直连 MCU

**结论: 可行**，但需要正确的串口配置。

#### 关键配置

| 参数 | 值 | 说明 |
|------|-----|------|
| 端口 | `/dev/ttyS3` | |
| 波特率 | 921600 | `cfg_get("buadrate")=6` |
| `open()` | `O_RDWR\|O_NOCTTY\|O_LARGEFILE` | 无 O_NONBLOCK，匹配 dlamPrinter |
| `CLOCAL` | **必须** | 忽略 DCD 载波检测，否则 `open()` 阻塞 |
| `CRTSCTS` | **禁止** | uart3 为 3 线（PE0/PE1/PE2），无 RTS/CTS 物理引脚，设了会卡内核驱动 |
| `CREAD` | 必须 | 使能接收器 |
| 流控 | 无 | 3 线串口无硬件流控 |

#### 已解决问题

| 之前现象 | 根因 | 修复 |
|---------|------|------|
| 无响应 / 读到 `0xFF` 浮空噪声 | `total_len` 用 BE，MCU 状态机拒收 | 改用 LE |
| 帧格式正确后仍无响应 | `open()` 缺 CLOCAL 被 DCD 卡住 | 加 `CLOCAL` |
| open 不卡但 write 卡 | `CRTSCTS` 在 3 线端口上阻塞内核 | 去掉 `CRTSCTS` |

#### 验证结果 (2026-07-08)

发送 `(1,0x07)` 版本查询，一次收到 **3 帧响应**：
```
RX: a5 01 10 00 01 07 02 0b 00 ..."01.00.17"...  ← MCU 版本 v1.0.11
RX: a5 01 09 00 05 0c 02 04 00 ...               ← (5,0x0C) ack=0x66 状态上报
RX: a5 01 09 00 05 0d 02 04 00 ...               ← (5,0x0D) ack=0x65 纸状态
```
MCU 会上电即开始周期性主动上报状态帧，不需要握手序列。

#### 串口配置要点（完整）

```c
int fd = open("/dev/ttyS3", O_RDWR | O_NOCTTY | O_LARGEFILE);
struct termios tty;
cfmakeraw(&tty);
cfsetspeed(&tty, B921600);   // 注意: B921600, 不是 921600
tty.c_cflag |= CLOCAL | CREAD;  // 必须! 无 CRTSCTS!
tty.c_cc[VMIN] = 0;
tty.c_cc[VTIME] = 10;
tcsetattr(fd, TCSANOW, &tty);
```

#### 测试工具

`dlam_mcu` (43KB, ARMv7 musl 静态编译)：直连 MCU 发送单个 UART 命令并读取响应。
```bash
adb push dlam_mcu /tmp/ && adb shell "killall dlamPrinter; /tmp/dlam_mcu"
```

---

## 四、通信架构

```
┌──────────────────────────────────────────────────────────────────┐
│                         ST03_app                                  │
│  ┌─[cURL/HTTPS]──→ 云端API（题库、VIP、音频、打印作业）            │
│  ├─[Unix Socket]─┐  /data/print.client → /data/print.server      │
│  ├─[D-Bus]───────┤  状态通知订阅                                   │
│  ├─[Msg Queue]───┤  System V IPC (ST03Handle::recv_message)       │
│  └─[LTE Socket]──┤  /data/lte.client → 4G modem                  │
└──────────────────┼───────────────────────────────────────────────┘
                   │
┌──────────────────▼───────────────────────────────────────────────┐
│                        dlamPrinter                                 │
│  local_server_thread (Unix Socket 服务端)                          │
│    ↓ recv JSON                                                    │
│  json_cmd_dispatcher (34 cmd 分发)                                 │
│    ├── cmd < 0xDC → UART (/dev/ttyS3) → MCU                       │
│    └── cmd ≥ 0xDC → USB-serial (/dev/ttyUSB1) → LTE modem         │
│                                                                   │
│  MCU ACK → handler → D-Bus system("dbus-send") + Unix Socket 回传  │
└───────────────────────────────────────────────────────────────────┘
                   │ UART /dev/ttyS3 (PE0/PE1/PE2)
┌──────────────────▼───────────────────────────────────────────────┐
│                    Paperang_N1 MCU (122KB)                        │
│  UART ISR → 协议解析 → 打印引擎 (SPI DMA)                          │
│  BLE Nordic + GPIO ADC (纸检测)                                    │
└──────────────────────────────────────────────────────────────────┘
```

### 通道汇总

| 通道 | 端点 | 协议 | N1→N2 变化 |
|------|------|------|-----------|
| **Unix Socket** | `/data/print.client` ↔ `/data/print.server` | JSON | 不变 |
| **D-Bus System** | `system("dbus-send ...")` | D-Bus | 不变 |
| **SystemV MsgQ** | `msgrcv()` | 二进制结构体 | 不变 |
| **UART** | `/dev/ttyS2`(N1)→**`/dev/ttyS3`**(N2) | 定制帧 | ⚠ 端口变化 |
| **USB-Serial** | `/dev/ttyUSB1` | AT command? | 🆕 LTE modem |
| **LTE Socket** | `/data/lte.client` | LwM2M/CoAP | 🆕 |
| **cURL/HTTPS** | 云端 API | HTTPS/JSON | 不变 |

---

## 五、JSON 命令分发 (dlamPrinter)

### 5.1 Socket 直连协议 (ST03_app → dlamPrinter)

`local_printer_send(cmd, data, type)` 构建 JSON 并通过 `/data/print.client` → `/data/print.server` 发送：

| type | JSON 格式 | 示例 |
|------|-----------|------|
| `0x10` (16) | `{"cmd": N, "data": "..."}` | 字符串参数 |
| `0x08` (8) | `{"cmd": N, "val": N}` | 单字节值 (cmd=0x42 DPI) |
| others | `{"cmd": N}` | 纯命令 (cmd=4/5/7 等) |

### 5.2 ACK 回传协议 (dlamPrinter → ST03_app)

`FUN_000578d8(ack_code, ack_byte, flag)` 构造：

```json
{"cmd": 1, "ack": <ack_code>, "type": <ack_byte>, "data": <flag>}
```

通过同一 Unix Socket 回传给 ST03_app。**打印三态 ACK 额外执行 `system("dbus-send ...")`**：
- ack=0x67: D-Bus 广播 + socket 回传（打印开始）
- ack=0x68: D-Bus 广播 + socket 回传（打印进度）
- ack=0x69: D-Bus 广播 + socket 回传（打印结束）

### 5.3 完整 JSON cmd 表 (N2 vs N1)

| cmd | JSON字段 | N1 UART | N2 UART | 变化 | 说明 |
|-----|---------|---------|---------|------|------|
| **0** | type,timeout,data | 见下 | 见下 | — | 打印图片/QR/自检 (type 子命令) |
| **1** | 无 | (5,0x23) | (5,0x37) | ⚠ **变** | 设备状态查询 |
| **2** | 无 | (5,0x24) | (5,0x24) | — | 设备状态查询 |
| **3** | 无 | (1,7) | (1,7) | — | 配置/版本查询 (ack=3) |
| **4** | 无 | (5,0x0C) | (5,0x0C) | — | 打印机状态 (ack=0x66) |
| **5** | 无 | (5,0x0D) | (5,0x0D) | — | 打印机状态 (ack=0x65) |
| **6** | 无 | (5,0x10) | (5,0x10) | — | 查询 (ack=6) |
| **7** | 无 | (5,0x17) | (5,0x17) | — | **打印自检页** (无ACK) |
| **8** | ota_url | — | — | — | OTA 升级 |
| **9** | 无 | BT reset | BLE+Spp 双reset | ⚠ **变** | `FUN_0002f27c()`+`FUN_000227e4()` |
| **0xA** | 无 | (5,0x27) | (5,0x27) | — | 查询 |
| **0xB** | val (≤2) | raw(1,0x16) | raw(1,0x16) | — | 4字节直发MCU |
| **0xC** | 无 | BT check | BLE+Spp 双check | ⚠ **变** | `FUN_000228cc()`+`FUN_0002f2f4()` |
| **0xD** 🆕 | 无 | — | 3字节直写UART | 🆕 | `FUN_00019fc8()` |
| **0xE** 🆕 | 无 | — | (1,0x33) | 🆕 | |
| **0xF** 🆕 | 无 | — | (1,0x15) | 🆕 | |
| 0x10~0x3B | — | — | — | — | 预留 (空 break) |
| **0x3C** | wifi_cfg | (1,3) | (1,3) | — | WiFi 配置 |
| **0x3D** | mac_str | (3,7) | (3,7) | — | MAC 地址 |
| **0x3E** | bt_cfg | (4,3) | (4,3) | — | 蓝牙配置 |
| **0x3F** | param | (5,0x11) | (5,0x11) | — | 单字节参数 (default 0x4B) |
| **0x40** | wifi_ssid | (1,5) | (1,5) | — | WiFi SSID |
| **0x41** | net_cfg | (1,0x18) | (1,0x18) | — | 网络配置 |
| **0x42** 🆕 | 无 | — | (5,0x35) | 🆕 | **像素宽度配置** (2寸=0x39/57, 3寸=0x4F/79) |
| 0x43 | — | — | — | — | 预留 |
| **0x44** 🆕 | 无 | — | (1,0x0E) | 🆕 | |
| 0x45~0x117 | — | — | — | — | 预留 |
| **0x118** | 无 | — | — | — | 开始打印 (printer_action 0x10) |
| **0x119** | 无 | — | — | — | 停止打印 (printer_action 0x11) |
| **0x11A** | 无 | — | — | — | printer_action 0x0A |
| **0x11B** | 无 | — | — | — | printer_action 0x0B |
| **0x11C** | 无 | — | — | — | printer_action 0x10 |
| **0x11D** | 无 | — | — | — | printer_action 0x08 |
| **0x11E** | 无 | — | — | — | printer_action 0x0B |
| **0x11F** 🆕 | 无 | — | — | 🆕 | **printer_action 0x1F** |
| **0x120** | 无 | — | — | — | printer_action 0x0A |
| **0x121** | 无 | — | — | — | printer_action 0x0C |
| **0x122** | 无 | — | — | — | printer_action 0x0D |

### 5.4 cmd=0 type 子命令（不变）

| type | 含义 | 流程 |
|------|------|------|
| 0 | 打印图片 | BMP→灰度→MMJ压缩→UART分包 [(5,0x19)→(5,0x1B)×N→(5,0x1A)→(5,0x16)] |
| 1,2 | 预留 | — |
| 3 | 直接发BMP | 仅1/4位色深，跳过灰度 |
| 4 | 打印QR码页 | → (5,0x18) |
| 5 | 打印测试页 | → (5,0x17)（同cmd=7自检） |

---

## 六、UART 命令注册表 (dlamPrinter)

`FUN_0001b250` → `register_cmd_handler(main, sub, handler, ack_dbus_code)`

### 6.1 完整注册表

| (main,sub) | handler类型 | ack | 备注 |
|------------|------------|-----|------|
| **(2,0x01)** 🆕 | specific | — | LTE/4G 配置域 |
| **(2,0x02)** 🆕 | specific | — | |
| **(2,0x03)** 🆕 | specific | — | |
| (1,0x01) | cmd_input_buf_common | — | 系统事件输入 |
| (1,0x03) | cmd_ack_with_ack_int | 0x3C | WiFi 配置 |
| (1,0x05) | cmd_ack_buf_common | 0x40 | SSID |
| (1,0x06) | cmd_input_buf_common | — | 事件输入 |
| (1,0x07) | specific | 3 | 版本号查询 |
| (1,0x08) | cmd_input_buf_common | — | 事件输入 |
| (1,0x0B) | specific | — | (与 0x0C 用同一 handler) |
| (1,0x0C) | specific | — | |
| **(1,0x0E)** 🆕 | specific | — | cmd=0x44 对应 |
| (1,0x13) | cmd_ack_buf_common | 8 | OTA |
| **(1,0x14)** 🆕 | specific | — | |
| **(1,0x15)** 🆕 | specific | — | cmd=0x0F 对应 |
| **(1,0x17)** 🆕 | cmd_ack_buf_common | 0x6B | 蓝牙信息 |
| (1,0x18) | cmd_ack_with_ack_int | 0x41 | 网络配置 |
| **(1,0x20)** 🆕 | specific | — | |
| **(1,0x33)** 🆕 | specific | — | cmd=0x0E 对应 |
| (3,0x07) | cmd_ack_with_ack_int | 0x3D | MAC |
| (4,0x03) | cmd_ack_with_ack_int | 0x3E | 蓝牙配置 |
| (5,0x01) | cmd_input_buf_common | — | 保留 |
| (5,0x02) | cmd_input_buf_common | — | 保留 |
| (5,0x04) | cmd_input_buf_common | — | 保留 |
| (5,0x05) | cmd_input_buf_common | — | **事件输入** |
| (5,0x0C) | cmd_ack_buf_common | 0x66 | 打印机状态(纸张) |
| (5,0x0D) | cmd_ack_buf_common | 0x65 | 打印机状态(硬件) |
| (5,0x0F) | cmd_input_buf_common | — | **事件输入** |
| (5,0x10) | cmd_ack_with_ack_int | 6 | 整数查询 |
| (5,0x11) | cmd_ack_with_ack_int | 0x3F | 参数查询 |
| (5,0x16) | cmd_ack_buf_common + system() | 0x69 | 打印结束 ACK |
| (5,0x19) | cmd_ack_buf_common + system() | 0x67 | 打印开始 ACK |
| (5,0x1A) | cmd_ack_buf_common + system() | 0x68 | 打印进度 ACK |
| (5,0x23) | cmd_ack_buf_common | 1 | (旧 cmd=1, 保留) |
| (5,0x24) | cmd_ack_buf_common | 2 | cmd=2 |
| (5,0x27) | specific | — | 查询 |
| **(5,0x28)** 🆕 | specific | — | |
| **(5,0x37)** 🆕 | specific | 1 | **替换 (5,0x23)** 为新 cmd=1 |

**单向命令**（无 handler 注册）: `(5,0x17)` 自检页, `(5,0x18)` QR码页, `(5,0x1B)` 打印行数据

### 6.2 电池电压命令 `(1,0x0C)` 双向流程

`cmd_ack_for_mcu_bat` (注册 handler `(1,0x0B)` 和 `(1,0x0C)` 均指向此函数) 是**双向函数**，根据调用上下文分两条路径：

#### 路径1: MCU 主动问 → dlamPrinter 读 Linux sysfs 回复

MCU 发来查询帧（ack=1, datalen=0）→ dispatch → `cmd_ack_for_mcu_bat`：

```
MCU 发来: A5 00 00 05  01 0C 01  00 00  [CRC] 5A
                        ^^  ^^^^^^^^
                       ack=1 datalen=0
```

```
cmd_ack_for_mcu_bat()
  │
  ├── FUN_0001a95c() 解析 payload 失败 (<0) → 说明 MCU 在"问"
  │
  ├── FUN_0001bc0c(param, 1, 0xC) → 匹配处理
  │     │
  │     ├── read_device_file("/sys/class/power_supply/battery/voltage_now")
  │     │      → atoi(str) → vol_mV
  │     │
  │     └── FUN_0001ec5c(1, 0xC, 2, &vol_mV, 2)
  │            → UART 回复: A5 01 00 07  01 0C 02  02 00  [vol_mV_LE] [CRC] 5A
  │                                         ^^
  │                                        ack=2 回复
```

#### 路径2: dlamPrinter 主动查 MCU → MCU 回复缓存值

dlamPrinter 发查询 → MCU 回复 → dispatch → `cmd_ack_for_mcu_bat`：

```
cmd_ack_for_mcu_bat()
  │
  ├── FUN_0001a95c() 解析 payload 成功 → 拿到数据
  │     ├── vol_mV = *(ushort*)(payload + 3)
  │     ├── printf("report bat vol: %dmv\n", vol_mV)
  │     └── 重新计算 CRC 写入帧尾（修正传输中的损坏）
```

#### 关键结论

| 谁发起 | ack 字段 |
|--------|----------|
| MCU → dlamPrinter (查询) | `ack=1` |
| dlamPrinter → MCU (主动查询) | `ack=1` |
| 回复 | `ack=2` |

**电池电压实际来自 Linux PMIC driver `/sys/class/power_supply/battery/voltage_now`**，不是 MCU 的 ADC。MCU 是"中继站"，需要时向 Linux SoC 请求。

同理 `(1,0x0B)` 走 `capacity` 文件拿百分比。

### 6.3 MCU 事件输入分发 (cmd_input_buf_common)

MCU 主动上报 tag-value-flag 三元组事件（详见 [§十三 MCU 事件上报系统](#十三mcu-事件上报系统)）：

| tag | 含义 | D-Bus ack | 状态位 |
|-----|------|-----------|--------|
| **1** | 加热/温度状态 (evtProc_HighTemp 等) | 0x6A | **bit2** |
| **2** | 硬件综合 (evtProc_Platen*/Voltage*/PowerFail 等) | 0x65 | **bit0** |
| **3** | 纸张状态 (evtProc_PaperIn/Out 等) | 0x66 | **bit1** |

```c
// 每个 tag 的结构: tag(1B) value(2B LE) flag(1B)
for each tag:
    switch (tag):
        case 1: D-Bus(0x6A, 1, flag)  // bit2 温度
        case 2: D-Bus(0x65, 1, flag)  // bit0 硬件
        case 3: D-Bus(0x66, 1, flag)  // bit1 纸张
```

---

## 七、打印机状态机 (ST03_app)

### 7.1 状态位定义

`ls_print_task` 维护一个状态字节 `*pbVar16`：

| bit | ack 来源 | 含义 | SET (异常) | CLEAR (正常) |
|-----|---------|------|-----------|-------------|
| **bit0** | 0x65 (tag=2 硬件) | 硬件综合: 开盖/电池/断电 | 异常 | OK |
| **bit1** | 0x66 (tag=3 纸张) | 纸张状态 | 缺纸 | 有纸 |
| **bit2** | 0x6A (tag=1 温度) | 加热/温度 | 过热 | 正常 |

### 7.2 就绪判定

```c
// ls_print_task 主循环
if ((status_byte & 7) == 0 && !sleep_mode) {  // 三态全0 + 唤醒
    if (!running && !heating) {
        SetPrintTipStatus(0x71);     // 显示"就绪"
    } else {
        set_print_paper_status(true); // 仅更新纸张状态
    }
}
```

### 7.3 初始化握手

ST03_app `ls_print_task` 连接后立即发送：

```
connect→  lsLink_printer_server()
  ├── local_printer_send(3, NULL, 4)    // 版本查询 → ack=3 → OTA固件检查
  ├── local_printer_send(4, NULL, 0)    // 纸张查询 → ack=0x66 → bit1
  ├── local_printer_send(5, NULL, 0)    // 硬件查询 → ack=0x65 → bit0
  └── local_printer_send(0x42, &type, 8) // 设置纸宽 (2寸=0x39, 3寸=0x4F)
```

### 7.4 主循环

```
select(10s) → recv() → cJSON_Parse → switch(ack):
  0xE6 → 状态码 (0x19-1f→4, 0x14-18→3, 0x0E-13→2, 0x09-0D→0)
  0x70 → 缺纸 → running=0 + Toast
  0x68 → 打印中 → running=0
  0x66 → bit1 更新 → 条件触发 cmd=0x118(开始打印)
  0x65 → bit0 更新
  0x6A → bit2 更新
  0x6B → 蓝牙信息
  0x6C → BT断开 → Toast + bt_disconn_app()
  0x6D → BT连接 → Toast + bt_conn_app()
  0xE0 → 停止 → +0x218=0
  0xE1 → 打印 → +0x218=1 → 发 cmd=0x119+0x11c
  0xE4 → cellular=0
  0xE5 → cellular=1
  0xEA → 设置cellular月份
  0xEB → 设置cellular当前号
  0xEC → 保存字符串到 +0x258
  0xED → 保存字符串到 +0x26d
  3    → 版本比对 → OTA触发
  100  → OTA状态
```

---

## 八、线程架构

### 8.1 dlamPrinter（6线程 + 条件线程）

| 线程 | 函数 | N1→N2 |
|------|------|--------|
| Unix Socket 服务端 | `local_server_thread` | 不变 |
| UART 收发 | `thread_uart_func` | 不变 |
| BLE GATT 服务 | `gatt_server_thread` 🆕 | 新增 |
| BLE GATT 接收 | `gatt_receive_thread` 🆕 | 新增 |
| BLE GATT 通知 | `gatt_notify_thread` 🆕 | 新增 |
| 蓝牙 SPP | `bt_spp_thread` / `spp_receive_thread` / `spp_bluetoothd_thread` | 不变 |
| LTE 服务 | `lte_server_thread` / `thread_lte_func` 🆕 | 新增 |
| 信号处理 | `signal_exit_handler` | 不变 |

### 8.2 ST03_app

| 线程 | 函数 | 职责 |
|------|------|------|
| GUI 主循环 | `main` | LVGL 渲染 |
| 打印机状态 | `ls_print_task` | select+recv 响应处理 |
| 事件监听 | `EventManager::ueventLoop` | uevent (电池/AC/USB/音频/SD) |
| 网络诊断 | `net_diagnosis_thread` | Ping 检测 |
| 文件加解密 | `file_decrypt_mp4_pthread_create` | MP4 解密 |
| 解压 | `unzip_thread` | ZIP 解压 |
| 显示 | `disp_fbio_display_async_thread` | 异步 framebuffer |
| 主线程管理 | `StMain::kill_all_thread` | 总控 |
| LwM2M | `runthreadfunc` → `cmcc_task` | 广电物联网 |

---

## 九、ST03_app 打印页面架构

### 9.1 三个打印页类

| 类 | 源文件 | 功能 |
|----|--------|------|
| `TakeTopicPrintShowPage` | TakeTopicPrintShowPage.cpp | 作业题打印（最复杂） |
| `CameraAndPhotoPrintPage` | CameraAndPhotoPrintPage.cpp | 拍照/相册打印 |
| `ReciteWordBookStudyPrintPage` | ReciteWordBookStudyPrintPage.cpp | 背单词学习打印 |

### 9.2 图片处理管线

```
原始图片
  ├── explain_gary_img()      → 灰度化
  ├── explain_ordinary_img()  → 普通处理
  ├── color_transfer_img()    → 色深转换
  ├── print_layout_c/v_dark/light_img() → 布局渲染 (组合/垂直 × 暗/亮)
  ├── printImgPack()          → 打包排版
  ├── print_disp_img()        → 预览显示
  ├── check_print_is_long()   → 长图检测
  └── print_check_up_img()    → 上传检查
```

### 9.3 画幅支持

| 尺寸 | 方法 |
|------|------|
| 2寸 | `print_2inch_img` / `print_2inchdown_img` |
| 3寸 | `print_3inch_img` / `print_3inchdown_img` |
| 小尺寸 | `print_small_inch_img` / `print_small_inchdown_img` |

### 9.4 云端通信

```
postPrintData(time, timeout, p_post, printerType)
  → zyb_post_task() → HTTPS POST → 云端打印作业API
  JSON: {"list": [{tid, tidTokenEnc, gradeId, deviceSN, deviceType}]}

postNewPrintData(time, timeout, p_post, printerType, fontSize, content, vertical)
  → 同上 + {"param": {fontSize, content, vertical, paperSize, ...}}
```

---

## 十、通信流程图

```
ST03_app                                                    dlamPrinter
  │                                                            │
  │──lsLink_printer_server()──▶ Unix Socket connect ──▶ accept │
  │                                                            │
  │──local_printer_send(cmd)──▶ {"cmd":N} ──────────▶ json_cmd_dispatcher
  │                              ◀──────────────────────        │
  │                            recv ←── {"cmd":1,"ack":0x66}   │
  │                              │                              │
  │  ls_print_task select(10s)  │                              │
  │    ├── ack=3  → OTA检查     │                              │
  │    ├── ack=0x66 → bit1更新  │                              │
  │    └── ack=0x65 → bit0更新  │                              │
  │                              │                              │
  │──local_printer_send(0x118)─▶ 开始打印 ──▶ UART (5,0x19) ──▶ MCU
  │                              ◀── ACK 0x67 ── UART ←─────── MCU
  │  recv ←── system("dbus-send 0x67")                         │
  │                              │                              │
  │  [并行] 图片处理 + 发送     │                              │
  │──postPrintData()──▶ HTTPS ──▶ 云端API                       │
  │                              │                              │
  │  [MCU 主动上报]             │                              │
  │  recv ←── ack=0x6A(tag=1) ── 温度事件                      │
  │  recv ←── ack=0x65(tag=2) ── 硬件事件                      │
  │  recv ←── ack=0x66(tag=3) ── 纸张事件                      │
```

---

## 十一、关键变化汇总 (N1→N2)

| 领域 | N1 | N2 |
|------|-----|-----|
| **启动** | `S60dlamPrinter` + `S60ST03` 两个脚本 | `S60ST03` → `dlamInit &` 统一管理 |
| **打印UART** | `/dev/ttyS2` | `/dev/ttyS3` |
| **LTE** | 无 | `/dev/ttyUSB1` + LwM2M/CoAP |
| **BLE GATT** | 无独立线程 | 3个线程 (server/receive/notify) |
| **cmd=1** | (5,0x23) | (5,0x37) |
| **cmd=9/0xC** | BT only | BLE GATT + SPP 双通道 |
| **cmd=0x42** | 无 | (5,0x35) DPI/纸宽配置 |
| **main=2 域** | 不存在 | (2,1)(2,2)(2,3) LTE配置 |
| **新增 cmd** | — | 0xD/0xE/0xF/0x42/0x44/0x11F |
| **MCU固件** | 67KB | 122KB (+82%), 新增 35 事件回调 + RFID/里程认证 |
| **dtb** | N/A | sun8iw21p1 112KB |
| **libdlamMisc.so** | 无 | 新公共库 |

---

## 十二、百度网盘 SDK 安全分析

> ⚠ 本节涉及已确认的凭据泄露漏洞。以下 AppKey/SecretKey 已通过百度 OAuth Device Flow 验证有效。

### 12.1 硬编码凭据

| 凭据 | 值 | 用途 |
|------|-----|------|
| Baidu AppKey | `XWCEf4OqLnwjmYvEZZB4NLHQYPPoelD4` | 百度开放平台 OAuth 客户端标识 |
| Baidu SecretKey | `UeE4tDf6JthP2XWDcK8KznJaweUYBxqu` | 百度开放平台 OAuth 客户端密钥 |
| 喵宝 AppKey | `mmdf921cb1e4e53be3` | 喵宝自有 API 认证 |
| 喵宝 AppSecret | `80cc5ea133e4d544c05fe4a92f633d79` | 喵宝自有 API 认证 |

所有凭据硬编码在 ST03_app (stripped) 和 ST03_app (debug) 的 `.rodata` 段中，`strings` 即可提取。

### 12.2 OAuth Device Flow

```
Step 1: device_code_get()
  POST https://openapi.baidu.com/oauth/2.0/device/code
       ?response_type=device_code
       &client_id={AppKey}
       &scope=basic,netdisk
  ← {device_code, user_code, qrcode_url, interval}

Step 2: qr_get(qrcode_url)
  下载二维码 → /data/qrcode.bmp → 屏幕展示
  用户用百度APP扫码授权（域名 openapi.baidu.com，完全合法）

Step 3: access_token_get(device_code) — 每 interval(5s) 轮询
  POST https://openapi.baidu.com/oauth/2.0/token
       ?grant_type=device_token
       &code={device_code}
       &client_id={AppKey}
       &client_secret={SecretKey}
  ← {access_token, refresh_token, expires_in=2592000}

Step 4: refresh_token_get() — 无限续期
  POST https://openapi.baidu.com/oauth/2.0/token
       ?grant_type=refresh_token
       &refresh_token={refresh_token}
       &client_id={AppKey}
       &client_secret={SecretKey}
  ← {新 access_token, 新 refresh_token}
```

### 12.3 API 端点

授权后可调用百度网盘开放 API (`scope=basic,netdisk`)：

| 端点 | 方法 | 功能 |
|------|------|------|
| `/rest/2.0/xpan/file?method=list` | GET | 遍历文件目录 |
| `/rest/2.0/xpan/file?method=streaming` | GET | 视频/音频流媒体播放 |
| `/rest/2.0/xpan/multimedia?method=listall` | GET | 递归列出所有文件 |
| `/rest/2.0/xpan/multimedia?method=filemetas` | GET | 批量获取文件元信息(含下载链接) |
| `/rest/2.0/xpan/multimedia?method=categorylist` | GET | 按分类(图片/视频/文档)筛选 |
| `/rest/2.0/xpan/nas?method=uinfo` | GET | 用户信息(昵称/头像/VIP/uk) |

### 12.4 本地存储

| 文件 | 路径 | 内容 | 权限 |
|------|------|------|------|
| 设备码 | `/mnt/UDISK/data/webdisk/device_code_info.txt` | device_code + user_code | `rw-rw-rw-` |
| Token | `/mnt/UDISK/data/webdisk/baidu_token_info.txt` | access_token + refresh_token | `rw-rw-rw-` |
| 用户信息 | `/mnt/UDISK/data/webdisk/baidu_user_info.txt` | 百度用户资料 | `rw-rw-rw-` |
| 二维码 | `net_user.bmp` / `net_user2.bmp` | OAuth 二维码缓存 | — |
| 媒体缓存 | `play_video.m3u8` / `play_audio.m3u8` | HLS 播放列表 | — |

所有敏感文件存储在可读写分区，**明文、世界可读**。

### 12.5 攻击面

| 攻击向量 | 难度 | 影响 |
|---------|------|------|
| **固件提取凭据** | 低 (strings 即可) | 任意人可注册为"合法"百度应用客户端 |
| **钓鱼扫码** | 低 (OAuth Device Flow 标准流程) | 用户在百度官方域名下授权，无法辨别真伪 |
| **物理设备 token 提取** | 低 (明文/世界可读) | 拿到设备=拿到网盘全量访问 |
| **refresh_token 永久维持** | 低 (无需用户交互) | 每30天自动续期，无吊销入口 |
| **scope=netdisk 全量读写** | — | 文件列表/下载/流播/元信息 |

### 12.6 修复建议

1. **AppKey/SecretKey 不应硬编码**：改用服务端代理模式，设备持有短期 token，凭据存于云端
2. **token 加密存储**：至少用 dm-crypt 或 Android Keystore 等效方案
3. **增加设备指纹校验**：OAuth 请求附带设备唯一标识，服务端验证
4. **提供授权吊销入口**：设备端"解绑百度网盘"功能，调用百度 `/oauth/2.0/revoke`
5. **access_token 有效期缩短**：30 天过长，建议 1-7 天 + 滑动刷新
6. **scope 最小化**：`basic,netdisk` 权限过大，应按需申请子目录或只读权限

---

## 十三、MCU 事件上报系统

> 基于 N2 MCU 固件 (122KB, Cortex-M0+) 反汇编分析

### 13.1 事件帧格式

MCU 通过 `(5, 0x05)` 或 `(5, 0x0F)` 主动上报事件。payload 由多个 4 字节 tag 元组拼接：

```
[tag(1B)][value(2B LE)][flag(1B)] × N
```

### 13.2 tag → D-Bus ack 映射

dlamPrinter 的 `cmd_input_buf_common` 分发：

| tag | ack | 含义 |
|-----|-----|------|
| 1 | 0x6A | 温度 (ST03_app bit2) |
| 2 | 0x65 | 硬件综合 (ST03_app bit0) |
| 3 | 0x66 | 纸张 (ST03_app bit1) |

### 13.3 实测帧例

```
MCU → dlamPrinter:
A5 00 11 00  05 0F 01  0C 00  01 01 00 00  02 01 00 00  03 01 00 01  [CRC] 5A
                 ^^^^  ^^  ^^  ^^^^^^^^^^^  ^^^^^^^^^^^  ^^^^^^^^^^^
                 m=5   len tag val flg      tag val flg  tag val flg
                 s=0F  12  1   1   0        2   1   0    3   1   1
                  ack=1
```

解析: tag=1(温度) value=1 flag=0 → 正常; tag=2(硬件) value=1 flag=0 → 正常; tag=3(纸张) value=1 flag=1 → **缺纸**

### 13.4 evtProc 事件表

MCU 固件中 35 个 `evtProc_*` 事件回调，按职能分组：

| 事件 | tag | 说明 |
|------|-----|------|
| `evtProc_PaperOut` | 3 | 缺纸 (flag=1) |
| `evtProc_PaperIn` | 3 | 有纸 (flag=0) |
| `evtProc_MarkSuccess` | 3 | 黑标检测成功 |
| `evtProc_MarkError` | 3 | 黑标检测失败 |
| `evtProc_PlatenOut` | 2 | 开盖 |
| `evtProc_PlatenIn` | 2 | 合盖 |
| `evtProc_HighVoltage` | 2 | 高压异常 |
| `evtProc_LowVoltage` | 2 | 低压异常 (电池低电) |
| `evtProc_VoltageOK` | 2 | 电压正常 |
| `evtProc_PowerFail` | 2 | 断电 |
| `evtProc_ChargeStart` | 2 | 开始充电 |
| `evtProc_ChargeStop` | 2 | 停止充电 |
| `evtProc_CounterUpdate` | 2 | 计数器更新 |
| `evtProc_WriteOperateHours` | 2 | 写入操作时长 |
| `evtProc_HighTemp` | 1 | 过热 |
| `evtProc_NotHighTemp` | 1 | 降温正常 |
| `evtProc_NotHighTempStart` | 1 | 开始降温 |
| `evtProc_BTBLEConected` | — | 蓝牙 BLE 连接 (不走 UART 事件) |
| `evtProc_BTBLEDisconected` | — | 蓝牙 BLE 断开 |
| `evtProc_BTSPPConected` | — | 蓝牙 SPP 连接 |
| `evtProc_BTSPPDisconected` | — | 蓝牙 SPP 断开 |
| `evtProc_FeedKeyDown/Up/Hold*` | — | 走纸键按钮事件 (本地处理) |
| `evtProc_PrinterSleep` | — | 休眠 |
| `evtProc_FirmwareUpdata` | — | 固件升级 |
| `evtProc_Aging` | — | 老化测试模式 |
| `evtProc_FeedMeasurement` | — | 走纸测量 |
| `evtProc_LearnFail/Success` | — | 纸张类型学习 |
| `evtProc_RfidAuthSucces` | — | RFID 耗材认证成功 |
| `evtProc_AuthMileageEnd/Ready` | — | 授权里程到期/就绪 |

### 13.5 value/flag 编码

`value` 字段 (2B LE) 取决于 `event->flag_field`:

| flag_field | value |
|-----------|-------|
| 0 | 1 |
| ≠0 | 2 |

`flag` 字段: **含义因事件类型而异**（非全局 0/1 异常位）

#### 实测 flag 对照表 (2026-07-08 UART 直连验证)

| Tag | 事件 | flag=0 | flag=1 | 验证方式 |
|-----|------|--------|--------|---------|
| 2 | 盖子 (platen) | 开盖 | **关盖** | `5 0c 1` 直接查询 |
| 3 | 纸张 (paper) | **缺纸** | 有纸 | `5 0d 1` 直接查询 |
| 1 | 温度 (temp) | 正常 | **过热** | 文档/代码分析 |

> 注意: tag 1 (温度) 和 tag 2/3 (盖子/纸) 的 flag 极性相反。温度 1=异常，盖/纸 1=正常/闭合/有。

### 13.6 串行化 switch 表

事件序列化函数 `FUN_00005b34` 按 event_type 分派：

| event_type | 目标 | 方式 |
|-----------|------|------|
| 0x01 | "false" @0x5a24 | 字符串 (PaperOut) |
| 0x02 | "true" @0x5a2c | 字符串 (PaperIn) |
| 0x04 | "null" @0x5574 | 字符串 (PlatenOut) |
| 0x08 | 0x5394 | 时间/运算处理 |
| 0x10 | 0x55FC | 3字节小缓冲 |
| 0x20 | 0x5E84 | 开 JSON 数组 `[` (复合事件) |
| 0x40 | 0x59D8 | 递归继续 |

---

## 十四、最终验证结果 (2026-07-08)

### 14.1 UART 直连 MCU：成功

跨过 dlamPrinter/ST03_app/dlamInit，从 SoC 直接通过 `/dev/ttyS3` 与 MCU 通信。

**三个关键修复**：

| # | 问题 | 根因 | 修复 |
|---|------|------|------|
| 1 | MCU 无响应 | `total_len` 使用 Big-Endian，MCU 状态机 `> 0x509` 拒收 | 改用 Little-Endian |
| 2 | `open()` 永久阻塞 | 缺 `CLOCAL`，内核等 DCD 载波信号 | 添加 `CLOCAL` |
| 3 | `open`/`write` 成功但无数据 | `CRTSCTS` 在 3 线串口(uart3: PE0/PE1/PE2)上阻塞内核驱动 | 去除 `CRTSCTS` |

**正确打开方式**：
```c
int fd = open("/dev/ttyS3", O_RDWR | O_NOCTTY | O_LARGEFILE);
struct termios tty;
cfmakeraw(&tty);
cfsetspeed(&tty, 921600);
tty.c_cflag |= CLOCAL | CREAD;  // 必须! 无 CRTSCTS!
tty.c_cc[VMIN] = 0;
tty.c_cc[VTIME] = 5;
tcsetattr(fd, TCSANOW, &tty);
```

### 14.2 MCU 协议行为确认

**帧结构**：
```
[A5][ver][total_len_LE][main][sub][ack][datalen_LE][data][CRC32_LE][5A]
```

- `ver`: dlamPrinter 发 0x01，MCU 回 0x00 (active report) 或 0x01 (reply)
- `total_len = 5 + datalen`
- CRC32: poly=0xEDB88320, init=0x35769521, reflected

**ack 字段协议**：
- `ack=1`: 查询/主动上报
- `ack=2`: 回复

**MCU 行为特性**：
- 上电/UART首次活动时自动 dump 5 帧 boot 数据（版本、纸状态、纸宽等）
- 有 **1 秒帧间超时**，超时后状态机复位
- CRC 校验失败 **静默丢弃**，不发 NAK
- 版本号字节不校验，直接存储后继续

### 14.3 已验证命令

所有命令通过 `dlam_mcu` 工具直接发送并收到 MCU 回复：

| (main,sub) | 用途 | MCU 响应数据 | CRC |
|------------|------|-------------|-----|
| `(1,0x07)` | 版本查询 | `01.00.17` (v1.0.11) | 2ea4bc15 |
| `(5,0x0C)` | 打印机状态(纸) | `01 01 00 01` | 10a3752f |
| `(5,0x0D)` | 硬件状态 | `01 01 00 01` | dc0975b1 |
| `(5,0x10)` | 整数查询 | `01 01 00 5f` | b7642db8 |
| `(5,0x11)` | 参数查询 | `01 00 00` | db5715e2 |
| `(5,0x27)` | 综合事件 | 15B 三元组 (tag-value-flag) | 571798e9 |
| `(5,0x37)` | N2 状态查询 | `01 02 00 ba 00` | 6654fc87 |
| `(1,0x0E)` | N2 新增命令 | `01 02 00 36 01` | 0d1ccda5 |

### 14.4 自检页打印验证

发送 `(5,0x17) ack=1` → MCU 回复 ACK → 打印机物理出纸。

完整闭环确认：**SoC UART → MCU 协议栈 → SPI 打印头 → 热敏出纸**。

### 14.5 通信架构（修正后）

```
┌──────────────────────────────────────────────────────────────┐
│                        ST03_app                               │
│  ├─ HTTPS → 云端API (题库/VIP/打印作业)                        │
│  ├─ Unix Socket /data/print.server → dlamPrinter               │
│  └─ System V MsgQ (内部 IPC)                                   │
└────────────────────────┬─────────────────────────────────────┘
                         │ JSON {"cmd":N,...}
┌────────────────────────▼─────────────────────────────────────┐
│                      dlamPrinter                               │
│  ├─ Unix Socket 服务端                                         │
│  ├─ MCU 命令代理 (34 cmd → UART 分包)                           │
│  ├─ GDBus → BlueZ (蓝牙 A2DP)                                  │
│  └─ UART /dev/ttyS3 → MCU                                      │
└────────────────────────┬─────────────────────────────────────┘
                         │ 921600 8N1, 3-wire, NO flow control
┌────────────────────────▼─────────────────────────────────────┐
│                   Paperang_N1 MCU (122KB)                      │
│  ├─ UART 帧协议 (自定义 CRC32)                                  │
│  ├─ 打印引擎 (SPI DMA, 加热控制)                                │
│  ├─ BLE Nordic                                                 │
│  └─ 传感器 (纸检测/温度/开盖)                                   │
└──────────────────────────────────────────────────────────────┘
```

> **重要修正**: 打印机状态播报不走 D-Bus，走 Unix Socket JSON (`{"cmd":100,"ack":N,...}`)。D-Bus 仅用于蓝牙 BlueZ 集成。

### 14.6 工具

| 工具 | 大小 | 功能 |
|------|------|------|
| `dlam_mcu` | 90KB | UART 直连 MCU，发命令+收响应+10s 监听 MCU 查询并自动回复电池电压 |
| `dlam_listen` | 89KB | 纯监听模式，接收 MCU 查询并回复 |

用法：
```bash
# 默认版本查询
adb shell "killall dlamPrinter; /tmp/dlam_mcu"
# 指定命令
adb shell "killall dlamPrinter; /tmp/dlam_mcu 5 0x27 1"
# 带数据
adb shell "killall dlamPrinter; /tmp/dlam_mcu 5 0x1B 1 01020304"
```

ARMv7 musl 静态编译，`/usr/x-tools/arm-unknown-linux-musleabihf/bin/arm-unknown-linux-musleabihf-gcc -static -O2`。

### 14.7 未完成

- MMJ 图像压缩算法 (`libMb.so::mbImg2GrayscaleData`)
- 完整打印管线 (BMP→灰度→MMJ→分包→(5,0x1B)×N)
- MCU handler 表完整解码 (flash 0x08021600, 文件无此段)
| 0x80 | memcpy [struct+16] | 带 payload 拷贝 |
