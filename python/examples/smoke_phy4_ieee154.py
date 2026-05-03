#!/usr/bin/env python3
"""
FeralRF - IEEE 802.15.4 RX smoke test (PHY 4)
"""

import argparse
import sys

from feralrf import PHY, Radio, RxStreamError
from feralrf.exceptions import ConnectionError, FeralRFError, TimeoutError


def step(title: str) -> None:
    print(f"[STEP] {title}")


def ok(msg: str) -> None:
    print(f"[ OK ] {msg}")


def fail(msg: str) -> None:
    print(f"[FAIL] {msg}")


def main() -> int:
    parser = argparse.ArgumentParser(description="FeralRF IEEE 802.15.4 RX smoke")
    parser.add_argument("--port", "-p", help="Serial port (auto-detect if omitted)", default=None)
    parser.add_argument("--baudrate", "-b", help="UART baudrate", type=int, default=921600)
    parser.add_argument(
        "--channel",
        "-c",
        help="IEEE 802.15.4 channel (11..26)",
        type=int,
        default=11,
    )
    parser.add_argument(
        "--duration",
        "-d",
        help="RX read window in seconds",
        type=float,
        default=5.0,
    )
    args = parser.parse_args()

    print("FeralRF IEEE 802.15.4 Smoke Test")
    print("================================")
    print(
        f"port={args.port or 'auto'} baudrate={args.baudrate} phy={int(PHY.IEEE_802_15_4)} "
        f"channel={args.channel} duration={args.duration}s"
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

        step("SET_PHY IEEE_802_15_4")
        radio.set_phy(PHY.IEEE_802_15_4, args.channel)
        ok("SET_PHY ACK")

        step("SET_CHANNEL")
        radio.set_channel(args.channel)
        ok("SET_CHANNEL ACK")

        step("RX_START")
        radio.start_rx()
        ok("RX_START ACK")

        step("Read packets")
        packets = [
            p for p in radio.read_packets(timeout=args.duration) if not isinstance(p, RxStreamError)
        ]
        ok(f"packets={len(packets)}")
        if packets:
            p = packets[0]
            ok(
                "first: "
                f"ts={p.timestamp_us}us ch={p.channel} rssi={p.rssi_dbm} "
                f"lqi={p.lqi} crc_ok={p.crc_ok} len={len(p.data)}"
            )

        step("RX_STOP")
        radio.stop_rx()
        ok("RX_STOP ACK")

        print()
        ok("SMOKE TEST PASS")
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
