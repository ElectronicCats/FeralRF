#!/usr/bin/env python3
"""F29 — Amazon Sidewalk Sub-G FSK (915 MHz US ISM) capture stub.

Default preset: sidewalk_915_fsk_50k. Otros (250k) via --preset.
Loads preset, starts RX, prints first N bytes hex of each packet.
NO parse de stack Sidewalk (PHY-only — auth + framing son layer superior
no soportado aquí). Ctrl-C para salir.

Sidewalk LR (LoRa-like) NO soportado en CC1352 — usa SX1262 vía Cat-LoRa.
"""
import argparse
import sys

from feralrf import PHY, PROP_PRESETS, Radio, RxStreamError


def main() -> int:
    parser = argparse.ArgumentParser(description="F29 Sidewalk Sub-G capture stub")
    parser.add_argument("--port", required=True)
    parser.add_argument(
        "--preset",
        default="sidewalk_915_fsk_50k",
        choices=[n for n in PROP_PRESETS if n.startswith("sidewalk_")],
    )
    parser.add_argument("--baudrate", type=int, default=921600)
    parser.add_argument("--max-hex", type=int, default=32)
    parser.add_argument("--duration", type=float, default=30.0)
    args = parser.parse_args()

    radio = Radio(port=args.port, baudrate=args.baudrate)
    try:
        radio.init()
        radio.set_phy(PHY.PROPRIETARY_GFSK, channel=0)
        radio.configure_prop(**PROP_PRESETS[args.preset])
        radio.start_rx()
        print(f"Capturing preset={args.preset}; Ctrl-C to stop")

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
