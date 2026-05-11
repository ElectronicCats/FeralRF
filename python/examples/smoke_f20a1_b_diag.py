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


def _interpret_ble_status(status: int) -> str:
    """Map a TI BLE mailbox status word to a human-readable label.

    Codes from firmware/sdk/.../driverlib/rf_ble_mailbox.h (BLE_DONE_*, BLE_ERROR_*).
    """
    # rf_ble_mailbox.h status words
    return {
        0x1400: "BLE_DONE_OK",
        0x1402: "BLE_DONE_NOSYNC",
        0x1403: "BLE_DONE_RXERR",
        0x1404: "BLE_DONE_CONNECT (CSA#2)",
        0x140A: "BLE_DONE_CONNECT_CHSEL0 (legacy CSA)",
        0x140B: "BLE_DONE_ENDED",
        0x1810: "BLE_ERROR_PAR",
        0x1811: "BLE_ERROR_RXBUF",
    }.get(status, f"unknown 0x{status:04X}")


def _diagnose_nosync(result) -> str:
    """F20.a.1.e — interpret HW counters to identify root cause of NOSYNC.

    Returns a single-line verdict based on which counters are non-zero.
    """
    tx = result.f21_total_tx_adv_ind
    cr = result.f21_total_rx_connect_req
    ig = result.f21_total_rx_ignored
    nok = result.f21_total_rx_nok

    if tx == 0:
        return "SLAVE NEVER TX'd ADV_IND — RF setup failed before loop entry"
    if cr > 0:
        return (
            f"CONNECT_IND was accepted by HW filter ({cr}x) — bug is in TI status "
            "code interpretation or BLE state machine. NOT a radio-layer reject."
        )
    if ig > 0:
        return (
            f"CONNECT_IND or other packets arrived ({ig}x) but HW filter rejected "
            "them — check AdvA/InitA address bytes at radio level; LSB-first wire "
            "encoding might be inverted vs. firmware-side stored bytes."
        )
    if nok > 0:
        return (
            f"Packets arrived in RX window but {nok}x had CRC errors — RF/antenna "
            "issue or different access address (0x8E89BED6 expected on adv chan)."
        )
    return (
        "NO packets arrived in RX window across the entire loop — CONNECT_IND "
        "never lands inside the T_IFS=150µs window after slave's ADV_IND TX. "
        "Master timing too slow / master scanning other channels / master not "
        "actually attempting connection."
    )


def trace_table(slave):
    """F20.a.1.d — print the 8 internal-state trace fields with interpretive notes.

    The output of this table is the primary diagnostic signal for
    Smoke V2: whichever line disagrees with the expected value tells us
    which firmware layer is failing. Returns the list of printed lines."""
    lines = ["Field                          Value           Interpretation"]
    lines.append("-" * 78)

    pa = slave.peripheral_active_at_handoff
    pa_note = {
        0xFF: "handoff never reached (CMD_BLE_ADV_LEGACY did not run)",
        0: "flag was CLEARED before handoff (serve_gatt didn't arm OR cleared mid-flight)",
        1: "flag survived (handoff entered the inner block)",
    }.get(pa, "unexpected value")
    lines.append(f"peripheral_active_at_handoff   0x{pa:02X}            {pa_note}")

    ts = slave.f21_last_status
    ts_note = (
        "never set (no TX ran since boot/last query)" if ts == 0x0000 else _interpret_ble_status(ts)
    )
    lines.append(f"f21_last_status                 0x{ts:04X}          {ts_note}")

    ec = slave.extract_call_count
    ec_note = (
        "parser was never invoked (handoff bypassed the call)"
        if ec == 0
        else f"parser ran {ec} time(s)"
    )
    lines.append(f"extract_call_count             {ec:<3}             {ec_note}")

    es = slave.extract_entries_seen
    es_note = (
        "queue was empty when parser ran" if es == 0 else f"parser walked {es} FINISHED entry(ies)"
    )
    lines.append(f"extract_entries_seen           {es:<3}             {es_note}")

    pt = slave.extract_first_pdu_type
    pt_note = {
        0xFF: "no FINISHED entry seen (sentinel)",
        0x05: "CONNECT_IND (expected)",
        0x00: "ADV_IND (peer responded with adv, not connect)",
        0x06: "ADV_SCAN_IND",
        0x03: "SCAN_REQ",
        0x04: "SCAN_RSP",
    }.get(pt, "other - check BT Core Spec PDU types")
    lines.append(f"extract_first_pdu_type         0x{pt:02X}            {pt_note}")

    ai = slave.advertise_iterations
    ai_note = (
        "loop completed full count (no CONNECT_IND break)"
        if ai >= 5000
        else "broke early - likely CONNECT_IND"
    )
    lines.append(f"advertise_iterations           {ai:<5}           {ai_note}")

    fns = slave.f21_first_nonzero_status
    fns_note = (
        "never set — no non-OK status recorded (loop may not have run, or all iters were OK)"
        if fns == 0
        else _interpret_ble_status(fns)
    )
    lines.append(f"f21_first_nonzero_status       0x{fns:04X}          {fns_note}")

    adv_a_str = ":".join(f"{b:02X}" for b in reversed(slave.f21_adv_a))
    lines.append(
        f"f21_adv_a                      {adv_a_str:<17}"
        "AdvA bytes actually used by CMD_BLE_ADV — compare against Sniffle wire capture"
    )
    lines.append(
        f"f21_total_tx_adv_ind           {slave.f21_total_tx_adv_ind:<17d}HW count of ADV_IND packets fully TX'd"
    )
    lines.append(
        f"f21_total_rx_connect_req       {slave.f21_total_rx_connect_req:<17d}HW count of CONNECT_IND accepted by radio filter"
    )
    lines.append(
        f"f21_total_rx_ignored           {slave.f21_total_rx_ignored:<17d}HW count of packets RX'd OK but ignored (filter mismatch)"
    )
    lines.append(
        f"f21_total_rx_nok               {slave.f21_total_rx_nok:<17d}HW count of CRC-error packets in RX window"
    )
    lines.append(
        f"f21_last_rssi                  {slave.f21_last_rssi:<17d}dBm of last RX'd packet (any kind, -128 = no RX captured)"
    )
    lines.append("")
    lines.append(f"NOSYNC verdict: {_diagnose_nosync(slave)}")

    return lines


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

    # F20.a.1.f — Reversed sequencing: central FIRST (~2s for reset_device +
    # init + ble_connect cmd dispatch to enter scan window), THEN slave starts
    # advertising. Previously slave ran first and often finished BEFORE central
    # was scanning — guaranteed timing miss for low peripheral_count.
    cen_holder: dict = {}

    def central_wrapper() -> None:
        cen_holder["result"] = run_central_attempt(
            args.central_port, args.baudrate, args.target_mac
        )

    cen_thread = Thread(target=central_wrapper, daemon=True)
    cen_thread.start()
    time.sleep(2.5)  # cover reset_device (~1.5s) + init + initiator dispatch

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

    cen_thread.join(timeout=15.0)
    per_thread.join(timeout=15.0)
    cen_result = cen_holder.get(
        "result",
        {
            "services_count": 0,
            "chars_count": 0,
            "name_val": b"",
            "test_val": b"",
            "connected": False,
        },
    )

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

    print("\n--- F20.a.1.c internal-state trace ---")
    for line in trace_table(slave):
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
