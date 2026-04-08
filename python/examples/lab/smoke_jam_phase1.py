#!/usr/bin/env python3
"""
FeralRF - JAM continuous smoke (phase 1)
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


def main() -> int:
    parser = argparse.ArgumentParser(description="FeralRF JAM continuous smoke (phase 1)")
    parser.add_argument("--port", "-p", help="Serial port (auto-detect if omitted)", default=None)
    parser.add_argument("--baudrate", "-b", help="UART baudrate", type=int, default=921600)
    parser.add_argument("--phy", help="PHY id (recommended 4 for phase 1)", type=int, default=4)
    parser.add_argument("--channel", "-c", help="RF channel", type=int, default=25)
    parser.add_argument("--power", help="TX power dBm", type=int, default=20)
    parser.add_argument(
        "--duration-ms",
        help="Jam duration in milliseconds (1..30000)",
        type=int,
        default=3000,
    )
    parser.add_argument(
        "--wait-ms",
        help="Wait after JAM_CONTINUOUS ACK before JAM_STOP (ms)",
        type=int,
        default=500,
    )
    parser.add_argument(
        "--timeout",
        help="Timeout waiting for command ACKs (seconds)",
        type=float,
        default=5.0,
    )
    args = parser.parse_args()

    if args.duration_ms <= 0 or args.duration_ms > 30000:
        fail("duration-ms must be in range 1..30000")
        return 2
    if args.wait_ms < 0:
        fail("wait-ms must be >= 0")
        return 2

    print("FeralRF JAM Continuous Smoke Test (Phase 1)")
    print("============================================")
    print(
        f"port={args.port or 'auto'} baudrate={args.baudrate} phy={args.phy} "
        f"channel={args.channel} power={args.power} duration_ms={args.duration_ms}"
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

        step("JAM_CONTINUOUS")
        radio.start_jam(
            channel=args.channel,
            power_dbm=args.power,
            duration_ms=args.duration_ms,
            timeout=args.timeout,
        )
        ok("JAM_CONTINUOUS ACK")

        if args.wait_ms > 0:
            time.sleep(args.wait_ms / 1000.0)

        step("JAM_STOP")
        radio.stop_jam(timeout=args.timeout)
        ok("JAM_STOP ACK")

        print()
        ok("JAM SMOKE PASS")
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
        try:
            radio.stop_jam(timeout=0.5)
        except Exception:
            pass
        radio.disconnect()


if __name__ == "__main__":
    sys.exit(main())
