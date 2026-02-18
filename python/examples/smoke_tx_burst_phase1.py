#!/usr/bin/env python3
"""
FeralRF - TX burst smoke (phase 1)
"""

import argparse
import sys


def step(title: str) -> None:
    print(f"[STEP] {title}")


def ok(msg: str) -> None:
    print(f"[ OK ] {msg}")


def fail(msg: str) -> None:
    print(f"[FAIL] {msg}")


def parse_packet_hex(packet_hex: str) -> bytes:
    normalized = packet_hex.replace(" ", "").replace(":", "").strip()
    if len(normalized) == 0:
        raise ValueError("empty packet")
    if len(normalized) % 2 != 0:
        raise ValueError("hex length must be even")
    return bytes.fromhex(normalized)


def main() -> int:
    parser = argparse.ArgumentParser(description="FeralRF TX burst smoke (phase 1)")
    parser.add_argument("--port", "-p", help="Serial port (auto-detect if omitted)", default=None)
    parser.add_argument("--baudrate", "-b", help="UART baudrate", type=int, default=921600)
    parser.add_argument("--phy", help="PHY id (phase 1 supports PHY 4)", type=int, default=4)
    parser.add_argument("--channel", "-c", help="RF channel", type=int, default=25)
    parser.add_argument("--power", help="TX power dBm", type=int, default=0)
    parser.add_argument(
        "--tx-timeout",
        help="Timeout waiting TX ACK (seconds)",
        type=float,
        default=5.0,
    )
    parser.add_argument(
        "--packet-hex",
        help="Raw packet bytes in hex (without 0x, default: 01020304)",
        default="01020304",
    )
    parser.add_argument(
        "--count",
        help="Burst packet count (1..65535)",
        type=int,
        default=5,
    )
    parser.add_argument(
        "--interval-us",
        help="Inter-packet interval in microseconds",
        type=int,
        default=5000,
    )
    args = parser.parse_args()

    try:
        packet = parse_packet_hex(args.packet_hex)
    except ValueError as exc:
        fail(f"Invalid packet hex: {exc}")
        return 2

    if args.count <= 0 or args.count > 0xFFFF:
        fail("count must be in range 1..65535")
        return 2
    if args.interval_us < 0:
        fail("interval-us must be >= 0")
        return 2

    print("FeralRF TX Burst Smoke Test (Phase 1)")
    print("======================================")
    print(
        f"port={args.port or 'auto'} baudrate={args.baudrate} phy={args.phy} "
        f"channel={args.channel} power={args.power} len={len(packet)} "
        f"count={args.count} interval_us={args.interval_us}"
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

        step("TX_BURST")
        radio.transmit_burst(
            packet,
            count=args.count,
            interval_us=args.interval_us,
            timeout=args.tx_timeout,
        )
        ok("TX_BURST ACK")

        print()
        ok("TX BURST SMOKE PASS")
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
