#!/usr/bin/env python3
"""
FeralRF - Phase 2 smoke test

Validates the minimal host <-> CC1352 command path:
RADIO_INIT -> GET_INFO -> SET_PHY -> SET_CHANNEL -> SET_POWER -> RX_START -> RX_STOP
"""

import argparse
import sys

from feralrf import PHY, Radio
from feralrf.exceptions import ConnectionError, FeralRFError, TimeoutError


def step(title: str) -> None:
    print(f"[STEP] {title}")


def ok(msg: str) -> None:
    print(f"[ OK ] {msg}")


def fail(msg: str) -> None:
    print(f"[FAIL] {msg}")


def main() -> int:
    parser = argparse.ArgumentParser(description="FeralRF Phase 2 smoke test")
    parser.add_argument("--port", "-p", help="Serial port (auto-detect if omitted)", default=None)
    parser.add_argument("--baudrate", "-b", help="UART baudrate", type=int, default=921600)
    parser.add_argument("--phy", help="PHY number", type=int, default=int(PHY.BLE_1M))
    parser.add_argument("--channel", "-c", help="RF channel", type=int, default=37)
    parser.add_argument("--power", help="TX power in dBm", type=int, default=0)
    args = parser.parse_args()

    print("FeralRF Phase 2 Smoke Test")
    print("==========================")
    print(f"port={args.port or 'auto'} baudrate={args.baudrate} phy={args.phy} channel={args.channel} power={args.power}")
    print()

    radio = Radio(port=args.port, baudrate=args.baudrate)

    try:
        step("Connect + RADIO_INIT + GET_INFO")
        info = radio.init()
        ok(
            f"INFO firmware={info.firmware_version} capabilities=0x{info.capabilities:02X} "
            f"serial={info.serial or 'n/a'}"
        )

        step("SET_PHY")
        radio.set_phy(PHY(args.phy), args.channel)
        ok("SET_PHY ACK")

        step("SET_CHANNEL")
        radio.set_channel(args.channel)
        ok("SET_CHANNEL ACK")

        step("SET_POWER")
        radio.set_power(args.power)
        ok("SET_POWER ACK")

        step("RX_START")
        radio.start_rx()
        ok("RX_START ACK")

        step("RX_STOP")
        radio.stop_rx()
        ok("RX_STOP ACK")

        print()
        ok("SMOKE TEST PASS")
        return 0

    except ValueError as exc:
        fail(f"Invalid argument: {exc}")
        return 2
    except ConnectionError as exc:
        fail(f"Connection error: {exc}")
        return 3
    except TimeoutError as exc:
        fail(f"Timeout waiting for response: {exc}")
        if args.port and args.port.endswith("ACM2"):
            print(
                "Hint: in CatSniffer RP2040 firmware on Linux, mapping is usually:\n"
                "  /dev/ttyACM0 = Cat-Bridge (CC1352 passthrough)\n"
                "  /dev/ttyACM1 = Cat-LoRa\n"
                "  /dev/ttyACM2 = Cat-Shell\n"
                "Try /dev/ttyACM0 for this smoke test."
            )
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
