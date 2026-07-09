#!/usr/bin/env python3
"""
dlamPrinter Unix Socket 直连客户端
协议: JSON 文本流, accept→recv→dispatch→close 单连接模型
"""

import socket
import json
import os
import time
import struct

# ---- 配置 ----
SOCKET_PATH = "/data/print.server"   # N2 dlamPrinter 服务端 socket
CLIENT_PATH = "/data/print.client"   # ST03_app 用的客户端路径 (N2)
# SOCKET_PATH = "/tmp/dlamPrinter"   # N1 路径

# ---- JSON cmd 常量 ----
CMD_PRINT_IMAGE    = 0       # type=0: 打印 BMP 图片
CMD_STATUS_1       = 1       # (5,0x37) N2 / (5,0x23) N1
CMD_STATUS_2       = 2       # (5,0x24)
CMD_GET_VERSION    = 3       # (1,0x07) 版本/SN 查询
CMD_PRINTER_STATUS = 4       # (5,0x0C) 打印机状态(纸张) → ack=0x66
CMD_DEVICE_STATUS  = 5       # (5,0x0D) 设备硬件状态 → ack=0x65
CMD_STATUS_6       = 6       # (5,0x10)
CMD_SELF_TEST      = 7       # (5,0x17) 打印自检页
CMD_OTA            = 8       # OTA 升级
CMD_RESET_BT       = 9       # BLE+SPP 双复位
CMD_STATUS_A       = 0xA     # (5,0x27)
CMD_RAW_MCU        = 0xB     # 直发 4 字节到 MCU
CMD_BT_STATUS      = 0xC     # BLE+SPP 双检查
CMD_RAW_UART       = 0xD     # 直写 3 字节到 UART
CMD_WIFI_CFG       = 0x3C    # WiFi 配置
CMD_MAC_ADDR       = 0x3D    # MAC 地址
CMD_BT_CFG         = 0x3E    # 蓝牙配置
CMD_PARAM          = 0x3F    # 单字节参数
CMD_WIFI_SSID      = 0x40    # WiFi SSID
CMD_NET_CFG        = 0x41    # 网络配置
CMD_DPI_WIDTH      = 0x42    # 像素宽度配置 (2寸=0x39, 3寸=0x4F)
CMD_START_PRINT    = 0x118   # 开始打印
CMD_STOP_PRINT     = 0x119   # 停止打印
CMD_PRINT_ACTION   = 0x11A   # printer_action

# ACK 码含义
ACK_NAMES = {
    1: "设备状态1", 2: "设备状态2", 3: "版本号",
    6: "整数查询",
    8: "OTA状态",
    0x3C: "WiFi配置", 0x3D: "MAC", 0x3E: "BT配置", 0x3F: "参数",
    0x40: "SSID", 0x41: "网络配置",
    0x65: "设备硬件(bit0)", 0x66: "纸张状态(bit1)", 0x6A: "温度(bit2)",
    0x67: "打印开始ACK", 0x68: "打印进度ACK", 0x69: "打印结束ACK",
    0x6B: "蓝牙信息",
    0x6C: "BT断开", 0x6D: "BT连接",
    0x70: "缺纸警告",
    0xDC: "开始打印结果", 0xDD: "开始打印失败",
    0xDE: "停止打印OK", 0xDF: "停止打印失败",
    0xE0: "停止", 0xE1: "打印中",
}

CMD_NAMES = {
    0: "打印图片", 1: "状态1", 2: "状态2", 3: "版本查询",
    4: "打印机状态", 5: "设备状态", 6: "查询6", 7: "自检页",
    8: "OTA", 9: "BT复位", 0xA: "查询A", 0xB: "直发MCU",
    0xC: "BT状态", 0xD: "直写UART",
    0x3C: "WiFi配置", 0x3D: "MAC", 0x3E: "BT配置", 0x3F: "参数",
    0x40: "SSID", 0x41: "网络配置", 0x42: "DPI/纸宽",
    0x118: "开始打印", 0x119: "停止打印", 0x11A: "动作",
}


