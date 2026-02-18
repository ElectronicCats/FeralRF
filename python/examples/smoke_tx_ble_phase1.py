#!/usr/bin/env python3
"""
FeralRF - TX BLE advertising smoke (phase 1)
"""

import argparse
import sys


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
    payload = bytes.fromhex(normalized)
    if len(payload) > 31:
        raise ValueError("BLE ADV payload must be <= 31 bytes")
    return payload


def main() -> int:
    parser = argparse.ArgumentParser(description="FeralRF TX BLE advertising smoke (phase 1)")
    parser.add_argument("--port", "-p", help="Serial port (auto-detect if omitted)", default=None)
    parser.add_argument("--baudrate", "-b", help="UART baudrate", type=int, default=921600)
    parser.add_argument(
        "--channel",
        "-c",
        help="BLE advertising channel (37, 38 or 39)",
        type=int,
        default=37,
    )
    parser.add_argument("--power", help="TX power dBm", type=int, default=0)
    parser.add_argument(
        "--tx-timeout", help="Timeout waiting TX ACK (seconds)", type=float, default=5.0
    )
    parser.add_argument(
        "--payload-hex",
        help="BLE advertising payload bytes in hex (default: 020106)",
        default="020106",
    )
    args = parser.parse_args()

    if args.channel not in (37, 38, 39):
        fail("Invalid channel: use 37, 38 or 39")
        return 2

    try:
        payload = parse_payload_hex(args.payload_hex)
    except ValueError as exc:
        fail(f"Invalid payload hex: {exc}")
        return 2

    print("FeralRF TX BLE ADV Smoke Test (Phase 1)")
    print("========================================")
    print(
        f"port={args.port or 'auto'} baudrate={args.baudrate} phy=0 "
        f"channel={args.channel} power={args.power} adv_len={len(payload)}"
    )
    print()

    from feralrf import PHY, Radio
    from feralrf.exceptions import ConnectionError, FeralRFError, TimeoutError

    radio = Radio(port=args.port, baudrate=args.baudrate)

    try:
        step("Connect + RADIO_INIT + GET_INFO")
        info = radio.init()
        ok(
            f"INFO firmware={info.firmware_version} capabilities=0x{info.capabilities:02X} "
            f"serial={info.serial or 'n/a'}"
        )

        step("SET_PHY BLE_1M + SET_CHANNEL + SET_POWER")
        radio.set_phy(PHY.BLE_1M, args.channel)
        radio.set_channel(args.channel)
        radio.set_power(args.power)
        ok("Config ACK")

        step("TX_RAW (BLE ADV payload)")
        radio.transmit(payload, power_dbm=args.power, timeout=args.tx_timeout)
        ok("TX_RAW ACK")

        print()
        ok("TX BLE SMOKE PASS")
        return 0

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
