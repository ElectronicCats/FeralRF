#!/usr/bin/env python3
"""
FeralRF - OTA RX probe helper

Use this script on the receiver radio while another radio transmits with
`ota_tx_burst.py` or `ota_tx_frame.py`.
"""

import argparse
import sys
from typing import List

from feralrf import PHY, Radio
from feralrf.exceptions import ConnectionError, FeralRFError, TimeoutError
from feralrf.radio import Packet


def step(title: str) -> None:
    print(f"[STEP] {title}")


def ok(msg: str) -> None:
    print(f"[ OK ] {msg}")


def fail(msg: str) -> None:
    print(f"[FAIL] {msg}")


def parse_hex(value: str) -> bytes:
    normalized = value.replace(" ", "").replace(":", "").strip()
    if len(normalized) == 0:
        raise ValueError("empty hex")
    if len(normalized) % 2 != 0:
        raise ValueError("hex length must be even")
    return bytes.fromhex(normalized)


def packet_matches_marker(pkt: Packet, marker: bytes, require_crc_ok: bool) -> bool:
    if require_crc_ok and not pkt.crc_ok:
        return False
    return marker in pkt.data


def main() -> int:
    parser = argparse.ArgumentParser(description="FeralRF OTA RX probe helper")
    parser.add_argument("--port", "-p", help="Serial port (auto-detect if omitted)", default=None)
    parser.add_argument("--baudrate", "-b", help="UART baudrate", type=int, default=921600)
    parser.add_argument("--phy", help="PHY id (0=BLE_1M, 4=IEEE_802_15_4)", type=int, default=4)
    parser.add_argument("--channel", "-c", help="RF channel", type=int, default=25)
    parser.add_argument(
        "--duration",
        "-d",
        help="RX window in seconds",
        type=float,
        default=10.0,
    )
    parser.add_argument(
        "--marker-hex",
        help="Marker to find inside packet payload (hex). If omitted, only packet count is checked.",
        default=None,
    )
    parser.add_argument(
        "--min-hits",
        help="Minimum marker matches (or packets when marker is omitted)",
        type=int,
        default=1,
    )
    parser.add_argument(
        "--allow-crc-fail",
        help="Count matches even if crc_ok is false",
        action="store_true",
    )
    parser.add_argument(
        "--print-limit",
        help="Maximum matching packets to print",
        type=int,
        default=5,
    )
    args = parser.parse_args()

    if args.duration <= 0:
        fail("duration must be > 0")
        return 2
    if args.min_hits < 0:
        fail("min-hits must be >= 0")
        return 2
    if args.print_limit < 0:
        fail("print-limit must be >= 0")
        return 2

    marker = None
    if args.marker_hex is not None:
        try:
            marker = parse_hex(args.marker_hex)
        except ValueError as exc:
            fail(f"Invalid marker-hex: {exc}")
            return 2

    print("FeralRF OTA RX Probe")
    print("====================")
    print(
        f"port={args.port or 'auto'} baudrate={args.baudrate} phy={args.phy} "
        f"channel={args.channel} duration={args.duration}s min_hits={args.min_hits} "
        f"marker={'none' if marker is None else marker.hex()} "
        f"allow_crc_fail={args.allow_crc_fail}"
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

        step("SET_PHY + SET_CHANNEL")
        radio.set_phy(PHY(args.phy), args.channel)
        radio.set_channel(args.channel)
        ok("Config ACK")

        step("RX_START")
        radio.start_rx()
        ok("RX_START ACK")

        step("Read packets")
        packets: List[Packet] = list(radio.read_packets(timeout=args.duration))
        crc_ok_packets = sum(1 for p in packets if p.crc_ok)
        ok(f"packets_total={len(packets)} crc_ok={crc_ok_packets}")

        match_count = 0
        printed = 0
        if marker is not None:
            for pkt in packets:
                if packet_matches_marker(pkt, marker, require_crc_ok=not args.allow_crc_fail):
                    match_count += 1
                    if printed < args.print_limit:
                        print(
                            "[HIT] "
                            f"ts={pkt.timestamp_us}us ch={pkt.channel} rssi={pkt.rssi_dbm} "
                            f"crc_ok={pkt.crc_ok} len={len(pkt.data)} data={pkt.data.hex()}"
                        )
                        printed += 1
            ok(f"marker_hits={match_count}")
        else:
            match_count = len(packets)

        step("RX_STOP")
        radio.stop_rx()
        ok("RX_STOP ACK")

        print()
        if match_count >= args.min_hits:
            ok(f"RX PROBE PASS hits={match_count} min_hits={args.min_hits}")
            return 0

        fail(f"RX PROBE FAIL hits={match_count} min_hits={args.min_hits}")
        return 7

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
        try:
            radio.stop_rx(retries=1, timeout=0.2)
        except Exception:
            pass
        radio.disconnect()


if __name__ == "__main__":
    sys.exit(main())
