#!/usr/bin/env python3
"""
FeralRF - OTA TX frame helper

Use this script on the transmitter radio while another radio listens with
`ota_rx_probe.py`.
"""

import argparse
import sys
import time


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
    parser = argparse.ArgumentParser(description="FeralRF OTA TX frame helper")
    parser.add_argument("--port", "-p", help="Serial port (auto-detect if omitted)", default=None)
    parser.add_argument("--baudrate", "-b", help="UART baudrate", type=int, default=921600)
    parser.add_argument("--phy", help="PHY id (0=BLE_1M, 4=IEEE_802_15_4)", type=int, default=4)
    parser.add_argument("--channel", "-c", help="RF channel", type=int, default=25)
    parser.add_argument("--power", help="TX power dBm", type=int, default=0)
    parser.add_argument(
        "--payload-hex",
        help="Frame payload bytes in hex",
        required=True,
    )
    parser.add_argument(
        "--count",
        help="Number of TX_FRAME commands",
        type=int,
        default=40,
    )
    parser.add_argument(
        "--interval-us",
        help="Inter-frame interval in microseconds",
        type=int,
        default=25000,
    )
    parser.add_argument(
        "--interval-ms",
        help="Inter-frame interval in milliseconds (legacy alias for --interval-us)",
        type=float,
        default=None,
    )
    parser.add_argument(
        "--tx-timeout",
        help="Timeout waiting TX ACK (seconds)",
        type=float,
        default=5.0,
    )
    parser.add_argument(
        "--progress-every",
        help="Print progress every N TX_FRAME sends (0 disables progress lines)",
        type=int,
        default=10,
    )
    args = parser.parse_args()

    from feralrf import PHY, Radio
    from feralrf.exceptions import ConnectionError, FeralRFError, TimeoutError

    if args.count <= 0:
        fail("count must be > 0")
        return 2
    if args.interval_ms is not None:
        args.interval_us = int(max(args.interval_ms, 0.0) * 1000.0)
    if args.interval_us < 0:
        fail("interval-us must be >= 0")
        return 2
    if args.progress_every < 0:
        fail("progress-every must be >= 0")
        return 2

    try:
        payload = parse_payload_hex(args.payload_hex)
    except ValueError as exc:
        fail(f"Invalid payload hex: {exc}")
        return 2

    if args.phy == int(PHY.BLE_1M):
        if args.channel not in (37, 38, 39):
            fail("For BLE TX frame, channel must be 37, 38 or 39")
            return 2
        if len(payload) > 31:
            fail("BLE TX frame payload must be <= 31 bytes")
            return 2

    if args.phy == int(PHY.IEEE_802_15_4) and len(payload) > 125:
        fail("IEEE 802.15.4 TX frame payload must be <= 125 bytes")
        return 2

    print("FeralRF OTA TX Frame")
    print("====================")
    print(
        f"port={args.port or 'auto'} baudrate={args.baudrate} phy={args.phy} "
        f"channel={args.channel} power={args.power} len={len(payload)} "
        f"count={args.count} interval_us={args.interval_us} tx_timeout={args.tx_timeout}s"
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

        step("TX_FRAME loop")
        for idx in range(1, args.count + 1):
            radio.transmit_frame(payload, timeout=args.tx_timeout)
            if args.progress_every > 0 and (idx % args.progress_every == 0 or idx == args.count):
                ok(f"TX_FRAME sent={idx}/{args.count}")
            if idx < args.count and args.interval_us > 0:
                time.sleep(args.interval_us / 1_000_000.0)

        print()
        ok(f"TX FRAME OTA PASS sent={args.count}")
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
