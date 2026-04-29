#!/usr/bin/env python3
"""F12 demo — passive vs active BLE scan, prints the delta active adds.

Auto-validates the F12 closure criterion: ≥1 device contributes scan_rsp
content (name completion or UUIDs/mfg) not present in passive ADV alone.

Usage:
    python demo_ble_scan_active.py [port] [--json out.json] [--duration 5]
"""

import argparse
import json
import sys
import time
import warnings
from typing import Dict

from feralrf import PHY, Radio
from feralrf._ble_scan import BleScanResult, extract_pdu_header

warnings.simplefilter("ignore")


def passive_scan(radio: Radio, duration: float) -> Dict[str, BleScanResult]:
    """Passive scan — no SCAN_REQ. Same per-MAC merge as active, just no scan_rsps."""
    radio.set_ble_scan_mode(active=False)
    radio.set_adv_hop(True)
    radio.set_phy(PHY.BLE_1M, channel=37)
    radio.start_rx()
    results: Dict[str, BleScanResult] = {}
    try:
        for pkt in radio.read_packets(timeout=duration):
            if not pkt.crc_ok or len(pkt.data) < 8 or pkt.ll_pdu_type is None:
                continue
            if pkt.ll_pdu_type not in (0x00, 0x01, 0x02, 0x06, 0x07):
                continue
            mac, addr_type = extract_pdu_header(pkt.data)
            if mac is None:
                continue
            r = results.setdefault(mac, BleScanResult(mac=mac, addr_type=addr_type))
            r.update_from_packet(pkt)
    finally:
        radio.stop_rx()
        radio.set_adv_hop(False)
    return results


def print_table(title: str, results: Dict[str, BleScanResult], show_rsp: bool):
    print(f"\n=== {title} ===")
    if not results:
        print("  (no devices)")
        return
    header = f"{'MAC':<18} {'name':<22} {'rssi':>5} {'adv':>4}"
    if show_rsp:
        header += f" {'rsp':>4}"
    header += f" {'uuids':>5} {'mfg':>4}"
    print(header)
    print("-" * len(header))
    rows = sorted(results.values(), key=lambda r: -r.rssi_max)
    for r in rows:
        name = (r.name or "(no name)")[:22]
        n_uuids = len(r.uuids_16bit) + len(r.uuids_128bit)
        n_mfg = len(r.manufacturer_data)
        line = f"{r.mac:<18} {name:<22} {r.rssi_max:>5d} {r.adv_count:>4d}"
        if show_rsp:
            line += f" {r.scan_rsp_count:>4d}"
        line += f" {n_uuids:>5d} {n_mfg:>4d}"
        print(line)


def diff_passive_vs_active(passive: Dict[str, BleScanResult], active: Dict[str, BleScanResult]):
    print("\n=== diff: what active adds (per device seen in both) ===")
    closure_pass = False
    common = set(passive.keys()) & set(active.keys())
    if not common:
        print("  (no devices in both — cannot diff)")
        return False

    for mac in sorted(common):
        p = passive[mac]
        a = active[mac]
        notes = []
        if a.name and p.name != a.name:
            notes.append(f"name '{p.name or 'empty'}' -> '{a.name}' *")
            closure_pass = True
        new_uuids16 = [u for u in a.uuids_16bit if u not in p.uuids_16bit]
        new_uuids128 = [u for u in a.uuids_128bit if u not in p.uuids_128bit]
        new_mfg = [hex(c) for c in a.manufacturer_data if c not in p.manufacturer_data]
        new_svc = [u for u in a.services_uuid16_data if u not in p.services_uuid16_data]
        if new_uuids16:
            notes.append(f"UUIDs16 +{len(new_uuids16)} {new_uuids16} *")
            closure_pass = True
        if new_uuids128:
            notes.append(f"UUIDs128 +{len(new_uuids128)} *")
            closure_pass = True
        if new_mfg:
            notes.append(f"mfg companies +{len(new_mfg)} {new_mfg} *")
            closure_pass = True
        if new_svc:
            notes.append(f"service-data +{len(new_svc)} *")
            closure_pass = True
        if notes:
            print(f"  {mac} '{a.name or '?'}'")
            for n in notes:
                print(f"      {n}")
    return closure_pass


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("port", nargs="?", default="/dev/ttyACM0")
    p.add_argument("--json", dest="json_path")
    p.add_argument("--duration", type=float, default=5.0)
    args = p.parse_args()

    radio = Radio(args.port)
    radio.connect()
    time.sleep(0.3)
    radio.init()

    try:
        print(f"[1/3] passive scan {args.duration}s on {args.port}")
        passive = passive_scan(radio, args.duration)
        print_table("passive", passive, show_rsp=False)

        print(f"\n[2/3] active scan {args.duration}s on {args.port}")
        active = radio.scan_ble_active(duration=args.duration)
        print_table("active", active, show_rsp=True)

        print("\n[3/3] computing diff")
        closure = diff_passive_vs_active(passive, active)

        n_p = len(passive)
        n_a = len(active)
        delta_uuids = sum(len(a.uuids_16bit) + len(a.uuids_128bit) for a in active.values()) - sum(
            len(p.uuids_16bit) + len(p.uuids_128bit) for p in passive.values()
        )
        print(
            f"\nSUMMARY  passive: {n_p} devices  active: {n_a} devices  delta_uuids={delta_uuids}"
        )

        if closure:
            print("F12 closure: PASS — at least 1 device contributed scan_rsp content not in adv")
        else:
            print(
                "F12 closure: SKIP — no scannable peripheral in range. "
                "Bring an ESP32/phone/smart-bulb closer and re-run."
            )

        if args.json_path:
            out = {
                "passive": {mac: res.to_dict() for mac, res in passive.items()},
                "active": {mac: res.to_dict() for mac, res in active.items()},
                "closure_pass": closure,
            }
            with open(args.json_path, "w") as f:
                json.dump(out, f, indent=2)
            print(f"\nFull results dumped to {args.json_path}")
    finally:
        radio.disconnect()
    return 0


if __name__ == "__main__":
    sys.exit(main())
