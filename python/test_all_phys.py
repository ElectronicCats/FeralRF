"""Test all PHYs — validates TX/RX with DEADBEEF markers between two CatSniffer boards.
Each PHY tested with fresh board reset. Explicit channel + frequency per PHY."""
import serial
import time
import threading
import sys
from feralrf import Radio, PHY

TX_PORT = "/dev/ttyACM3"  # Board 1
RX_PORT = "/dev/ttyACM0"  # Board 2
TX_SHELL = "/dev/ttyACM5"
RX_SHELL = "/dev/ttyACM2"
COUNT = 10

# PHY configs: (name, phy, channel, frequency_hz)
PHY_TESTS = [
    ("BLE 1M",         PHY.BLE_1M,            37, 0),
    ("IEEE 802.15.4",  PHY.IEEE_802_15_4,      15, 0),
    ("Sub-1GHz 868",   PHY.SUB_1GHZ_868,       0,  868000000),
    ("Sub-1GHz 915",   PHY.SUB_1GHZ_915,       0,  902000000),
    ("GFSK 868",       PHY.PROPRIETARY_GFSK,   0,  868000000),
    ("GFSK 433",       PHY.PROPRIETARY_GFSK,   0,  433920000),
]


def reset_boards():
    """Reset both boards via shell. Raises on failure."""
    for name, port in [("TX", TX_SHELL), ("RX", RX_SHELL)]:
        try:
            s = serial.Serial(port, 115200, timeout=2)
            s.write(b"boot\r\n"); time.sleep(0.5)
            s.write(b"exit\r\n"); time.sleep(0.5)
            resp = s.read(256)
            s.close()
            if b"PASSTHROUGH" not in resp:
                raise RuntimeError(f"{name} shell ({port}) did not confirm PASSTHROUGH")
        except serial.SerialException as e:
            raise RuntimeError(f"{name} shell ({port}) not accessible: {e}")
    time.sleep(3)


def test_phy(name, phy, channel, freq_hz, count=COUNT):
    rx_data = []
    rx_err = [None]

    def rx_thread():
        r = None
        try:
            r = Radio(RX_PORT)
            r.connect(); time.sleep(0.5); r.init()
            r.set_phy(phy, channel=channel, frequency_hz=freq_hz)
            time.sleep(0.3)
            r.start_rx()
            start = time.time()
            while time.time() - start < (count * 1.0) + 5:
                for p in r.read_packets(timeout=1.0):
                    if b"\xDE\xAD\xBE\xEF" in p.data:
                        rx_data.append(p.data)
        except Exception as e:
            rx_err[0] = str(e)
        finally:
            if r is not None:
                try:
                    r.stop_rx()
                except Exception:
                    pass
                try:
                    r.disconnect()
                except Exception:
                    pass

    t = threading.Thread(target=rx_thread, daemon=True)
    t.start()
    time.sleep(3)

    tx = Radio(TX_PORT)
    tx.connect(); time.sleep(0.5); tx.init()
    tx.set_phy(phy, channel=channel, frequency_hz=freq_hz)
    time.sleep(0.3)
    tx.set_power(5)

    pkt = bytearray(30)
    pkt[2] = 0xDE; pkt[3] = 0xAD; pkt[4] = 0xBE; pkt[5] = 0xEF
    for k in range(6, 30):
        pkt[k] = k

    tx_ok = 0
    for i in range(count):
        pkt[0] = (i >> 8) & 0xFF
        pkt[1] = i & 0xFF
        try:
            tx.transmit(bytes(pkt))
            tx_ok += 1
        except Exception:
            pass
        time.sleep(1.0)

    tx.disconnect()
    t.join(timeout=10)

    err = rx_err[0]
    rx_count = len(rx_data)
    rx_thresh = max(1, int(count * 0.8))
    tx_thresh = max(1, int(count * 0.8))

    if err:
        status = f"ERROR: {err}"
    elif tx_ok < tx_thresh:
        status = f"FAIL (TX only {tx_ok}/{count})"
    elif rx_count >= rx_thresh:
        status = "PASS"
    else:
        status = "FAIL"

    return name, tx_ok, rx_count, count, status


def main():
    tests = PHY_TESTS[:]
    if len(sys.argv) > 1:
        filt = sys.argv[1].lower()
        tests = [(n, p, c, f) for n, p, c, f in tests if filt in n.lower()]

    results = []
    for name, phy, ch, freq in tests:
        print(f"\n--- {name} (ch={ch}, freq={freq or 'default'}) ---")
        try:
            reset_boards()
        except RuntimeError as e:
            print(f"  RESET FAILED: {e}")
            results.append((name, 0, 0, COUNT, f"ERROR: reset failed"))
            continue
        r = test_phy(name, phy, ch, freq)
        print(f"  {r[0]:20s}: TX {r[1]}/{r[3]}, RX {r[2]}/{r[3]} (DEADBEEF) [{r[4]}]")
        results.append(r)

    print(f"\n{'='*55}")
    print("SUMMARY")
    print(f"{'='*55}")
    for name, tx, rx, cnt, status in results:
        print(f"  {name:20s}: TX {tx}/{cnt}, RX {rx}/{cnt} [{status}]")


if __name__ == "__main__":
    main()
