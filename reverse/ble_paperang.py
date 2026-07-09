#!/usr/bin/env python3
"""Paperang N2 BLE client — bypass app, direct MCU bridge via Nordic UART Service."""
import asyncio, sys
from bleak import BleakScanner, BleakClient

# Nordic UART Service (NUS) UUIDs
NUS_SVC = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
NUS_TX  = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"  # phone→MCU
NUS_RX  = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"  # MCU→phone

# Custom GATT Service (dlamPrinter side)
CUSTOM_SVC = "a3c87500-8ed3-4bdf-8a39-a01bebede295"

# UART frame helpers
SYNC, END = 0xA5, 0x5A

def crc32(data: bytes) -> bytes:
    """CRC32 poly=0xEDB88320 init=0x35769521 reflected LE (MCU protocol)"""
    c = 0x35769521 ^ 0xFFFFFFFF
    for b in data:
        c = c ^ b
        for _ in range(8):
            c = (c >> 1) ^ 0xEDB88320 if (c & 1) else (c >> 1)
    c = c ^ 0xFFFFFFFF
    return c.to_bytes(4, 'little')

def build_frame(main, sub, ack=1, payload=b''):
    """Build MCU UART frame: [A5][01][tl_LE][m][s][ack][dl_LE][data][CRC32_LE][5A]"""
    dl = len(payload)
    tl = 5 + dl
    hdr = bytes([SYNC, 0x01, tl & 0xFF, (tl >> 8) & 0xFF, main, sub, ack,
                 dl & 0xFF, (dl >> 8) & 0xFF])
    body = hdr[4:] + payload  # CRC covers from main onwards
    return hdr + payload + crc32(body) + bytes([END])

async def scan(addr=None):
    """Scan for Paperang devices or return specific device by address."""
    if addr:
        dev = await BleakScanner.find_device_by_address(addr, timeout=5)
        if dev:
            print(f"Found: {dev.name} ({dev.address}) RSSI={dev.rssi}")
        return dev
    print("Scanning 10s for Paperang...")
    devices = await BleakScanner.discover(timeout=10)
    for d in devices:
        if d.name and 'paper' in d.name.lower():
            print(f"  {d.address}  {d.name}  RSSI={d.rssi}")

async def connect(addr):
    """Connect and demonstrate basic MCU interaction."""
    async with BleakClient(addr) as c:
        print(f"Connected MTU={c.mtu}")

        def on_rx(sender, data):
            print(f"MCU> {data.hex()}")

        await c.start_notify(NUS_RX, on_rx)
        print("Listening on NUS RX...")

        # Send status query (5,0x0D) — hardware status
        f = build_frame(5, 0x0D, ack=1)
        print(f"TX> {f.hex()}")
        await c.write_gatt_char(NUS_TX, f, response=False)
        await asyncio.sleep(3)

        # Send paper status query (5,0x0C)
        f = build_frame(5, 0x0C, ack=1)
        print(f"TX> {f.hex()}")
        await c.write_gatt_char(NUS_TX, f, response=False)
        await asyncio.sleep(5)

async def cmd(addr, main, sub, payload_hex=""):
    """Send a single UART frame and listen for response."""
    payload = bytes.fromhex(payload_hex) if payload_hex else b''
    async with BleakClient(addr) as c:
        def on_rx(sender, data):
            print(f"MCU> {data.hex()}")
        await c.start_notify(NUS_RX, on_rx)

        f = build_frame(int(main, 16), int(sub, 16))
        print(f"TX> {f.hex()}")
        await c.write_gatt_char(NUS_TX, f, response=False)
        await asyncio.sleep(5)

async def list_services(addr):
    """List all GATT services and characteristics."""
    async with BleakClient(addr) as c:
        for svc in c.services:
            print(f"\n{svc.uuid}")
            for char in svc.characteristics:
                p = "".join(
                    "R" if "read" in char.properties else "",
                    "W" if "write" in char.properties else "",
                    "N" if "notify" in char.properties else "",
                )
                print(f"  {char.uuid} [{p}]")

if __name__ == "__main__":
    import argparse
    p = argparse.ArgumentParser(description="Paperang BLE client")
    p.add_argument("-a", "--addr", help="BLE MAC address")
    p.add_argument("--scan", action="store_true", help="Scan for devices")
    p.add_argument("--list", action="store_true", help="List services")
    p.add_argument("-m", "--main", default="5", help="UART main (hex)")
    p.add_argument("-s", "--sub", default="0D", help="UART sub (hex)")
    p.add_argument("-d", "--data", default="", help="Payload hex")
    args = p.parse_args()

    if args.scan:
        asyncio.run(scan(args.addr))
    elif args.list and args.addr:
        asyncio.run(list_services(args.addr))
    elif args.addr:
        if args.main != "5" or args.sub != "0D":
            asyncio.run(cmd(args.addr, args.main, args.sub, args.data))
        else:
            asyncio.run(connect(args.addr))
    else:
        p.print_help()
