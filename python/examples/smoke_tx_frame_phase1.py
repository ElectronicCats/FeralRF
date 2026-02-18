#!/usr/bin/env python3
"""
FeralRF - TX frame smoke (phase 1)
"""

import argparse
import sys


def step(title: str) -> None:
    print(f"[STEP] {title}")


def ok(msg: str) -> None:
    print(f"[ OK ] {msg}")


def fail(msg: str) -> None:
    print(f"[FAIL] {msg}")


def parse_frame_hex(frame_hex: str) -> bytes:
    normalized = frame_hex.replace(" ", "").replace(":", "").strip()
    if len(normalized) == 0:
        raise ValueError("empty frame")
    if len(normalized) % 2 != 0:
        raise ValueError("hex length must be even")
    return bytes.fromhex(normalized)


def main() -> int:
    parser = argparse.ArgumentParser(description="FeralRF TX frame smoke (phase 1)")
    parser.add_argument("--port", "-p", help="Serial port (auto-detect if omitted)", default=None)
    parser.add_argument("--baudrate", "-b", help="UART baudrate", type=int, default=921600)
    parser.add_argument("--phy", help="PHY id", type=int, default=4)
    parser.add_argument("--channel", "-c", help="RF channel", type=int, default=25)
    parser.add_argument("--power", help="TX power dBm", type=int, default=0)
    parser.add_argument(
        "--tx-timeout",
        help="Timeout waiting TX ACK (seconds)",
        type=float,
        default=5.0,
    )
    parser.add_argument(
        "--frame-hex",
        help="Frame payload bytes in hex (without 0x, default: 01020304)",
        default="01020304",
    )
    args = parser.parse_args()

    try:
        frame = parse_frame_hex(args.frame_hex)
    except ValueError as exc:
        fail(f"Invalid frame hex: {exc}")
        return 2

    if args.phy in (0, 1, 2, 3):
        if args.channel not in (37, 38, 39):
            fail("BLE TX frame currently supports only advertising channels 37, 38 or 39")
            return 2
        if len(frame) > 31:
            fail("BLE TX frame payload must be <= 31 bytes")
            return 2

    if args.phy == 4 and len(frame) > 125:
        fail("IEEE 802.15.4 TX frame payload must be <= 125 bytes")
        return 2

    print("FeralRF TX Frame Smoke Test (Phase 1)")
    print("======================================")
    print(
        f"port={args.port or 'auto'} baudrate={args.baudrate} phy={args.phy} "
        f"channel={args.channel} power={args.power} len={len(frame)}"
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

        step("SET_PHY + SET_CHANNEL + SET_POWER")
        radio.set_phy(PHY(args.phy), args.channel)
        radio.set_channel(args.channel)
        radio.set_power(args.power)
        ok("Config ACK")

        step("TX_FRAME")
        radio.transmit_frame(frame, timeout=args.tx_timeout)
        ok("TX_FRAME ACK")

        print()
        ok("TX FRAME SMOKE PASS")
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
