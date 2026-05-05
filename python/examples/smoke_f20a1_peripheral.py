#!/usr/bin/env python3
"""F20.a.1 — Smoke V1 BLE peripheral Read-only cross-validation 2 boards."""
import argparse
import re
import sys
import time
from threading import Thread

import serial

from feralrf.radio import Radio


def reset_cc1352(port: str) -> None:
    m = re.search(r"(\d+)$", port)
    if not m:
        return
    shell = port[: m.start(1)] + str(int(m.group(1)) + 2)
    try:
        s = serial.Serial(shell, 115200, timeout=1.0, write_timeout=1.0)
        s.write(b"boot\r\n")
        time.sleep(0.5)
        s.write(b"exit\r\n")
        time.sleep(0.3)
        s.close()
    except Exception:
        pass
    time.sleep(3.5)


def run_peripheral(port: str, baud: int, target_addr: str) -> None:
    radio = Radio(port=port, baudrate=baud)
    try:
        radio.init()
        radio.serve_gatt()
        radio.advertise_ind(
            payload=b"\x02\x01\x06",
            scan_resp_data=b"FERAL_GATT_SR",
            target_addr=target_addr,
            count=200,
            interval_us=10000,
        )
    finally:
        radio.disconnect()


def run_central(port: str, baud: int, target_addr: str) -> tuple:
    """Returns (services_count, chars_count, name_value, test_value)."""
    addr_le = bytes(int(p, 16) for p in reversed(target_addr.split(":")))
    radio = Radio(port=port, baudrate=baud)
    try:
        radio.init()
        radio.reset_device()
        radio.init()
        result = radio.ble_connect(addr_le, addr_type=1, timeout=10.0)
        if not result.is_ok:
            return (0, 0, b"", b"")
        services = radio.gatt_discover(timeout=10.0)
        name_val = radio.gatt_read(handle=3, timeout=5.0)
        test_val = radio.gatt_read(handle=6, timeout=5.0)
        try:
            radio.ble_disconnect(timeout=5.0)
        except Exception:
            pass
        return (len(services.services), len(services.characteristics), name_val, test_val)
    finally:
        radio.disconnect()


def main() -> int:
    parser = argparse.ArgumentParser(description="F20.a.1 smoke V1 peripheral Read-only")
    parser.add_argument("--peripheral-port", required=True)
    parser.add_argument("--central-port", required=True)
    parser.add_argument("--baudrate", type=int, default=921600)
    parser.add_argument("--target-mac", default="DE:AD:BE:EF:CA:FE")
    args = parser.parse_args()

    reset_cc1352(args.peripheral_port)
    reset_cc1352(args.central_port)

    print(f"Peripheral on {args.peripheral_port}; Central on {args.central_port}")
    print(f"Target MAC: {args.target_mac}")
    print("=" * 60)

    peripheral_thread = Thread(
        target=run_peripheral,
        args=(args.peripheral_port, args.baudrate, args.target_mac),
        daemon=True,
    )
    peripheral_thread.start()
    time.sleep(0.5)

    services_count, chars_count, name_val, test_val = run_central(
        args.central_port, args.baudrate, args.target_mac
    )

    peripheral_thread.join(timeout=5.0)

    print(f"\nServices discovered: {services_count}")
    print(f"Chars discovered:    {chars_count}")
    print(f"Device Name read:    {name_val!r}")
    print(f"Test Read read:      {test_val!r}")

    expected_name = b"FERAL_GATT"
    expected_test = b"HELLO_FERAL"
    pass_services = services_count >= 2
    pass_chars = chars_count >= 2
    pass_name = name_val == expected_name
    pass_test = test_val == expected_test

    print("\n" + "=" * 60)
    print(f"[{'PASS' if pass_services else 'FAIL'}] services >= 2: {services_count}")
    print(f"[{'PASS' if pass_chars else 'FAIL'}] chars >= 2: {chars_count}")
    print(f"[{'PASS' if pass_name else 'FAIL'}] device name == 'FERAL_GATT'")
    print(f"[{'PASS' if pass_test else 'FAIL'}] test value == 'HELLO_FERAL'")

    all_pass = pass_services and pass_chars and pass_name and pass_test
    return 0 if all_pass else 1


if __name__ == "__main__":
    sys.exit(main())
