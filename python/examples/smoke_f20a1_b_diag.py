#!/usr/bin/env python3
"""F20.a.1.b — Smoke V2 NOSYNC diagnostic.

Runs the F20.a.1 smoke (peripheral advertise + central connect), waits for
disconnect, then queries both boards for diagnostic dumps and diffs them.

Pass criteria:
  1. All compared fields match (accessAddr, crcInit, hopIncrement,
     winOffset, hopInterval, supervTimeout).
  2. At least one slave ring entry has nRxOk > 0 (slave actually received
     a packet from master).
  3. GATT discovery completes (services >= 2, name == FERAL_GATT,
     test == HELLO_FERAL).

Run:
    python smoke_f20a1_b_diag.py --peripheral-port /dev/ttyACM0 \\
        --central-port /dev/ttyACM3
"""
import argparse
import re
import sys
import time
from threading import Thread

import serial

from feralrf.radio import Radio


def reset_cc1352(port: str) -> None:
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


def run_peripheral(port: str, baud: int, target_addr: str, count: int) -> None:
    radio = Radio(port=port, baudrate=baud)
    try:
        radio.init()
        radio.serve_gatt()
        radio.advertise_ind(
            payload=b"\x02\x01\x06",
            scan_resp_data=b"FERAL_GATT_SR",
            target_addr=target_addr,
            count=count,
            interval_us=10000,
        )
    finally:
        radio.disconnect()


def run_central_attempt(port: str, baud: int, target_addr: str) -> dict:
    addr_le = bytes(int(p, 16) for p in reversed(target_addr.split(":")))
    radio = Radio(port=port, baudrate=baud)
    out = {
        "services_count": 0,
        "chars_count": 0,
        "name_val": b"",
        "test_val": b"",
        "connected": False,
    }
    try:
        radio.init()
        radio.reset_device()
        radio.init()
        result = radio.ble_connect(addr_le, addr_type=1, timeout=10.0)
        if not result.is_ok:
            return out
        out["connected"] = True
        try:
            services = radio.gatt_discover(timeout=10.0)
            out["services_count"] = len(services.services)
            out["chars_count"] = len(services.characteristics)
            try:
                out["name_val"] = radio.gatt_read(handle=3, timeout=5.0)
            except Exception:
                pass
            try:
                out["test_val"] = radio.gatt_read(handle=6, timeout=5.0)
            except Exception:
                pass
        except Exception:
            pass
        try:
            radio.ble_disconnect(timeout=2.0)
        except Exception:
            pass
        return out
    finally:
        radio.disconnect()


def query_diagnostics(per_port: str, cen_port: str, baud: int):
    """After disconnect, query both boards for their diagnostic dumps.
    Must call connect() — Radio() ctor doesn't open the port."""
    per = Radio(port=per_port, baudrate=baud)
    cen = Radio(port=cen_port, baudrate=baud)
    try:
        per.connect()
        cen.connect()
        slave_dump = per.debug_slave()
        central_dump = cen.debug_conn_params()
        return slave_dump, central_dump
    finally:
        per.disconnect()
        cen.disconnect()


def diff_table(slave, central):
    """Compare slave-parsed (SlaveDbgResult) vs central-actual
    (DebugConnParamsResponse) values. Returns (all_match, lines)."""
    fields = [
        (
            "accessAddr",
            f"0x{slave.access_addr:08X}",
            f"0x{central.access_addr:08X}",
            slave.access_addr == central.access_addr,
        ),
        (
            "crcInit",
            f"0x{slave.crc_init:06X}",
            f"0x{central.crc_init:06X}",
            slave.crc_init == central.crc_init,
        ),
        (
            "hopIncrement",
            str(slave.hop_increment),
            str(central.hop_increment),
            slave.hop_increment == central.hop_increment,
        ),
        (
            "winOffset",
            str(slave.win_offset),
            str(central.win_offset),
            slave.win_offset == central.win_offset,
        ),
        (
            "hopInterval",
            str(slave.hop_interval),
            str(central.conn_interval),
            slave.hop_interval == central.conn_interval,
        ),
        (
            "supervTimeout",
            str(slave.superv_timeout),
            str(central.superv_timeout),
            slave.superv_timeout == central.superv_timeout,
        ),
    ]
    lines = [f"{'Field':<16} {'Slave':<16} {'Central':<16} Match"]
    lines.append("-" * 60)
    all_match = True
    for name, s, c, ok in fields:
        mark = "OK" if ok else "MISMATCH"
        lines.append(f"{name:<16} {s:<16} {c:<16} {mark}")
        if not ok:
            all_match = False
    return all_match, lines


