"""F8A Session 3 — correlate our debug_timing buffer with a Sniffle pcap.

Inputs:
  --json       Output of f8a_session3_capture.py.
  --pcap       Sniffle pcap covering the same connection.
  --target     Peer MAC (default DC:32:62:8D:E1:09).

What it does:
  1. Reads the Sniffle pcap (linktype 256 = BTLE_LL_WITH_PHDR).
  2. Finds the CONNECT_IND frame addressed to <target>.
  3. Computes peer first-listen wallclock window:
        start = ts(connect_ind_end) + 1.25 ms + WinOffset * 1.25 ms
        close = start + WinSize * 1.25 ms
  4. Calibrates wall<->RAT using connTime as the anchor:
        rat_us(connTime) ↔ ts(connect_ind_end)
  5. Projects our captured master events into wallclock and prints
     the signed offset between event-0's start and the peer window.

Notes / assumptions:
  - 1 RAT tick = 250 ns (4 MHz).
  - We assume TI's connectTime, written into pConnectReqOutput by
    CMD_BLE5_INITIATOR with bDynamicWinOffset=1, equals the wallclock
    timestamp of end-of-CONNECT_IND on air. If telemetry shows a
    consistent integer-µs offset, that's the wrong assumption and the
    delta becomes the right correction.
"""

import argparse
import json
import struct
import sys
from pathlib import Path

PCAP_GLOBAL_HEADER_LEN = 24
PCAP_RECORD_HEADER_LEN = 16
LINKTYPE_BLUETOOTH_LE_LL_WITH_PHDR = 256


def parse_pcap_records(path: Path):
    """Yield (ts_ns:int, data:bytes) for every record."""
    raw = path.read_bytes()
    if len(raw) < PCAP_GLOBAL_HEADER_LEN:
        raise ValueError("pcap too short")
    magic = struct.unpack("<I", raw[0:4])[0]
    nano = magic == 0xA1B23C4D
    micro = magic == 0xA1B2C3D4
    if not (nano or micro):
        raise ValueError(f"unsupported pcap magic 0x{magic:08x}")
    linktype = struct.unpack("<I", raw[20:24])[0]
    if linktype != LINKTYPE_BLUETOOTH_LE_LL_WITH_PHDR:
        print(f"warning: unexpected linktype {linktype}", file=sys.stderr)
    off = PCAP_GLOBAL_HEADER_LEN
    while off + PCAP_RECORD_HEADER_LEN <= len(raw):
        ts_sec, ts_sub, incl, _orig = struct.unpack("<IIII", raw[off : off + 16])
        ts_ns = ts_sec * 1_000_000_000 + (ts_sub if nano else ts_sub * 1000)
        off += PCAP_RECORD_HEADER_LEN
        data = raw[off : off + incl]
        off += incl
        yield ts_ns, data


