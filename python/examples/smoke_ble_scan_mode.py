#!/usr/bin/env python3
"""
FeralRF - BLE passive/active scan smoke
"""

import argparse
import sys


def step(title: str) -> None:
    print(f"[STEP] {title}")


def ok(msg: str) -> None:
    print(f"[ OK ] {msg}")


def fail(msg: str) -> None:
    print(f"[FAIL] {msg}")


def main() -> int:
    parser = argparse.ArgumentParser(description="FeralRF BLE passive/active scan smoke")
    parser.add_argument("--port", "-p", help="Serial port (auto-detect if omitted)", default=None)
    parser.add_argument("--baudrate", "-b", help="UART baudrate", type=int, default=921600)
    parser.add_argument(
        "--channel",
        "-c",
        help="BLE advertising channel (37, 38 or 39)",
        type=int,
        default=37,
    )
    parser.add_argument(
        "--mode",
        choices=("passive", "active"),
        default="passive",
        help="BLE scan mode to validate",
    )
    parser.add_argument(
        "--duration",
        "-d",
        help="RX read window in seconds",
        type=float,
        default=5.0,
    )
    parser.add_argument(
        "--min-packets",
        help="Minimum packets required to consider smoke valid",
        type=int,
        default=0,
    )
    args = parser.parse_args()

    if args.channel not in (37, 38, 39):
        fail("Invalid channel: use 37, 38 or 39")
        return 2
    if args.duration <= 0:
        fail("duration must be > 0")
        return 2
    if args.min_packets < 0:
        fail("min-packets must be >= 0")
        return 2

    print("FeralRF BLE Scan Mode Smoke Test")
    print("================================")
    print(
        f"port={args.port or 'auto'} baudrate={args.baudrate} "
        f"channel={args.channel} mode={args.mode} duration={args.duration}s "
        f"min_packets={args.min_packets}"
    )
    print()

    from feralrf import PHY, Radio, RxStreamError
    from feralrf.exceptions import ConnectionError, FeralRFError, TimeoutError

    radio = Radio(port=args.port, baudrate=args.baudrate)
    active = args.mode == "active"

    try:
        step("Connect + RADIO_INIT + GET_INFO")
        info = radio.init()
        ok(
            f"INFO firmware={info.firmware_version} capabilities=0x{info.capabilities:02X} "
            f"serial={info.serial or 'n/a'}"
        )

        step("SET_PHY BLE_1M + SET_CHANNEL")
        radio.set_phy(PHY.BLE_1M, args.channel)
        radio.set_channel(args.channel)
        ok("Config ACK")

        step(f"SET_BLE_SCAN_MODE {args.mode}")
        radio.set_ble_scan_mode(active=active)
        ok("SET_BLE_SCAN_MODE ACK")

        step("RX_START")
        radio.start_rx()
        ok("RX_START ACK")

        step("Read packets")
        packets = [
            p for p in radio.read_packets(timeout=args.duration) if not isinstance(p, RxStreamError)
        ]
        total = len(packets)
        scan_like = sum(1 for pkt in packets if pkt.ll_pdu_kind == 2)
        adv_like = sum(1 for pkt in packets if pkt.ll_pdu_kind == 1)
        ok(f"packets={total} adv_like={adv_like} scan_like={scan_like}")

        step("RX_STOP")
        radio.stop_rx()
        ok("RX_STOP ACK")

        step("GET_STATS")
        stats = radio.get_stats()
        ok(
            "stats: "
            f"rx_ok={stats.rx_ok} rx_crc_err={stats.rx_crc_err} "
            f"rx_drop={stats.rx_drop} rx_overflow={stats.rx_overflow} "
            f"ll_adv={stats.ll_kind_adv} ll_scan={stats.ll_kind_scan}"
        )

        if total < args.min_packets:
            fail(f"Expected at least {args.min_packets} packets, got {total}")
            return 7

        print()
        ok("BLE SCAN SMOKE PASS")
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
