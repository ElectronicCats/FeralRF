#!/usr/bin/env python3
"""
FeralRF - KillerBee sniff example (IEEE 802.15.4)

Requires: pip install feralrf[killerbee]  (installs the optional `killerbee`
dependency; see docs/PYTHON_API.md, section "KillerBee integration").
Requires: real FeralRF/CatSniffer hardware connected over USB.

Uses feralrf.integrations.killerbee.KillerBeeFeralRF directly, i.e. the same
adapter a KillerBee-side `dev_feralcat.py` shim would construct for tools
like zbdump/zbwireshark/zbstumbler. Sniffs raw IEEE 802.15.4 frames on a
single channel and prints bytes/validcrc/rssi for each `pnext()` result.
Optionally writes a Wireshark-loadable pcap (DLT_IEEE802_15_4).
"""

import argparse
import struct
import sys
import time
from typing import Optional

from feralrf.integrations.killerbee import KillerBeeFeralRF

DLT_IEEE802_15_4 = 195


def step(title: str) -> None:
    print(f"[STEP] {title}")


def ok(msg: str) -> None:
    print(f"[ OK ] {msg}")


def fail(msg: str) -> None:
    print(f"[FAIL] {msg}")


class PcapWriter:
    """Minimal classic-pcap writer for DLT_IEEE802_15_4 (stdlib `struct` only)."""

    _GLOBAL_HEADER = struct.Struct("<IHHiIII")
    _RECORD_HEADER = struct.Struct("<IIII")

    def __init__(self, path: str):
        self._fh = open(path, "wb")
        self._fh.write(
            self._GLOBAL_HEADER.pack(
                0xA1B2C3D4,  # magic number (native byte order, microsecond ts)
                2,
                4,  # version major, minor
                0,
                0,  # thiszone, sigfigs
                65535,  # snaplen
                DLT_IEEE802_15_4,
            )
        )

    def write(self, data: bytes) -> None:
        now = time.time()
        ts_sec = int(now)
        ts_usec = int((now - ts_sec) * 1_000_000)
        n = len(data)
        self._fh.write(self._RECORD_HEADER.pack(ts_sec, ts_usec, n, n))
        self._fh.write(data)

    def close(self) -> None:
        self._fh.close()


def main() -> int:
    parser = argparse.ArgumentParser(description="FeralRF KillerBee sniff example (IEEE 802.15.4)")
    parser.add_argument("--port", "-p", help="Serial port (auto-detect if omitted)", default=None)
    parser.add_argument(
        "--channel",
        "-c",
        help="IEEE 802.15.4 channel (11..26)",
        type=int,
        default=11,
    )
    parser.add_argument(
        "--count",
        "-n",
        help="Stop after N packets (0 = run until Ctrl-C)",
        type=int,
        default=0,
    )
    parser.add_argument(
        "--timeout",
        "-t",
        help="pnext() poll timeout in milliseconds",
        type=int,
        default=100,
    )
    parser.add_argument(
        "--pcap",
        help="Optional output path for a Wireshark-loadable pcap (DLT_IEEE802_15_4)",
        default=None,
    )
    args = parser.parse_args()

    print("FeralRF KillerBee Sniff Example")
    print("================================")
    print(
        f"port={args.port or 'auto'} channel={args.channel} "
        f"count={args.count or 'inf'} pcap={args.pcap or 'none'}"
    )
    print()

    kb: Optional[KillerBeeFeralRF] = None
    pcap: Optional[PcapWriter] = None
    count = 0

    try:
        step("Construct KillerBeeFeralRF + sniffer_on")
        kb = KillerBeeFeralRF(dev=args.port)
        kb.sniffer_on(args.channel)
        ok(f"dev_info={kb.get_dev_info()} capabilities={kb.get_capabilities()}")
        ok(f"sniffing on channel {args.channel}")

        if args.pcap:
            pcap = PcapWriter(args.pcap)
            ok(f"writing pcap to {args.pcap}")

        step("Read packets via pnext() (Ctrl-C to stop)")
        while args.count == 0 or count < args.count:
            pkt = kb.pnext(timeout=args.timeout)
            if pkt is None:
                continue
            count += 1
            print(
                f"[{count}] bytes={pkt['bytes'].hex()} "
                f"validcrc={pkt['validcrc']} rssi={pkt['rssi']}"
            )
            if pcap is not None:
                pcap.write(pkt["bytes"])

        print()
        ok(f"SNIFF DONE packets={count}")
        return 0

    except KeyboardInterrupt:
        print()
        ok(f"stopped by user, packets={count}")
        return 0
    except Exception as exc:
        fail(f"Unexpected error: {exc}")
        return 6
    finally:
        if pcap is not None:
            pcap.close()
        if kb is not None:
            kb.sniffer_off()
            kb.close()


if __name__ == "__main__":
    sys.exit(main())
