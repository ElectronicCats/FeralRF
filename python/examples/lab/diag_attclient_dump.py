#!/usr/bin/env python3
"""F8b Track A Task 0.2: AttClient state-machine instrumentation dump.

Wraps the deterministic repro from diag_attclient_repro.py and after each
phase (post-connect, post-discover, post-disconnect) reads the firmware's
AttClient debug ring buffer via CMD_ATT_DEBUG (0x49) → RSP_ATT_DEBUG (0xAA).

Each ring-buffer entry is 8 bytes:
    seq u16 LE | tag u8 | oldState u8 | newState u8 | connAlive u8 | mtu u8 | reqPending u8

Tag values match att_client.h's AttClient_DbgTag enum.
State values match att_client.h's AttClient_State enum.

Usage: python diag_attclient_dump.py [port] [target_mac]
"""

import struct
import sys
import time

from feralrf import Radio

CMD_ATT_DEBUG = 0x49
RSP_ATT_DEBUG = 0xAA

TAGS = {
    1: "INIT",
    2: "RESET",
    3: "startDisc-enter",
    4: "startDisc-exit-OK",
    5: "startDisc-exit-FAIL",
    6: "startRead-enter",
    7: "startRead-exit-OK",
    8: "startRead-exit-FAIL",
    9: "startWrite-enter",
    10: "startWrite-exit-OK",
    11: "startWrite-exit-FAIL",
    12: "L2CAP-RX",
    13: "MTU-RSP",
    14: "GROUP-RSP",
    15: "TYPE-RSP",
    16: "READ-RSP",
    17: "WRITE-RSP",
    18: "ERROR-RSP",
    19: "POLL-DONE",
    20: "POLL-tx-GROUP",
    21: "POLL-tx-TYPE",
    22: "POLL-tx-READ",
    23: "POLL-tx-MTU",
    24: "DONE-CB",
}

STATES = {
    0: "IDLE",
    1: "WAIT_MTU",
    2: "WAIT_DISC",
    3: "WAIT_CHAR",
    4: "WAIT_READ",
    5: "WAIT_WRITE",
    6: "DONE",
    7: "ERROR",
}


def dump_att_log(r: Radio, label: str) -> None:
    """Send CMD_ATT_DEBUG and pretty-print the response."""
    r._send_command(CMD_ATT_DEBUG, b"")
    cmd_id, seq, payload = r._read_response(timeout=2.0, expected={RSP_ATT_DEBUG})
    if cmd_id != RSP_ATT_DEBUG:
        print(f"  [{label}] unexpected response 0x{cmd_id:02X}")
        return
    n = payload[0]
    print(f"  [{label}] AttClient log: {n} entries")
    for i in range(n):
        off = 1 + i * 8
        seq_val, tag, old, new, alive, mtu, pending = struct.unpack_from("<HBBBBBB", payload, off)
        tag_s = TAGS.get(tag, f"?{tag}")
        old_s = STATES.get(old, f"?{old}")
        new_s = STATES.get(new, f"?{new}")
        print(
            f"    #{seq_val:5d} {tag_s:24s} {old_s:10s} -> {new_s:10s} "
            f"conn={alive} mtu={mtu} pending={pending}"
        )


def main() -> int:
    port = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM2"
    target_mac = sys.argv[2] if len(sys.argv) > 2 else "A8:E6:E8:8A:7D:F8"
    addr_le = bytes.fromhex("".join(target_mac.split(":")[::-1]))

    r = Radio(port)
    r.connect()
    time.sleep(0.3)
    print("== reset_device (clean baseline) ==")
    r.reset_device(wait=2.5)
    r.init()

    print("\n== initial dump (post-init, post-reset) ==")
    dump_att_log(r, "boot")

    for cycle in range(1, 4):
        print(f"\n=== cycle {cycle} ===")
        print("  ble_connect")
        try:
            res = r.ble_connect(addr_le, addr_type=0, timeout=10.0)
            print(f"    result code = {res.result}")
            if res.result != 0:
                print("    CONNECT FAILED — aborting cycles")
                dump_att_log(r, f"cycle{cycle}-connect-fail")
                break
        except Exception as e:
            print(f"    ble_connect EXCEPTION: {type(e).__name__}: {e}")
            dump_att_log(r, f"cycle{cycle}-connect-exc")
            break
        time.sleep(0.5)
        dump_att_log(r, f"cycle{cycle}-after-connect")

        st = r.conn_status()
        print(f"  conn_status: connected={st.connected} att_state={st.att_state}")

        print("  gatt_discover")
        try:
            d = r.gatt_discover(timeout=15.0)
            print(f"    OK — services={len(d.services)} chars={len(d.characteristics)}")
        except Exception as e:
            print(f"    FAILED: {type(e).__name__}: {e}")
        dump_att_log(r, f"cycle{cycle}-after-discover")

        st = r.conn_status()
        print(f"  conn_status: connected={st.connected} att_state={st.att_state}")

        print("  ble_disconnect")
        try:
            r.ble_disconnect()
        except Exception as e:
            print(f"    ble_disconnect EXCEPTION: {type(e).__name__}: {e}")
        time.sleep(1.0)
        dump_att_log(r, f"cycle{cycle}-after-disconnect")

        try:
            st = r.conn_status()
            print(f"  conn_status: connected={st.connected} att_state={st.att_state}")
        except Exception as e:
            print(f"  conn_status EXC: {type(e).__name__}: {e}")

    r.disconnect()
    return 0


if __name__ == "__main__":
    sys.exit(main())
