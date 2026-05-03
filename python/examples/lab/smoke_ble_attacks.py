#!/usr/bin/env python3
"""Wire-level smoke for BLE attacks (no human in the loop).

For each attack, the TX board emits a short burst and the RX board
captures advertising on channel 37. The script checks that the emitted
PDUs match the attack's signature byte pattern, not that a phone shows
a popup. Phone validation is the F11b human checkpoint.

Usage:
    python smoke_ble_attacks.py --tx-port /dev/ttyACM8 --rx-port /dev/ttyACM5 \\
        --attack beacon_flood

Available attacks: beacon_flood, apple_popup_spam, google_popup_spam,
                   adv_spoof, capture_and_replay
"""

import argparse
import sys
import threading
import time

import serial


def reset_one(port):
    import re

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


def rx_collect(port, duration, results):
    """Run on a thread: scan BLE 1M ch 37 for `duration` seconds, append packets."""
    from feralrf import PHY, Radio, RxStreamError

    rx = Radio(port=port)
    try:
        rx.connect()
        time.sleep(0.3)
        rx.init()
        rx.set_phy(PHY.BLE_1M, channel=37)
        rx.start_rx()
        time.sleep(0.3)
        deadline = time.time() + duration
        while time.time() < deadline:
            for pkt in rx.read_packets(timeout=0.5):
                # F8f #7b: read_packets now also yields RxStreamError; skip those.
                if isinstance(pkt, RxStreamError):
                    continue
                results.append(pkt)
        rx.stop_rx()
    finally:
        rx.disconnect()


def smoke(args):
    from feralrf import Radio
    from feralrf.attacks import ble

    print("[STEP] reset both")
    reset_one(args.tx_port)
    reset_one(args.rx_port)

    print(f"[STEP] start RX on {args.rx_port} (ch 37, BLE 1M, {args.duration}s)")
    rx_packets = []
    rx_thread = threading.Thread(target=rx_collect, args=(args.rx_port, args.duration, rx_packets))
    rx_thread.start()
    # Give RX a head start so it's listening when TX begins
    time.sleep(2.0)

    print(f"[STEP] run attack {args.attack} on {args.tx_port}")
    tx = Radio(port=args.tx_port)
    tx.connect()
    time.sleep(0.3)
    tx.init()

    if args.attack == "beacon_flood":
        result = ble.beacon_flood(
            tx, names=["AirPods", "Free WiFi", "Tile"], count=8, channels=[37], power_dbm=0
        )
    elif args.attack == "apple_popup_spam":
        result = ble.apple_popup_spam(
            tx, device="airpods_pro", count=20, channels=[37], power_dbm=0
        )
    elif args.attack == "google_popup_spam":
        result = ble.google_popup_spam(
            tx, device="pixel_buds_pro", count=20, channels=[37], power_dbm=0
        )
    elif args.attack == "adv_spoof":
        adv_data = ble.build_adv_payload("SpoofedDevice")
        result = ble.adv_spoof(
            tx,
            target_addr="DE:AD:BE:EF:CA:FE",
            adv_data=adv_data,
            count=20,
            channel=37,
            power_dbm=0,
        )
    elif args.attack == "capture_and_replay":
        # Use a shorter capture+replay window
        result = ble.capture_and_replay(
            tx,
            capture_seconds=3.0,
            replay_count=3,
            channel=37,
            power_dbm=0,
        )
    else:
        raise SystemExit(f"unknown attack: {args.attack}")

    tx.disconnect()
    rx_thread.join()

    print(f"[INFO] attack returned: {result}")
    print(f"[INFO] RX captured {len(rx_packets)} packets total")

    # Per-attack signature verification
    matched = 0
    macs = set()
    for pkt in rx_packets:
        if not pkt.crc_ok or len(pkt.data) < 8:
            continue
        mac = pkt.data[2:8]
        macs.add(bytes(mac))
        ad = pkt.data[8:]

        if args.attack == "beacon_flood":
            # Look for Complete Local Name AD type 0x09 with our names
            for name in ["AirPods", "Free WiFi", "Tile"]:
                if name.encode() in ad:
                    matched += 1
                    break
        elif args.attack == "apple_popup_spam":
            # Apple company ID 0x004C + Proximity Pairing 0x07
            if b"\x4c\x00\x07" in ad:
                matched += 1
        elif args.attack == "google_popup_spam":
            # Service Data UUID 0xFE2C (little-endian: 0x2C 0xFE)
            if b"\x16\x2c\xfe" in ad:
                matched += 1
        elif args.attack == "adv_spoof":
            # Spoofed MAC: DE:AD:BE:EF:CA:FE → little-endian FE:CA:EF:BE:AD:DE
            if mac == bytes.fromhex("fecaefbeadde") and b"SpoofedDevice" in ad:
                matched += 1
        elif args.attack == "capture_and_replay":
            # Just count packets seen during window — more is better
            matched += 1

    print(f"[OTA ] attack={args.attack}: matched={matched} packets, distinct_macs={len(macs)}")

    # Pass criteria: we need to see at least N matches
    threshold = {
        "beacon_flood": 3,
        "apple_popup_spam": 3,
        "google_popup_spam": 3,
        "adv_spoof": 3,
        "capture_and_replay": 1,
    }[args.attack]

    if matched >= threshold:
        print(f"[ OK ] {args.attack} wire smoke PASS ({matched} >= {threshold})")
        return 0
    print(f"[FAIL] {args.attack} wire smoke FAIL ({matched} < {threshold})")
    return 1


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--tx-port", default="/dev/ttyACM8")
    p.add_argument("--rx-port", default="/dev/ttyACM5")
    p.add_argument(
        "--attack",
        required=True,
        choices=[
            "beacon_flood",
            "apple_popup_spam",
            "google_popup_spam",
            "adv_spoof",
            "capture_and_replay",
        ],
    )
    p.add_argument("--duration", type=float, default=10.0)
    args = p.parse_args()
    sys.exit(smoke(args))


if __name__ == "__main__":
    main()