def main() -> int:
    parser = argparse.ArgumentParser(description="F20.a.1.b smoke V2 NOSYNC diagnostic")
    parser.add_argument("--peripheral-port", required=True)
    parser.add_argument("--central-port", required=True)
    parser.add_argument("--baudrate", type=int, default=921600)
    parser.add_argument("--target-mac", default="DE:AD:BE:EF:CA:FE")
    parser.add_argument(
        "--peripheral-count",
        type=int,
        default=5000,
        help="ADV iterations on peripheral (default ~50s buffer).",
    )
    args = parser.parse_args()

    print("F20.a.1.b smoke V2 - NOSYNC diagnostic")
    print(f"Peripheral={args.peripheral_port} Central={args.central_port}")
    print(f"Target MAC={args.target_mac}")
    print("=" * 60)

    reset_cc1352(args.peripheral_port)
    reset_cc1352(args.central_port)

    per_thread = Thread(
        target=run_peripheral,
        args=(
            args.peripheral_port,
            args.baudrate,
            args.target_mac,
            args.peripheral_count,
        ),
        daemon=True,
    )
    per_thread.start()
    time.sleep(2.0)

    cen_result = run_central_attempt(args.central_port, args.baudrate, args.target_mac)
    per_thread.join(timeout=10.0)

    print("\n--- Central run ---")
    print(f"connected: {cen_result['connected']}")
    print(f"services discovered: {cen_result['services_count']}")
    print(f"chars discovered: {cen_result['chars_count']}")
    print(f"device name: {cen_result['name_val']!r}")
    print(f"test value: {cen_result['test_val']!r}")

    print("\n--- Querying diagnostic dumps ---")
    try:
        slave, central = query_diagnostics(args.peripheral_port, args.central_port, args.baudrate)
    except Exception as e:
        print(f"diagnostic query FAILED: {e!r}")
        return 1

    print("\n--- Field diff (slave parsed vs central actual) ---")
    all_match, lines = diff_table(slave, central)
    for line in lines:
        print(line)

    print("\n--- Slave RX ring (oldest first) ---")
    if not slave.entries:
        print("  (empty - startSlave never ran or pollSlave never completed an event)")
    else:
        print(
            f"{'evt':>4} {'chan':>4} {'anchor':>10} {'actual':>10} "
            f"{'us':>6} {'status':>6} {'nRxOk':>5} {'nRxNok':>6} "
            f"{'nRxIgn':>7} {'pktSt':>5}"
        )
        for entry in slave.entries:
            delta_us = (entry.actual_start_rat - entry.anchor_rat) // 4
            print(
                f"{entry.event_counter:>4} {entry.chan:>4} 0x{entry.anchor_rat:08x} "
                f"0x{entry.actual_start_rat:08x} {delta_us:>6} 0x{entry.status:04x} "
                f"{entry.n_rx_ok:>5} {entry.n_rx_nok:>6} {entry.n_rx_ignored:>7} "
                f"0x{entry.pkt_status:02x}"
            )

    any_rx = any(entry.n_rx_ok > 0 for entry in slave.entries)
    gatt_pass = (
        cen_result["services_count"] >= 2
        and cen_result["name_val"] == b"FERAL_GATT"
        and cen_result["test_val"] == b"HELLO_FERAL"
    )

    print("\n" + "=" * 60)
    print(f"[{'PASS' if all_match else 'FAIL'}] all parsed fields match central actuals")
    print(f"[{'PASS' if any_rx else 'FAIL'}] slave received >=1 packet from master (any nRxOk>0)")
    print(f"[{'PASS' if gatt_pass else 'FAIL'}] GATT path: services>=2, name+test correct")

    overall = all_match and any_rx and gatt_pass
    print(f"\n[{'PASS' if overall else 'FAIL'}] Smoke V2 overall")
    return 0 if overall else 1


if __name__ == "__main__":
    sys.exit(main())
