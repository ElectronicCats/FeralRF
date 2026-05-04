#!/usr/bin/env python3
"""F29 — Wi-SUN FAN 1.0 (915 MHz US ISM) capture stub.

Default preset: wisun_915_fsk_50k. Otros rates via --preset.
Loads preset, starts RX, prints first N bytes hex of each packet.
NO parse de stack (PHY-only). Ctrl-C para salir.
"""
import argparse
import sys

from feralrf import PHY, PROP_PRESETS, Radio, RxStreamError


def main() -> int:
    parser = argparse.ArgumentParser(description="F29 Wi-SUN scan stub")
    parser.add_argument("--port", required=True)
    parser.add_argument(
        "--preset",
        default="wisun_915_fsk_50k",
        choices=[n for n in PROP_PRESETS if n.startswith("wisun_")],
    )
    parser.add_argument("--baudrate", type=int, default=921600)
    parser.add_argument("--max-hex", type=int, default=32, help="Max bytes to hex-print")
    parser.add_argument("--duration", type=float, default=30.0)
    args = parser.parse_args()

    radio = Radio(port=args.port, baudrate=args.baudrate)
    try:
        radio.init()
        radio.set_phy(PHY.PROPRIETARY_GFSK, channel=0)
        radio.configure_prop(**PROP_PRESETS[args.preset])
        radio.start_rx()
        print(f"Scanning preset={args.preset}; Ctrl-C to stop")

        for pkt in radio.read_packets(timeout=args.duration):
            if isinstance(pkt, RxStreamError):
                continue
            head = pkt.data[: args.max_hex].hex()
            crc_flag = "OK" if pkt.crc_ok else "ER"
            print(f"[{crc_flag}] len={len(pkt.data)} rssi={pkt.rssi_dbm} {head}")
    except KeyboardInterrupt:
        pass
    finally:
        radio.disconnect()
    return 0


if __name__ == "__main__":
    sys.exit(main())
