#!/usr/bin/env python3
"""F29 — MIOTY TS-UNB (868 MHz EU SRD) capture stub.

Default preset: mioty_868_tsunb. Loads preset, starts RX, prints first N
bytes hex of each packet. NO parse de TS-UNB stack (PHY-only). Ctrl-C
para salir. WARNING: MIOTY preset marcado como "pending native support"
en F29.b (escape M3 wire-level smoke 0/10 — CC1352 likely needs custom
CPE patch). Este demo no captura nada hasta que F29.c cierre el caso.
"""
import argparse
import sys

from feralrf import PHY, PROP_PRESETS, Radio, RxStreamError


def main() -> int:
    parser = argparse.ArgumentParser(description="F29 MIOTY listen stub")
    parser.add_argument("--port", required=True)
    parser.add_argument(
        "--preset",
        default="mioty_868_tsunb",
        choices=[n for n in PROP_PRESETS if n.startswith("mioty_")],
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
        print(f"Listening preset={args.preset}; Ctrl-C to stop")

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
