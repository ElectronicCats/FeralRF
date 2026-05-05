#!/usr/bin/env python3
"""F21 — Smoke V1.b cross-validation 2-board.

For each PDU type, TX board emits via advertise_*, RX board captures raw
packets and inspects byte 0 (header byte, bits 3:0 = PDU type).

Pass criteria:
  - ADV_IND: >=10/20 packets with header & 0x0F == 0x0 + AdvA match
  - ADV_DIRECT_IND: >=10/20 with header & 0x0F == 0x1 + AdvA match
  - ADV_SCAN_IND: >=10/20 with header & 0x0F == 0x6 + AdvA match

Usage:
    python smoke_f21_advertise.py --tx-port /dev/ttyACM1 --rx-port /dev/ttyACM2
"""
import argparse
import re
import sys
import time

import serial

from feralrf import PHY, Radio, RxStreamError

ADV_IND_TYPE = 0x0
ADV_DIRECT_IND_TYPE = 0x1
ADV_SCAN_IND_TYPE = 0x6


def reset_cc1352(port: str) -> None:
    """Reset CC1352 via RP2040 shell port (data port + 2)."""
    m = re.search(r"(\d+)$", port)
    if not m:
        return
    shell = port[: m.start(1)] + str(int(m.group(1)) + 2)
    try:
        s = serial.Serial(shell, 115200, timeout=1.0, write_timeout=1.0)
        s.write(b"boot\r\n")
        time.sleep(0.5)
        s.write(b"exit\r\n")
        time.sleep(0.3)
        s.close()
    except Exception:
        pass
    time.sleep(3.5)


def run_pdu_type(tx_port, rx_port, baud, pdu_name, count):
    reset_cc1352(tx_port)
    reset_cc1352(rx_port)
    tx = Radio(port=tx_port, baudrate=baud)
    rx = Radio(port=rx_port, baudrate=baud)

    target_mac_str = "DE:AD:BE:EF:CA:FE"
    target_mac_le = bytes.fromhex("FECAEFBEADDE")

    try:
        tx.init()
        rx.init()
        rx.set_phy(PHY.BLE_1M, channel=37)
        rx.start_rx()
        time.sleep(0.3)

        if pdu_name == "ADV_IND":
            tx.advertise_ind(
                payload=b"\x02\x01\x06",
                scan_resp_data=b"FERAL_SCAN_RSP",
                target_addr=target_mac_str,
                count=count,
            )
            expected_type = ADV_IND_TYPE
        elif pdu_name == "ADV_DIRECT_IND":
            tx.advertise_direct(
                target_addr=target_mac_str,
                init_addr="11:22:33:44:55:66",
                mode="low",
                count=count,
            )
            expected_type = ADV_DIRECT_IND_TYPE
        elif pdu_name == "ADV_SCAN_IND":
            tx.advertise_scan_ind(
                payload=b"\x02\x01\x06",
                scan_resp_data=b"FERAL_SCAN_RSP",
                target_addr=target_mac_str,
                count=count,
            )
            expected_type = ADV_SCAN_IND_TYPE
        else:
            raise ValueError(f"unknown pdu_name: {pdu_name}")

        time.sleep(1.0)

        matched = 0
        total = 0
        for pkt in rx.read_packets(timeout=3.0):
            if isinstance(pkt, RxStreamError):
                continue
            total += 1
            if len(pkt.data) < 8:
                continue
            pdu_type = pkt.data[0] & 0x0F
            adv_addr = pkt.data[2:8]
            if pdu_type == expected_type and adv_addr == target_mac_le:
                matched += 1

        try:
            rx.stop_rx()
        except Exception:
            pass

        return matched, total
    finally:
        tx.disconnect()
        rx.disconnect()


def run_with_retry(fn, *args):
    last_exc = None
    for attempt in range(2):
        try:
            return fn(*args), None
        except Exception as exc:
            last_exc = exc
            if attempt == 0:
                print(f"  [WARN] {exc!r} — retry 1/2")
    return (0, 0), last_exc


def main() -> int:
    parser = argparse.ArgumentParser(description="F21 smoke V1.b")
    parser.add_argument("--tx-port", required=True)
    parser.add_argument("--rx-port", required=True)
    parser.add_argument("--baudrate", type=int, default=921600)
    parser.add_argument("--count", type=int, default=20)
    parser.add_argument("--threshold", type=int, default=10)
    args = parser.parse_args()

    print("F21 BLE Connectable Advertiser smoke V1.b")
    print(f"TX={args.tx_port} RX={args.rx_port} count={args.count} threshold>={args.threshold}")
    print("=" * 60)

    pdu_names = ("ADV_IND", "ADV_DIRECT_IND", "ADV_SCAN_IND")
    results = []
    for name in pdu_names:
        print(f"\n[ -- ] {name}")
        (matched, total), exc = run_with_retry(
            run_pdu_type, args.tx_port, args.rx_port, args.baudrate, name, args.count
        )
        if exc is not None:
            print(f"[FAIL] {name}: exception {exc!r}")
            results.append((name, 0, 0, False))
            continue
        passed = matched >= args.threshold
        status = "PASS" if passed else "FAIL"
        print(f"[{status}] {name}: matched={matched}/{args.count} total_rx={total}")
        results.append((name, matched, total, passed))

    print("\n" + "=" * 60)
    presets_passed = sum(r[3] for r in results)
    print(f"Aggregate: {presets_passed}/{len(results)} PDU types pass")
    return 0 if presets_passed == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
