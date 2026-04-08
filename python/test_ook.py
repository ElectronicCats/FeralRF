"""Test OOK TX/RX between two boards.
Usage: python test_ook.py [868|433]
OOK locks the radio — needs reset_device() to recover."""

import sys
import threading
import time

from feralrf import PHY, PROP_PRESETS, Radio

TX_PORT = "/dev/ttyACM3"
RX_PORT = "/dev/ttyACM0"
COUNT = 5


def main():
    freq = sys.argv[1] if len(sys.argv) > 1 else "868"
    preset_name = f"ook_{freq}_4k8"
    if preset_name not in PROP_PRESETS:
        print(f"Unknown preset: {preset_name}")
        print(f"Available: {[k for k in PROP_PRESETS if 'ook' in k]}")
        return
    preset = PROP_PRESETS[preset_name]
    print(f"Using preset: {preset_name} ({preset})")

    # RX
    rx_data = []
    rx_err = [None]

    def rx_thread():
        try:
            r = Radio(RX_PORT)
            r.connect()
            time.sleep(0.5)
            r.init()
            r.set_phy(PHY.PROPRIETARY_GFSK, frequency_hz=preset["frequency_hz"])
            time.sleep(0.3)
            r.configure_prop(**preset)
            time.sleep(0.3)
            r.start_rx()
            print("[RX] Listening OOK 868 MHz...")
            start = time.time()
            while time.time() - start < (COUNT * 1.5) + 5:
                for p in r.read_packets(timeout=1.0):
                    if b"\xDE\xAD\xBE\xEF" in p.data:
                        rx_data.append(p.data)
                        print(f"  [RX] #{len(rx_data)}: {p.data[:10].hex()}")
            r.stop_rx()
            print("[RX] Resetting device (OOK recovery)...")
            r.reset_device()
            r.disconnect()
        except Exception as e:
            rx_err[0] = str(e)

    t = threading.Thread(target=rx_thread, daemon=True)
    t.start()
    time.sleep(3)

    # TX
    tx = Radio(TX_PORT)
    tx.connect()
    time.sleep(0.5)
    tx.init()
    tx.set_phy(PHY.PROPRIETARY_GFSK, frequency_hz=preset["frequency_hz"])
    time.sleep(0.3)
    tx.configure_prop(**preset)
    time.sleep(0.3)
    tx.set_power(5)

    pkt = bytearray(20)
    pkt[0] = 0xDE
    pkt[1] = 0xAD
    pkt[2] = 0xBE
    pkt[3] = 0xEF
    for k in range(4, 20):
        pkt[k] = k

    print(f"[TX] Sending {COUNT} OOK packets at 868 MHz...")
    tx_ok = 0
    for i in range(COUNT):
        try:
            tx.transmit(bytes(pkt))
            tx_ok += 1
            print(f"  [TX] {i+1}/{COUNT} OK")
        except Exception as e:
            print(f"  [TX] {i+1}/{COUNT} FAIL: {e}")
        time.sleep(1.5)

    print("[TX] Resetting device (OOK recovery)...")
    tx.reset_device()
    tx.disconnect()

    t.join(timeout=15)

    err = rx_err[0]
    rx_count = len(rx_data)
    status = (
        "PASS" if rx_count >= max(1, int(COUNT * 0.8)) else ("ERROR: " + err if err else "FAIL")
    )
    print(f"\n=== OOK 868: TX {tx_ok}/{COUNT}, RX {rx_count}/{COUNT} (DEADBEEF) [{status}] ===")


if __name__ == "__main__":
    main()
