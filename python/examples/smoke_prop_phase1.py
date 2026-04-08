#!/usr/bin/env python3
"""
FeralRF - Proprietary preset smoke (phase 1)
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
    parser = argparse.ArgumentParser(description="FeralRF proprietary preset smoke")
    parser.add_argument("--port", "-p", help="Serial port (auto-detect if omitted)", default=None)
    parser.add_argument("--baudrate", "-b", help="UART baudrate", type=int, default=921600)
    parser.add_argument(
        "--preset",
        help="Preset name from feralrf.PROP_PRESETS",
        default="gfsk_868_50k",
    )
    parser.add_argument("--power", help="TX power dBm", type=int, default=0)
    parser.add_argument(
        "--duration",
        "-d",
        help="RX read window in seconds",
        type=float,
        default=3.0,
    )
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
        "--min-packets",
        help="Minimum packets required during RX window",
        type=int,
        default=0,
    )
    parser.add_argument(
        "--auto-reset",
        help="Call reset_device() automatically after OOK preset validation",
        action="store_true",
    )
    args = parser.parse_args()

    if args.duration <= 0:
        fail("duration must be > 0")
        return 2
    if args.min_packets < 0:
        fail("min-packets must be >= 0")
        return 2

    try:
        packet = parse_packet_hex(args.packet_hex)
    except ValueError as exc:
        fail(f"Invalid packet hex: {exc}")
        return 2

    from feralrf import PHY, PROP_PRESETS, Radio
    from feralrf.exceptions import ConnectionError, FeralRFError, TimeoutError

    if args.preset not in PROP_PRESETS:
        fail(f"Unknown preset: {args.preset}")
        return 2

    preset = PROP_PRESETS[args.preset]
    mod_type = preset.get("mod_type", 1)
    is_ook = mod_type == 2

    print("FeralRF Proprietary Preset Smoke Test")
    print("=====================================")
    print(
        f"port={args.port or 'auto'} baudrate={args.baudrate} preset={args.preset} "
        f"freq={preset.get('frequency_hz')} mod_type={mod_type} "
        f"symbol_rate={preset.get('symbol_rate')} power={args.power} "
        f"duration={args.duration}s min_packets={args.min_packets} "
        f"auto_reset={'yes' if args.auto_reset else 'no'}"
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

        step("SET_PHY PROPRIETARY_GFSK")
        radio.set_phy(PHY.PROPRIETARY_GFSK, 0)
        ok("SET_PHY ACK")

        step("SET_PROP_CONFIG")
        radio.configure_prop(**preset)
        ok("SET_PROP_CONFIG ACK")

        step("SET_POWER")
        radio.set_power(args.power)
        ok("SET_POWER ACK")

        step("RX_START")
        radio.start_rx()
        ok("RX_START ACK")

        step("Read packets")
        packets = list(radio.read_packets(timeout=args.duration))
        ok(f"packets={len(packets)}")

        step("RX_STOP")
        radio.stop_rx()
        ok("RX_STOP ACK")

        if len(packets) < args.min_packets:
            fail(f"Expected at least {args.min_packets} packets, got {len(packets)}")
            return 7

        step("TX_RAW")
        radio.transmit(packet, power_dbm=args.power, timeout=args.tx_timeout)
        ok("TX_RAW ACK")

        step("GET_STATS")
        stats = radio.get_stats()
        ok(
            "stats: "
            f"rx_ok={stats.rx_ok} rx_crc_err={stats.rx_crc_err} "
            f"rx_drop={stats.rx_drop} rx_overflow={stats.rx_overflow}"
        )

        if is_ook and args.auto_reset:
            step("reset_device() after OOK")
            radio.reset_device()
            ok("reset_device() PASS")
        elif is_ook:
            ok("OOK preset validated without auto-reset; device may remain locked to OOK")

        print()
        ok("PROP PRESET SMOKE PASS")
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
