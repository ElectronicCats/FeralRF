#!/usr/bin/env python3
"""
FeralRF - OTA TX burst helper

Use this script on the transmitter radio while another radio listens with
`ota_rx_probe.py`.
"""

import argparse
import sys
import time

from feralrf import PHY, Radio
from feralrf.exceptions import ConnectionError, FeralRFError, TimeoutError


def step(title: str) -> None:
    print(f"[STEP] {title}")


def ok(msg: str) -> None:
    print(f"[ OK ] {msg}")


def fail(msg: str) -> None:
    print(f"[FAIL] {msg}")


def parse_payload_hex(payload_hex: str) -> bytes:
    normalized = payload_hex.replace(" ", "").replace(":", "").strip()
    if len(normalized) == 0:
        raise ValueError("empty payload")
    if len(normalized) % 2 != 0:
        raise ValueError("hex length must be even")
    return bytes.fromhex(normalized)


def main() -> int:
    parser = argparse.ArgumentParser(description="FeralRF OTA TX burst helper")
    parser.add_argument("--port", "-p", help="Serial port (auto-detect if omitted)", default=None)
    parser.add_argument("--baudrate", "-b", help="UART baudrate", type=int, default=921600)
    parser.add_argument("--phy", help="PHY id (0=BLE_1M, 4=IEEE_802_15_4)", type=int, default=4)
    parser.add_argument("--channel", "-c", help="RF channel", type=int, default=25)
    parser.add_argument("--power", help="TX power dBm", type=int, default=0)
    parser.add_argument(
        "--payload-hex",
        help="Raw payload bytes in hex",
        required=True,
    )
    parser.add_argument(
        "--count",
        help="Number of TX frames to send",
        type=int,
        default=40,
    )
    parser.add_argument(
        "--interval-ms",
        help="Inter-frame interval in milliseconds",
        type=float,
        default=25.0,
    )
    parser.add_argument(
        "--tx-timeout",
        help="Timeout waiting TX ACK (seconds)",
        type=float,
        default=5.0,
    )
    args = parser.parse_args()

    if args.count <= 0:
        fail("count must be > 0")
        return 2
    if args.interval_ms < 0:
        fail("interval-ms must be >= 0")
        return 2

    try:
        payload = parse_payload_hex(args.payload_hex)
    except ValueError as exc:
        fail(f"Invalid payload hex: {exc}")
        return 2

    if args.phy == int(PHY.BLE_1M) and args.channel not in (37, 38, 39):
        fail("For BLE TX, channel must be 37, 38 or 39")
        return 2

    print("FeralRF OTA TX Burst")
    print("====================")
    print(
        f"port={args.port or 'auto'} baudrate={args.baudrate} phy={args.phy} "
        f"channel={args.channel} power={args.power} len={len(payload)} "
        f"count={args.count} interval_ms={args.interval_ms} tx_timeout={args.tx_timeout}s"
    )
    print()

    radio = Radio(port=args.port, baudrate=args.baudrate)

    try:
        step("Connect + RADIO_INIT + GET_INFO")
        info = radio.init()
        ok(
            f"INFO firmware={info.firmware_version} capabilities=0x{info.capabilities:02X} "
            f"serial={info.serial or 'n/a'}"
        )

        step("SET_PHY + SET_CHANNEL + SET_POWER")
        radio.set_phy(PHY(args.phy), args.channel)
        radio.set_channel(args.channel)
        radio.set_power(args.power)
        ok("Config ACK")

        step("TX burst")
        sent = 0
        for i in range(args.count):
            radio.transmit(payload, power_dbm=args.power, timeout=args.tx_timeout)
            sent += 1
            if i < 3 or i + 1 == args.count:
                print(f"[TX {i + 1:03d}/{args.count:03d}] ack")
            if args.interval_ms > 0 and (i + 1) < args.count:
                time.sleep(args.interval_ms / 1000.0)

        print()
        ok(f"TX BURST PASS sent={sent}/{args.count}")
        return 0

    except ValueError as exc:
        fail(f"Invalid argument: {exc}")
        return 2
    except ConnectionError as exc:
        fail(f"Connection error: {exc}")
        return 3
    except TimeoutError as exc:
        fail(f"Timeout waiting for response: {exc}")
        return 4
    except FeralRFError as exc:
        fail(f"Protocol/command error: {exc}")
        return 5
    except Exception as exc:
        fail(f"Unexpected error: {exc}")
        return 6
    finally:
        radio.disconnect()


if __name__ == "__main__":
    sys.exit(main())
