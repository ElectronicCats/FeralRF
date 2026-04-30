#!/usr/bin/env python3
"""FeralRF — BLE connect + GATT discovery demo (F8 validation).

Usage:
    python demo_ble_connect_gatt.py               # scan + interactive pick
    python demo_ble_connect_gatt.py <addr>        # connect to address (random by default)
    python demo_ble_connect_gatt.py <addr> 0      # connect to address (public)
    python demo_ble_connect_gatt.py <addr> <type> --read  # also read first readable char
"""

import struct
import sys
import time
from typing import Optional

from feralrf import Radio
from feralrf.enums import PHY

KNOWN_UUIDS = {
    0x1800: "Generic Access",
    0x1801: "Generic Attribute",
    0x180A: "Device Information",
    0x180F: "Battery Service",
    0x1810: "Blood Pressure",
    0x1812: "Human Interface Device",
    0x1816: "Cycling Speed and Cadence",
    0x1818: "Cycling Power",
    0x181C: "User Data",
    0x1848: "Media Control Service",
    0xFE2C: "Google Fast Pair",
    0x2A00: "Device Name",
    0x2A01: "Appearance",
    0x2A04: "Peripheral Preferred Conn Params",
    0x2A05: "Service Changed",
    0x2A19: "Battery Level",
    0x2A24: "Model Number String",
    0x2A25: "Serial Number String",
    0x2A26: "Firmware Revision String",
    0x2A27: "Hardware Revision String",
    0x2A28: "Software Revision String",
    0x2A29: "Manufacturer Name String",
    0x2A50: "PnP ID",
}

CHAR_PROPS = {
    0x01: "Broadcast",
    0x02: "Read",
    0x04: "WriteNoRsp",
    0x08: "Write",
    0x10: "Notify",
    0x20: "Indicate",
    0x40: "AuthWrite",
    0x80: "ExtProps",
}


def uuid_str(uuid_bytes: bytes) -> str:
    if len(uuid_bytes) == 2:
        val = struct.unpack("<H", uuid_bytes)[0]
        name = KNOWN_UUIDS.get(val, "")
        return f"0x{val:04X} ({name})" if name else f"0x{val:04X}"
    if len(uuid_bytes) == 16:
        b = uuid_bytes[::-1]
        return f"{b[0:4].hex()}-{b[4:6].hex()}-{b[6:8].hex()}-" f"{b[8:10].hex()}-{b[10:16].hex()}"
    return uuid_bytes.hex()


def props_str(props: int) -> str:
    flags = [name for bit, name in CHAR_PROPS.items() if props & bit]
    return "|".join(flags) if flags else "None"


def parse_addr_arg(arg: str) -> bytes:
    """Accept 'AA:BB:CC:DD:EE:FF' or 'AABBCCDDEEFF', return 6 LE bytes."""
    clean = arg.replace(":", "").strip()
    if len(clean) != 12:
        raise SystemExit("ERROR: address must be 6 bytes (12 hex chars)")
    raw = bytes.fromhex(clean)
    return raw[::-1]


