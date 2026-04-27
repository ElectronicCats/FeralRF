"""
FeralRF - Response parsers
"""

import struct
from dataclasses import dataclass
from typing import Optional


@dataclass
class RxPacketResponse:
    """RX packet response payload"""

    timestamp_us: int
    channel: int
    rssi_dbm: int
    lqi: int
    crc_ok: bool
    data: bytes
    ll_pdu_kind: Optional[int] = None
    ll_pdu_type: Optional[int] = None
    ll_pdu_flags: Optional[int] = None

    @classmethod
    def parse(cls, payload: bytes) -> "RxPacketResponse":
        if len(payload) < 13:
            raise ValueError("Payload too short for RX packet")

        timestamp = struct.unpack("<Q", payload[0:8])[0]
        channel = payload[8]
        rssi = struct.unpack("<b", bytes([payload[9]]))[0]
        lqi = payload[10]
        crc_ok = payload[11] == 1
        pkt_len = payload[12]
        data = payload[13 : 13 + pkt_len]
        ll_pdu_kind = None
        ll_pdu_type = None
        ll_pdu_flags = None
        ll_meta_offset = 13 + pkt_len

        if len(payload) >= ll_meta_offset + 2:
            ll_pdu_kind = payload[ll_meta_offset]
            ll_pdu_type = payload[ll_meta_offset + 1]
            if len(payload) >= ll_meta_offset + 3:
                ll_pdu_flags = payload[ll_meta_offset + 2]

        return cls(
            timestamp_us=timestamp,
            channel=channel,
            rssi_dbm=rssi,
            lqi=lqi,
            crc_ok=crc_ok,
            data=data,
            ll_pdu_kind=ll_pdu_kind,
            ll_pdu_type=ll_pdu_type,
            ll_pdu_flags=ll_pdu_flags,
        )


@dataclass
class SpectrumDataResponse:
    """Spectrum data response payload"""

    frequency_hz: int
    rssi_dbm: int
    samples: list

    @classmethod
    def parse(cls, payload: bytes) -> "SpectrumDataResponse":
        if len(payload) < 5:
            raise ValueError("Payload too short for spectrum data")

        freq = struct.unpack("<I", payload[0:4])[0]
        rssi = struct.unpack("<b", bytes([payload[4]]))[0]
        samples = list(payload[5:])

        return cls(
            frequency_hz=freq,
            rssi_dbm=rssi,
            samples=samples,
        )


@dataclass
class InfoResponse:
    """Device info response payload"""

    firmware_major: int
    firmware_minor: int
    firmware_patch: int
    capabilities: int
    serial: str

    @classmethod
    def parse(cls, payload: bytes) -> "InfoResponse":
        if len(payload) < 4:
            raise ValueError("Payload too short for info")

        return cls(
            firmware_major=payload[0],
            firmware_minor=payload[1],
            firmware_patch=payload[2],
            capabilities=payload[3] if len(payload) > 3 else 0,
            serial=payload[4:12].hex() if len(payload) > 4 else "",
        )


@dataclass
class DebugTimingEntry:
    """One captured master-event timing record (matches firmware ring entry).

    Wire layout (18 bytes, Session 4):
        u16 event_idx + u32 start_rat + u32 end_rat + u16 status
        + u8 num_sent + u8 n_tx + u8 n_rx_ok + u8 n_rx_nok + u8 n_rx_ignored
        + u8 pkt_status.
    Session 3 was 13 bytes (the first five fields only). The (n_tx,
    n_rx_*, pkt_status bTimeStampValid) tuple distinguishes "master state
    machine never executed" (all zero) from "master TX'd but slave silent"
    (n_tx>=1, n_rx_*==0).
    """

    event_idx: int       # u16 — BleConnMgr s_event_counter at capture time
    start_rat: int       # u32 — curHopTime fed to RadioIF_bleCentral
    end_rat: int         # u32 — s_next_hop_time fed to RadioIF_bleCentral
    status: int          # u16 — RF status code (BLE_DONE_NOSYNC=0x1402, OK=0x1400, …)
    num_sent: int        # u8  — pOutput.nTxEntryDone
    n_tx: int            # u8  — pOutput.nTx (total TX incl. auto-empty + retrans)
    n_rx_ok: int         # u8  — pOutput.nRxOk
    n_rx_nok: int        # u8  — pOutput.nRxNok
    n_rx_ignored: int    # u8  — pOutput.nRxIgnored
    pkt_status: int      # u8  — packed pktStatus bitfield (see properties)

    @property
    def b_time_stamp_valid(self) -> bool:
        return bool(self.pkt_status & 0x01)

    @property
    def b_last_crc_err(self) -> bool:
        return bool(self.pkt_status & 0x02)

    @property
    def b_last_ignored(self) -> bool:
        return bool(self.pkt_status & 0x04)

    @property
    def b_last_empty(self) -> bool:
        return bool(self.pkt_status & 0x08)

    @property
    def b_last_ctrl(self) -> bool:
        return bool(self.pkt_status & 0x10)

    @property
    def b_last_md(self) -> bool:
        return bool(self.pkt_status & 0x20)

    @property
    def b_last_ack(self) -> bool:
        return bool(self.pkt_status & 0x40)


@dataclass
class DebugTimingResponse:
    """Parsed RSP_DEBUG_TIMING payload: 1-byte count + count×18-byte entries."""

    count: int
    entries: list

    _ENTRY_SIZE = 18  # u16 + u32 + u32 + u16 + u8*6

    @classmethod
    def parse(cls, payload: bytes) -> "DebugTimingResponse":
        if len(payload) < 1:
            raise ValueError("DEBUG_TIMING payload too short (no count byte)")
        count = payload[0]
        expected_len = 1 + count * cls._ENTRY_SIZE
        if len(payload) < expected_len:
            raise ValueError(
                f"DEBUG_TIMING payload truncated: got {len(payload)}, "
                f"need {expected_len} for count={count}"
            )
        entries = []
        for i in range(count):
            base = 1 + i * cls._ENTRY_SIZE
            (event_idx, start_rat, end_rat, status, num_sent,
             n_tx, n_rx_ok, n_rx_nok, n_rx_ignored, pkt_status) = struct.unpack(
                "<HIIHBBBBBB", payload[base : base + cls._ENTRY_SIZE]
            )
            entries.append(
                DebugTimingEntry(
                    event_idx=event_idx,
                    start_rat=start_rat,
                    end_rat=end_rat,
                    status=status,
                    num_sent=num_sent,
                    n_tx=n_tx,
                    n_rx_ok=n_rx_ok,
                    n_rx_nok=n_rx_nok,
                    n_rx_ignored=n_rx_ignored,
                    pkt_status=pkt_status,
                )
            )
        return cls(count=count, entries=entries)