class DlamClient:
    def __init__(self, server_path=SOCKET_PATH):
        self.server_path = server_path
        self.sock = None

    def connect(self):
        """通过 Unix Socket connect 到 dlamPrinter"""
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.settimeout(5)
        try:
            self.sock.connect(self.server_path)
            print(f"[+] connected to {self.server_path}")
            return True
        except FileNotFoundError:
            print(f"[-] socket {self.server_path} not found, is dlamPrinter running?")
            return False
        except ConnectionRefusedError:
            print(f"[-] connection refused on {self.server_path}")
            return False
        except socket.timeout:
            print("[-] connection timeout")
            return False

    def send_cmd(self, cmd, data=None, val=None, timeout=5):
        """
        发送 JSON 命令并接收响应
        cmd: 命令号 (int)
        data: 字符串数据 (用于 type=0x10)
        val:  单字节值 (用于 type=0x08)
        """
        if self.sock is None:
            if not self.connect():
                return None

        # 构造 JSON
        obj = {"cmd": cmd}
        if data is not None:
            obj["data"] = data
        if val is not None:
            obj["val"] = val

        json_str = json.dumps(obj)
        cmd_name = CMD_NAMES.get(cmd, f"未知(0x{cmd:X})")
        print(f"\n→ [{cmd_name}] {json_str}")

        try:
            self.sock.sendall(json_str.encode())
        except (BrokenPipeError, ConnectionResetError) as e:
            print(f"[-] send failed: {e}, reconnecting...")
            self.close()
            time.sleep(0.5)
            if not self.connect():
                return None
            self.sock.sendall(json_str.encode())

        # 接收响应
        self.sock.settimeout(timeout)
        buf = b""
        try:
            while True:
                chunk = self.sock.recv(4096)
                if not chunk:
                    break
                buf += chunk
                # 尝试解析完整 JSON
                try:
                    responses = self._extract_json(buf)
                    if responses:
                        break
                except ValueError:
                    continue
        except socket.timeout:
            if not buf:
                print("[-] no response (timeout)")
                return None

        results = []
        for js in self._extract_json(buf):
            try:
                obj = json.loads(js)
                results.append(obj)
                self._print_response(obj)
            except json.JSONDecodeError:
                print(f"[!] invalid JSON: {js[:100]}")

        return results if results else None

    def _extract_json(self, data):
        """从流中按 { } 配对提取完整 JSON 对象"""
        results = []
        for line in data.split(b'\n'):
            line = line.strip()
            if not line:
                continue
            try:
                json.loads(line)
                results.append(line.decode())
            except (json.JSONDecodeError, UnicodeDecodeError):
                # 尝试括号配对分割
                depth = 0
                start = -1
                s = line.decode(errors='ignore')
                for i, ch in enumerate(s):
                    if ch == '{':
                        if depth == 0:
                            start = i
                        depth += 1
                    elif ch == '}':
                        depth -= 1
                        if depth == 0 and start >= 0:
                            candidate = s[start:i+1]
                            try:
                                json.loads(candidate)
                                results.append(candidate)
                            except json.JSONDecodeError:
                                pass
                            start = -1
        return results

    def _print_response(self, obj):
        """格式化打印响应"""
        ack = obj.get("ack", "?")
        ack_name = ACK_NAMES.get(ack, "")
        cmd_val = obj.get("cmd", "?")
        
        parts = [f"← [cmd={cmd_val}, ack={ack}"]
        if ack_name:
            parts.append(f"({ack_name})")
        parts.append("]")
        
        if "type" in obj:
            parts.append(f" type={obj['type']}")
        if "data" in obj:
            parts.append(f" data={obj['data']}")
        
        print(" ".join(parts))

    def close(self):
        if self.sock:
            try:
                self.sock.close()
            except:
                pass
            self.sock = None

    # ---- 便利方法 ----

    def get_version(self):
        """查询版本号 (cmd=3)"""
        return self.send_cmd(CMD_GET_VERSION)

    def query_printer_status(self):
        """查询打印机纸张状态 (cmd=4 → ack=0x66)"""
        return self.send_cmd(CMD_PRINTER_STATUS)

    def query_device_status(self):
        """查询设备硬件状态 (cmd=5 → ack=0x65)"""
        return self.send_cmd(CMD_DEVICE_STATUS)

    def self_test(self):
        """打印自检页 (cmd=7) — 单向命令, 无 ACK"""
        return self.send_cmd(CMD_SELF_TEST, timeout=2)

    def set_dpi_width(self, inch_2=True):
        """
        设置纸宽 (cmd=0x42)
        inch_2=True → 2寸 (0x39), False → 3寸 (0x4F)
        """
        val = 0x39 if inch_2 else 0x4F
        return self.send_cmd(CMD_DPI_WIDTH, val=val, timeout=2)

    def set_wifi(self, ssid, password):
        """设置 WiFi (cmd=0x3C)"""
        return self.send_cmd(CMD_WIFI_CFG, data=f"{ssid},{password}")

    def start_print(self):
        """开始打印 (cmd=0x118)"""
        return self.send_cmd(CMD_START_PRINT)

    def stop_print(self):
        """停止打印 (cmd=0x119)"""
        return self.send_cmd(CMD_STOP_PRINT)

    def reset_bt(self):
        """复位蓝牙 (cmd=9)"""
        return self.send_cmd(CMD_RESET_BT)

    def status_all(self):
        """查询所有状态"""
        results = {}
        for cmd in [CMD_GET_VERSION, CMD_PRINTER_STATUS, CMD_DEVICE_STATUS, CMD_STATUS_1]:
            r = self.send_cmd(cmd)
            if r:
                results[cmd] = r
            time.sleep(0.1)
        return results


# ---- demo ----
if __name__ == "__main__":
    import sys

    # 允许命令行指定 socket 路径
    path = sys.argv[1] if len(sys.argv) > 1 else SOCKET_PATH
    cli = DlamClient(path)

    try:
        if not cli.connect():
            print("Tip: 确保 dlamPrinter 正在运行, sock path 正确")
            print(f"  N1: /tmp/dlamPrinter")
            print(f"  N2: /data/print.server")
            sys.exit(1)

        # 1. 查询版本
        cli.get_version()
        time.sleep(0.1)

        # 2. 查询纸张状态
        cli.query_printer_status()
        time.sleep(0.1)

        # 3. 查询设备状态
        cli.query_device_status()
        time.sleep(0.1)

        # 4. 设置纸宽 (可选)
        if len(sys.argv) > 1 and sys.argv[1] == "3inch":
            cli.set_dpi_width(inch_2=False)

        # 5. 打印自检页 (可选, 取消注释以执行)
        # cli.self_test()

        # 6. 显示综合状态
        # results = cli.status_all()
        # print(f"\n综合状态: {json.dumps(results, indent=2, ensure_ascii=False)}")

    except KeyboardInterrupt:
        print("\n中断")
    finally:
        cli.close()
