#!/usr/bin/env python3
"""
FeralRF - IEEE 802.15.4 channel sweep (PHY 4)
"""

import argparse
import sys
import time
from dataclasses import dataclass
from typing import Optional

from feralrf import PHY, Radio
from feralrf.exceptions import ConnectionError, FeralRFError, TimeoutError


@dataclass
class SweepResult:
    channel: int
    packets: int
    delta_ok: Optional[int]
    delta_crc_err: Optional[int]
    delta_drop: Optional[int]
    delta_ovf: Optional[int]
    error: Optional[str]


def main() -> int:
    parser = argparse.ArgumentParser(description="FeralRF IEEE 802.15.4 channel sweep")
    parser.add_argument("--port", "-p", help="Serial port (auto-detect if omitted)", default=None)
    parser.add_argument("--baudrate", "-b", help="UART baudrate", type=int, default=921600)
    parser.add_argument("--ch-min", help="First channel (11..26)", type=int, default=11)
    parser.add_argument("--ch-max", help="Last channel (11..26)", type=int, default=26)
    parser.add_argument(
        "--duration",
        "-d",
        help="Capture window per channel in seconds",
        type=float,
        default=5.0,
    )
    parser.add_argument(
        "--retries",
        help="Retries per channel on timeout/protocol error",
        type=int,
        default=2,
    )
    parser.add_argument(
        "--settle",
        help="Settle delay after SET_CHANNEL in seconds",
        type=float,
        default=0.15,
    )
    args = parser.parse_args()

    ch_min = max(11, min(26, args.ch_min))
    ch_max = max(11, min(26, args.ch_max))
    if ch_min > ch_max:
        ch_min, ch_max = ch_max, ch_min

    print("FeralRF IEEE 802.15.4 Channel Sweep")
    print("===================================")
    print(
        f"port={args.port or 'auto'} baudrate={args.baudrate} phy={int(PHY.IEEE_802_15_4)} "
        f"channels={ch_min}..{ch_max} duration={args.duration}s retries={args.retries} settle={args.settle}s"
    )
    print()

    results: list[SweepResult] = []

    for channel in range(ch_min, ch_max + 1):
        print(f"[CH {channel:02d}]")
        result = SweepResult(
            channel=channel,
            packets=0,
            delta_ok=None,
            delta_crc_err=None,
            delta_drop=None,
            delta_ovf=None,
            error=None,
        )

        for attempt in range(1, args.retries + 1):
            radio = Radio(port=args.port, baudrate=args.baudrate)
            try:
                info = radio.init()
                base = radio.get_stats(timeout=2.0)

                radio.set_phy(PHY.IEEE_802_15_4, channel)
                radio.set_channel(channel)
                if args.settle > 0:
                    time.sleep(args.settle)

                radio.start_rx()
                packets = list(radio.read_packets(timeout=args.duration))
                radio.stop_rx()

                end = radio.get_stats(timeout=2.0)

                result.packets = len(packets)
                result.delta_ok = end.rx_ok - base.rx_ok
                result.delta_crc_err = end.rx_crc_err - base.rx_crc_err
                result.delta_drop = end.rx_drop - base.rx_drop
                result.delta_ovf = end.rx_overflow - base.rx_overflow
                result.error = None

                print(
                    "  "
                    f"ok fw={info.firmware_version} packets={result.packets} "
                    f"delta(ok={result.delta_ok},crc_err={result.delta_crc_err},"
                    f"drop={result.delta_drop},ovf={result.delta_ovf})"
                )
                break
            except (TimeoutError, FeralRFError, ConnectionError) as exc:
                result.error = str(exc)
                print(f"  warn attempt {attempt}/{args.retries}: {exc}")
                if attempt == args.retries:
                    print("  fail")
            except Exception as exc:
                result.error = f"Unexpected error: {exc}"
                print(f"  fail {result.error}")
                break
            finally:
                radio.disconnect()

        results.append(result)

    print()
    print("Summary")
    print("=======")
    detected = [r for r in results if r.packets > 0 or (r.delta_ok and r.delta_ok > 0)]
    for r in results:
        if r.error:
            print(f"ch {r.channel:02d}: ERROR {r.error}")
        else:
            print(
                f"ch {r.channel:02d}: packets={r.packets} "
                f"delta_ok={r.delta_ok} delta_crc_err={r.delta_crc_err} "
                f"delta_drop={r.delta_drop} delta_ovf={r.delta_ovf}"
            )

    print()
    if detected:
        detected_channels = ", ".join(str(r.channel) for r in detected)
        print(f"[ OK ] Detected activity on channel(s): {detected_channels}")
        return 0

    print("[WARN] No activity detected on scanned channels")
    return 2


if __name__ == "__main__":
    sys.exit(main())
