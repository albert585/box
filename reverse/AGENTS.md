# AGENTS.md — 喵宝/Paperang 逆向分析项目

## 项目概述
逆向分析 dlamPrinter 打印机服务与 ST03_app、Paperang_N1 MCU 固件、board_test 之间的通信协议和命令体系。涵盖 N1、N2 两个硬件版本。

## 关键文件

| 文件 | 路径 | 描述 |
|------|------|------|
| dlamPrinter (N1) | `/home/albert/Downloads/dlamPrinter` | 核心打印机后台服务 ELF，ARMv7，1653 函数 |
| ST03_app (N1) | `/home/albert/Downloads/ST03_app` | 主控应用 ELF，ARMv7，4405 函数 |
| Paperang_N1.bin (N1) | `/mnt/data/res/Paperang_N1.bin` | N1 MCU 固件，Cortex-M0+，~67KB，255 函数 |
| board_test (N1) | `/mnt/usr/bin/board_test` | 工厂板级测试程序，ARMv7，548 函数 |
| dlamPrinter (N2) | `/tmp/n2_rootfs/usr/bin/dlamPrinter` | N2 打印服务，412KB，~1680 函数 |
| ST03_app (N2 dbg) | `/tmp/n2_rootfs/data/dlam/ST03_app` | N2 主控应用(含DWARF)，32MB，5746 命名函数 |
| Paperang_N1.bin (N2) | `/tmp/n2_rootfs/data/res/Paperang_N1.bin` | N2 MCU 固件，122KB |
| board_test (N2) | `/tmp/n2_rootfs/usr/bin/board_test` | N2 工厂测试，186KB |
| dlamInit (N2) | `/tmp/n2_rootfs/usr/bin/dlamInit` | N2 服务管理器，86KB |
| DTB (N2) | `/tmp/n2_sun8iw21.dtb` | Allwinner V853，112KB |
| 分析文档 | `/home/albert/Projects/reverse/dlamPrinter_cmd_analysis.md` | N1 完整协议分析 |
| 分析文档 | `/home/albert/Projects/reverse/N2_analysis.md` | **N2 完整分析（含命令表、状态机、线程架构）** |

### N2 rootfs 挂载
```bash
sudo mount -o loop,ro /home/albert/image/zyb/N2/rootfs /tmp/n2_rootfs
```

### N2 OTA 固件
`/home/albert/image/zyb/N2/UDISK/ota/` — 含 uboot(TOC1含dtb)、kernel、rootfs 等

## 架构约定

- **导入架构**：ARMv7 Linux ELF 用默认自动检测；Cortex-M0+ 用 `ARM:LE:32:Cortex`
- **重命名策略**：基于 printf 日志字符串推断函数功能命名
- **文档格式**：Markdown，含表、代码块、Mermaid 流程图
- **Ghidra MCP**：通过 `ghidra_assist_*` 工具进行动态反编译分析

## 核心协议

### UART 帧结构（最终确认）
```
[A5][ver][total_len_LE][main][sub][ack][datalen_LE][data...][CRC32][5A]
  总长 = 9 + total_len = 14 + datalen
  CRC32 计算范围: [offset+4 .. offset+4+total_len]（从 main 开始 total_len 字节）
```
- `ver` = `0x01`（dlamPrinter 发送）/ `0x00`（MCU 响应）
- **datalen = total_len - 5**（total_len 含 main+sub+ack+datalen字段2B+data）
- **字节序**：`total_len` **小端**（MCU 响应侧实测确认），`datalen` **小端**
- CRC32 从 `main` 字段开始计算，包含 `main+sub+ack+datalen+data` 全部（不含 ver 和 total_len）

### (main, sub) 两级命令编码
- main=1：系统配置（WiFi、网络）
- main=2：LTE/4G 配置 (N2 新增)
- main=3：MAC 地址
- main=4：蓝牙配置
- main=5：打印引擎

### JSON 命令分发（34 条 cmd）
dlamPrinter 通过 Unix Socket 接收 JSON，`json_cmd_dispatcher` 解析 `cmd` 字段分发：

| cmd | 含义 | UART (m,s) |
|-----|------|------------|
| 0 | 初始化/打印（type子命令） | 见下 |
| 7 | **打印自检页** | (5,0x17) |
| 0x118 | 开始打印 | — (printer_action(10)) |
| 0x119 | 停止打印 | — (printer_action(11)) |
| ... | (完整表见分析文档) | |