def scan_and_pick(radio: Radio, duration: float = 5.0) -> Optional[tuple]:
    """Scan ch37 for connectable advertisers; return (addr_le, addr_type)."""
    radio.set_phy(PHY.BLE_1M, channel=37)
    radio.start_rx()
    print(f"Scanning BLE ch37 for {duration:.0f}s...")
    devices: dict = {}
    deadline = time.monotonic() + duration

    for pkt in radio.read_packets(timeout=duration):
        if time.monotonic() > deadline:
            break
        if not pkt.crc_ok or not pkt.data or len(pkt.data) < 8:
            continue
        pdu_type = pkt.data[0] & 0x0F
        tx_add = (pkt.data[0] >> 6) & 1
        if pdu_type not in (0, 1, 2, 6):
            continue
        addr_le = pkt.data[2:8]
        key = bytes(addr_le)
        name = ""
        i = 8
        while i + 1 < len(pkt.data):
            ad_len = pkt.data[i]
            if ad_len == 0 or i + 1 + ad_len > len(pkt.data):
                break
            ad_type = pkt.data[i + 1]
            if ad_type in (0x08, 0x09):
                try:
                    name = pkt.data[i + 2 : i + 1 + ad_len].decode("utf-8", errors="replace")
                except Exception:
                    pass
            i += 1 + ad_len
        connectable = pdu_type in (0, 1)
        entry = (addr_le, tx_add, pkt.rssi_dbm, name, connectable)
        if key not in devices or pkt.rssi_dbm > devices[key][2]:
            devices[key] = entry

    radio.stop_rx()

    if not devices:
        print("No BLE advertisers found.")
        return None

    items = list(devices.values())
    connectable_idx = [i for i, e in enumerate(items) if e[4]]
    print(f"\nFound {len(items)} device(s):")
    for i, (addr, atype, rssi, name, conn) in enumerate(items):
        addr_str = ":".join(f"{b:02X}" for b in reversed(addr))
        type_str = "random" if atype else "public"
        conn_str = "CONN" if conn else "non-conn"
        line = f"  [{i}] {addr_str} ({type_str}, {conn_str}) RSSI={rssi}"
        if name:
            line += f'  "{name}"'
        print(line)

    if not connectable_idx:
        print("\nNo connectable advertisers found.")
        return None

    try:
        choice = input(f"\nSelect device to connect [{connectable_idx[0]}]: ").strip()
        idx = int(choice) if choice else connectable_idx[0]
    except EOFError:
        idx = connectable_idx[0]
        print(f"(auto-selected [{idx}])")
    addr_le, addr_type, _, _, _ = items[idx]
    return bytes(addr_le), addr_type


def run(argv: list) -> int:
    do_read = "--read" in argv
    positional = [a for a in argv if not a.startswith("--")]

    addr_le: Optional[bytes] = None
    addr_type = 1

    with Radio() as radio:
        radio.init()

        if len(positional) >= 1:
            addr_le = parse_addr_arg(positional[0])
            addr_type = int(positional[1]) if len(positional) >= 2 else 1
        else:
            pick = scan_and_pick(radio)
            if pick is None:
                return 1
            addr_le, addr_type = pick

        assert addr_le is not None
        addr_str = ":".join(f"{b:02X}" for b in reversed(addr_le))
        print(f"\nConnecting to {addr_str} (type={'random' if addr_type else 'public'})")

        result = radio.ble_connect(addr_le, addr_type, timeout=8.0)
        results = {0: "OK", 1: "TIMEOUT", 2: "NO_SYNC", 3: "RF_ERR"}
        print(f"Connection result: {results.get(result.result, f'0x{result.result:02X}')}")
        if not result.is_ok:
            return 2

        time.sleep(2.0)
        status = radio.conn_status()
        print(
            f"  connected={status.connected} events={status.events} "
            f"att_state={status.att_state} last_status=0x{status.last_status:04X}"
        )
        if not status.connected:
            print("Connection dropped before GATT.")
            return 3

        print("\n=== GATT Discovery ===")
        discovery = radio.gatt_discover(timeout=30.0)

        print(f"\nServices ({len(discovery.services)}):")
        for svc in discovery.services:
            print(f"  0x{svc.start_handle:04X}-0x{svc.end_handle:04X}  UUID={uuid_str(svc.uuid)}")
        print(f"\nCharacteristics ({len(discovery.characteristics)}):")
        for ch in discovery.characteristics:
            print(
                f"  decl=0x{ch.handle:04X} val=0x{ch.value_handle:04X}  "
                f"props=[{props_str(ch.properties)}]  UUID={uuid_str(ch.uuid)}"
            )
        print(f"\nGATT done (status={discovery.status})")

        if do_read and discovery.characteristics:
            for ch in discovery.characteristics:
                if ch.properties & 0x02:
                    print(f"\nReading 0x{ch.value_handle:04X} ({uuid_str(ch.uuid)})...")
                    value = radio.gatt_read(ch.value_handle)
                    try:
                        print(f"  UTF-8: {value.decode('utf-8')!r}")
                    except UnicodeDecodeError:
                        print(f"  hex:   {value.hex()}")
                    break

        print("\nDisconnecting...")
        radio.ble_disconnect()
        print("Disconnected.")
    return 0


if __name__ == "__main__":
    sys.exit(run(sys.argv[1:]))