def find_connect_ind(records, target_mac_le: bytes):
    """Sniffle BTLE pHdr is 14 bytes (Wireshark BTLE_LL_WITH_PHDR layout);
    LL PDU follows. CONNECT_IND PDU type 0x05.
    Return (ts_ns, ll_pdu_bytes)."""
    for ts_ns, data in records:
        if len(data) < 14 + 2 + 12:
            continue
        ll = data[14:]
        if len(ll) < 2:
            continue
        pdu_type = ll[0] & 0x0F
        if pdu_type != 0x05:  # CONNECT_IND
            continue
        # InitA = ll[2:8], AdvA = ll[8:14], LLData = ll[14:36]
        if len(ll) < 14 + 22:
            continue
        adva = ll[8:14]
        if adva == target_mac_le:
            return ts_ns, ll
    return None


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--json", required=True)
    p.add_argument("--pcap", required=True)
    p.add_argument("--target", default="DC:32:62:8D:E1:09")
    args = p.parse_args()

    target_le = bytes(reversed(bytes.fromhex(args.target.replace(":", ""))))
    blob = json.loads(Path(args.json).read_text())
    timing = blob["debug_timing"]
    if not timing:
        print("no debug_timing entries", file=sys.stderr)
        sys.exit(1)
    conn_time_rat = blob["conn_status"]["conn_time"]
    if conn_time_rat is None:
        print(
            "conn_status.conn_time is None — firmware older than session 1?",
            file=sys.stderr,
        )
        sys.exit(1)

    records = list(parse_pcap_records(Path(args.pcap)))
    found = find_connect_ind(records, target_le)
    if not found:
        print("no CONNECT_IND in pcap addressed to target", file=sys.stderr)
        sys.exit(1)
    ts_connect_ind_start_ns, ll = found

    # CONNECT_IND length on 1 Mbps:
    #   preamble(1) + AA(4) + header(2) + payload(34) + CRC(3) = 44 B → 352 µs
    connect_ind_air_ns = 352_000
    ts_connect_ind_end_ns = ts_connect_ind_start_ns + connect_ind_air_ns

    # WinOffset is bytes [8..9] of LLData (LLData starts at ll[14]).
    win_offset = struct.unpack("<H", ll[14 + 8 : 14 + 10])[0]
    win_size = ll[14 + 7]
    interval = struct.unpack("<H", ll[14 + 10 : 14 + 12])[0]
    print(f"CONNECT_IND start ts (ns): {ts_connect_ind_start_ns}")
    print(f"CONNECT_IND end ts   (ns): {ts_connect_ind_end_ns}")
    print(f"  WinOffset={win_offset} (1.25ms units) → {win_offset * 1250} µs")
    print(f"  WinSize={win_size} → {win_size * 1250} µs")
    print(f"  Interval={interval} → {interval * 1250} µs")

    transmit_window_delay_ns = 1_250_000  # 1.25 ms
    peer_first_listen_ns = ts_connect_ind_end_ns + transmit_window_delay_ns + win_offset * 1_250_000
    peer_window_close_ns = peer_first_listen_ns + win_size * 1_250_000
    print(f"\npeer first-listen open  (ns): {peer_first_listen_ns}")
    print(f"peer first-listen close (ns): {peer_window_close_ns}")
    print(f"peer window width (µs)     : {(peer_window_close_ns - peer_first_listen_ns) // 1000}")

    # Calibrate RAT→wall: assume connTime corresponds to ts_connect_ind_end_ns
    # 4 MHz RAT → 1 tick = 250 ns.
    def rat_to_wall_ns(rat):
        return ts_connect_ind_end_ns + (rat - conn_time_rat) * 250

    print(f"\nconnTime (RAT)        = {conn_time_rat}")
    print(f"  → wall ns (anchor)  = {rat_to_wall_ns(conn_time_rat)}")

    # Show every captured event projected to wall + offset to peer first listen
    print("\nper-event projection (oldest first):")
    print(f"  {'idx':>4}  {'startRAT':>10}  {'wall_ns':>20}  {'Δfromlisten_µs':>15}  status")
    for e in timing:
        wall = rat_to_wall_ns(e["start_rat"])
        delta_us = (wall - peer_first_listen_ns) // 1000
        print(
            f"  {e['event_idx']:>4}  {e['start_rat']:>10}  {wall:>20}  {delta_us:>15}  "
            f"0x{e['status']:04X}"
        )

    # Synthesise event 0's start (firmware sets it to connTime exactly) and report.
    e0_start_rat = conn_time_rat  # by firmware logic curHopTime = connTime
    e0_wall = rat_to_wall_ns(e0_start_rat)
    offset_us = (e0_wall - peer_first_listen_ns) // 1000
    delta_ticks = (e0_wall - peer_first_listen_ns) // 250
    print("\nsynthetic event 0:")
    print(f"  startRAT (= connTime) = {e0_start_rat}")
    print(f"  wall_ns               = {e0_wall}")
    print(f"  offset master_tx − peer_listen_open = {offset_us} µs")
    if offset_us > 0:
        print(f"  → MASTER STARTS {offset_us} µs AFTER peer opens listening")
    elif offset_us < 0:
        print(f"  → MASTER STARTS {-offset_us} µs BEFORE peer opens listening")
    else:
        print("  → MASTER STARTS exactly at peer listen open (zero offset)")
    print(f"  delta in RAT ticks    = {delta_ticks}")
    print()
    print(
        f"Proposed firmware fix: subtract {delta_ticks} from s_next_hop_time "
        "in BleConnMgr_start (i.e. add `- BLE_CONN_MGR_ANCHOR_CAL_TICKS` "
        f"with #define BLE_CONN_MGR_ANCHOR_CAL_TICKS ({delta_ticks})."
    )

    # Sanity: |delta_ticks| should be a small fraction of interval ticks (interval*4000 = 120000 for 30ms)
    interval_ticks = interval * 5000  # 1.25ms = 5000 ticks
    if abs(delta_ticks) >= interval_ticks:
        print(
            f"\n⚠ WARNING: |delta_ticks| ({abs(delta_ticks)}) >= "
            f"interval_ticks ({interval_ticks}). Calibration likely wrong; STOP and debug.",
            file=sys.stderr,
        )
        sys.exit(2)


if __name__ == "__main__":
    main()