### cmd=0 type 子命令
| type | 含义 | 流程 |
|------|------|------|
| 0 | 打印图片 | BMP→灰度→MMJ压缩→UART分包 [(5,0x19)→(5,0x1B)×N→(5,0x1A)→(5,0x16)] |
| 1,2 | 预留 | — |
| 3 | 直接发BMP | 仅1/4位色深，跳过灰度 |
| 4 | 打印QR码页 | → (5,0x18) |
| 5 | 打印测试页 | → (5,0x17)（同cmd=7自检） |

### print_action 直接控制码（不经 UART 帧封装）
- `0x10`：开始打印
- `0x11`：停止打印
- 等… 直接 `write(fd, data, len)`

## 通信通道

| 通道 | 用途 | 说明 |
|------|------|------|
| **Unix Socket** | 主控连接 | `/tmp/dlamPrinter`，单连接阻塞模型，JSON 协议 |
| **D-Bus** | 系统广播 | 状态变更通知、多客户端共享 |
| **UART** | MCU 通信 | `/dev/ttyS2`(N1)→`/dev/ttyS3`(N2)，9600/19200 baud，定制帧协议 |
| **System V 消息队列** | ST03_app 内部 IPC | 16 个队列（ftok+msgget） |
| **cURL/HTTPS** | 云通讯 | 音频下载、VIP验证、消息推送 |
| **UDP 广播** | 设备发现 | 5×1s sendto 广播 |

## 已完成的重命名

### dlamPrinter
- `printer_process_image_file` — BMP 文件处理入口
- `printer_send_bmp_data` — 直接发送 BMP 数据
- `parse_uart_packet_payload` — 解析 UART 帧 payload
- `parse_uart_packet_ack_payload` — 解析 ACK 帧 payload
- `build_and_send_uart_packet` — 构建并发送 UART 帧
- `register_cmd_handler` — 注册 (main,sub)→handler 映射
- `validate_uart_cmd_match` — 校验响应帧 (main,sub) 匹配
- `get_printer_temp` — 读取打印机温度
- `read_device_file` — 读取 /sys/class 设备文件
- `bmp_get_color_depth` — 获取 BMP 色深
- `convert_rgb_to_gray` — RGB→灰度转换
- `convert_32bit_to_gray` — 32位→灰度转换
- `signal_exit_handler` — 信号退出处理
- `printer_shutdown_cleanup` — 清理 fd/进程（原名 network_shutdown_cleanup）

### ST03_app
- `udp_build_packet` — 构建 UDP 发现包
- `udp_parse_send_target` — 解析发送目标
- `udp_log_error_msg` — 记录错误日志
- `udp_broadcast_discovery` — 广播设备发现
- `device_mgr_set_app` — 关联 app 上下文
- `uevent_check_net_state` — 检查网络状态

## MCU 固件要点（Paperang_N1.bin）
- Cortex-M0+，构建日期 Jul 17 2023
- 双通道发送：UART(256B缓冲) + BLE(8KB DMA缓冲)
- 纸检测 GPAD（GPIO ADC）
- SPI DMA 驱动打印头
- 纸类型：Black mark / Label / Receipt
- BLE Nordic、WiFi MAC 信息编码在固件中

## 分析约定
- cmd 值使用 hex 的原因是源码按功能块分组（0x00~0x0C 基础，0x3C~0x41 网络，0x118~0x122 打印控制），非协议要求
- `printer_action()` 不走 UART 帧封装，直接 `write(fd)` 发送私有控制码
- `(5,0x17)` 和 `(5,0x18)` 无注册 ACK handler，为单向命令
- Paperang_N1.bin 需用 `ARM:LE:32:Cortex` 正确导入
- N2 打印机 UART 端口在 `/dev/ttyS3`，N1 为 `/dev/ttyS2`
- N2 打印机波特率 **921600** (config index 6)，N1 为 9600
- **UART 配置要点**: uart3 为 3 线（PE0/PE1/PE2），必须设 `CLOCAL`|`CREAD` 但 **不能设 CRTSCTS**（无 RTS/CTS 引脚，设了卡内核）
- N2 新增 main=2 (LTE/4G) 域 + `/dev/ttyUSB1` USB-serial LTE modem
- N2 dlamPrinter 源码仍标为 `v853_N1/lzl_n1`，ST03_app 源码为 `v853/N2`
- N2 DTB 嵌入在 `ota/uboot` (TOC1 Boot Image) 中，用 `dtc -I dtb -O dts` 反编译
- 工具链: `/usr/x-tools/arm-unknown-linux-musleabihf/bin/`
