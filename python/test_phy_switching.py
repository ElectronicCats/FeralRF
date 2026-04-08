"""Test PHY switching WITHOUT reset — validates s_433_handle alias survives transitions.
Sequence: 433 → BLE → 868 → IEEE → 433 → 915 → 433
Each step does TX/RX with DEADBEEF validation."""

import threading
import time

from feralrf import PHY, Radio

TX_PORT = "/dev/ttyACM3"
RX_PORT = "/dev/ttyACM0"
COUNT = 5

SEQUENCE = [
    ("GFSK 433", PHY.PROPRIETARY_GFSK, 0, 433920000),
    ("BLE 1M", PHY.BLE_1M, 37, 0),
    ("Sub-1GHz 868", PHY.SUB_1GHZ_868, 0, 868000000),
    ("IEEE 802.15.4", PHY.IEEE_802_15_4, 15, 0),
    ("GFSK 433 (2nd)", PHY.PROPRIETARY_GFSK, 0, 433920000),
    ("Sub-1GHz 915", PHY.SUB_1GHZ_915, 0, 902000000),
    ("GFSK 433 (3rd)", PHY.PROPRIETARY_GFSK, 0, 433920000),
]


def do_txrx(tx_radio, rx_radio, name, phy, channel, freq_hz):
    """Switch both radios to PHY, TX/RX COUNT packets, return (tx_ok, rx_count)."""
    rx_data = []
    rx_err = [None]

    def rx_thread():
        try:
            rx_radio.set_phy(phy, channel=channel, frequency_hz=freq_hz)
            time.sleep(0.3)
            rx_radio.start_rx()
            start = time.time()
            while time.time() - start < (COUNT * 1.0) + 4:
                for p in rx_radio.read_packets(timeout=1.0):
                    if b"\xDE\xAD\xBE\xEF" in p.data:
                        rx_data.append(p.data)
            rx_radio.stop_rx()
        except Exception as e:
            rx_err[0] = str(e)
            try:
                rx_radio.stop_rx()
            except Exception:
                pass

    t = threading.Thread(target=rx_thread, daemon=True)
    t.start()
    time.sleep(2)

    tx_radio.set_phy(phy, channel=channel, frequency_hz=freq_hz)
    time.sleep(0.3)
    tx_radio.set_power(5)

    pkt = bytearray(30)
    pkt[2] = 0xDE
    pkt[3] = 0xAD
    pkt[4] = 0xBE
    pkt[5] = 0xEF
    for k in range(6, 30):
        pkt[k] = k

    tx_ok = 0
    for i in range(COUNT):
        pkt[0] = (i >> 8) & 0xFF
        pkt[1] = i & 0xFF
        try:
            tx_radio.transmit(bytes(pkt))
            tx_ok += 1
        except Exception:
            pass
        time.sleep(1.0)

    t.join(timeout=10)
    return tx_ok, len(rx_data), rx_err[0]


def main():
    # Connect once, no reset between PHYs
    tx = Radio(TX_PORT)
    tx.connect()
    time.sleep(0.5)
    tx.init()

    rx = Radio(RX_PORT)
    rx.connect()
    time.sleep(0.5)
    rx.init()

    results = []
    for name, phy, ch, freq in SEQUENCE:
        print(f"\n--- {name} ---")
        tx_ok, rx_count, err = do_txrx(tx, rx, name, phy, ch, freq)
        thresh = max(1, int(COUNT * 0.8))
        if err:
            status = f"ERROR: {err}"
        elif tx_ok < thresh:
            status = f"FAIL (TX {tx_ok}/{COUNT})"
        elif rx_count >= thresh:
            status = "PASS"
        else:
            status = "FAIL"
        print(f"  TX {tx_ok}/{COUNT}, RX {rx_count}/{COUNT} (DEADBEEF) [{status}]")
        results.append((name, tx_ok, rx_count, status))

    tx.disconnect()
    rx.disconnect()

    print(f"\n{'='*55}")
    print("PHY SWITCHING SUMMARY (no reset between steps)")
    print(f"{'='*55}")
    for name, tx_ok, rx_count, status in results:
        print(f"  {name:20s}: TX {tx_ok}/{COUNT}, RX {rx_count}/{COUNT} [{status}]")


if __name__ == "__main__":
    main()
