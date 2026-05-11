"""F20.a.1.d — unit tests for Radio.debug_slave parser (no hardware)."""

from typing import List, Optional, Tuple

import pytest

from feralrf.enums import Response
from feralrf.protocol import build_frame
from feralrf.radio import Radio, SlaveDbgResult


class FakeSerial:
    def __init__(self) -> None:
        self.is_open = True
        self.written: bytearray = bytearray()
        self._read_buf: bytearray = bytearray()
        self.timeout: Optional[float] = None

    def write(self, data: bytes) -> int:
        self.written.extend(data)
        return len(data)

    def flush(self) -> None:
        pass

    def read(self, n: int = 1) -> bytes:
        if not self._read_buf:
            return b""
        out = bytes(self._read_buf[:n])
        del self._read_buf[:n]
        return out

    def reset_input_buffer(self) -> None:
        self._read_buf.clear()

    def reset_output_buffer(self) -> None:
        self.written.clear()

    def close(self) -> None:
        self.is_open = False

    def queue_response(self, cmd_id: int, seq: int, payload: bytes = b"") -> None:
        self._read_buf.extend(build_frame(cmd_id, seq, payload))


def _radio_with_fake_serial() -> Tuple[Radio, FakeSerial]:
    radio = Radio(port="/dev/null")
    fake = FakeSerial()
    radio._serial = fake  # type: ignore[assignment]
    return radio, fake


def _build_payload(snapshot: dict, entries: List[dict]) -> bytes:
    """Build a synthetic RSP_DEBUG_SLAVE payload for testing (F20.a.1.e 51 B layout)."""
    buf = bytearray()
    buf.extend(snapshot["access_addr"].to_bytes(4, "little"))
    buf.extend(snapshot["crc_init"].to_bytes(4, "little"))
    buf.extend(snapshot["win_offset"].to_bytes(2, "little"))
    buf.extend(snapshot["hop_interval"].to_bytes(2, "little"))
    buf.extend(snapshot["latency"].to_bytes(2, "little"))
    buf.extend(snapshot["superv_timeout"].to_bytes(2, "little"))
    buf.append(snapshot["hop_increment"])
    buf.extend(snapshot["connect_ind_end_rat"].to_bytes(4, "little"))
    buf.extend(snapshot["first_anchor_rat"].to_bytes(4, "little"))
    # F20.a.1.d trace block (16 B): renames last_tx_status → f21_last_status,
    # adds f21_first_nonzero_status (u16) + f21_adv_a (6 B).
    buf.extend(snapshot.get("f21_last_status", 0).to_bytes(2, "little"))  # off 25
    buf.append(snapshot.get("peripheral_active_at_handoff", 0))  # off 27
    buf.append(snapshot.get("extract_call_count", 0))  # off 28
    buf.append(snapshot.get("extract_entries_seen", 0))  # off 29
    buf.append(snapshot.get("extract_first_pdu_type", 0))  # off 30
    buf.extend(snapshot.get("advertise_iterations", 0).to_bytes(2, "little"))  # off 31
    buf.extend(snapshot.get("f21_first_nonzero_status", 0).to_bytes(2, "little"))  # off 33
    buf.extend(snapshot.get("f21_adv_a", b"\x00" * 6))  # off 35
    # F20.a.1.e HW counters block (9 B)
    buf.extend(snapshot.get("f21_total_tx_adv_ind", 0).to_bytes(2, "little"))  # off 41
    buf.extend(snapshot.get("f21_total_rx_connect_req", 0).to_bytes(2, "little"))  # off 43
    buf.extend(snapshot.get("f21_total_rx_ignored", 0).to_bytes(2, "little"))  # off 45
    buf.extend(snapshot.get("f21_total_rx_nok", 0).to_bytes(2, "little"))  # off 47
    buf.append(snapshot.get("f21_last_rssi", 0) & 0xFF)  # off 49 (i8 as unsigned byte)
    buf.append(len(entries))  # off 50
    for e in entries:
        buf.extend(e["event_counter"].to_bytes(2, "little"))
        buf.append(e["chan"])
        buf.extend(e["anchor_rat"].to_bytes(4, "little"))
        buf.extend(e["actual_start_rat"].to_bytes(4, "little"))
        buf.extend(e["status"].to_bytes(2, "little"))
        buf.append(e["n_rx_ok"])
        buf.append(e["n_rx_nok"])
        buf.append(e["n_rx_ignored"])
        buf.append(e["pkt_status"])
    return bytes(buf)


class TestDebugSlaveParser:
    def test_empty_ring(self):
        radio, fake = _radio_with_fake_serial()
        snap = {
            "access_addr": 0x12345678,
            "crc_init": 0x00ABCDEF,
            "win_offset": 5,
            "hop_interval": 24,
            "latency": 0,
            "superv_timeout": 100,
            "hop_increment": 7,
            "connect_ind_end_rat": 0x4F00_0000,
            "first_anchor_rat": 0x4F00_5000,
        }
        payload = _build_payload(snap, [])
        fake.queue_response(Response.DEBUG_SLAVE, seq=radio._seq, payload=payload)
        result = radio.debug_slave()
        assert isinstance(result, SlaveDbgResult)
        assert result.access_addr == 0x12345678
        assert result.crc_init == 0x00ABCDEF
        assert result.win_offset == 5
        assert result.hop_interval == 24
        assert result.hop_increment == 7
        assert result.connect_ind_end_rat == 0x4F00_0000
        assert result.first_anchor_rat == 0x4F00_5000
        assert result.entries == []

    def test_two_entries(self):
        radio, fake = _radio_with_fake_serial()
        snap = {
            "access_addr": 0x11223344,
            "crc_init": 0x00112233,
            "win_offset": 6,
            "hop_interval": 24,
            "latency": 0,
            "superv_timeout": 100,
            "hop_increment": 5,
            "connect_ind_end_rat": 1000,
            "first_anchor_rat": 7000,
        }
        entries = [
            {
                "event_counter": 1,
                "chan": 5,
                "anchor_rat": 7000,
                "actual_start_rat": 7000,
                "status": 0x1A03,
                "n_rx_ok": 0,
                "n_rx_nok": 0,
                "n_rx_ignored": 0,
                "pkt_status": 0,
            },
            {
                "event_counter": 2,
                "chan": 10,
                "anchor_rat": 12000,
                "actual_start_rat": 12100,
                "status": 0x1A04,
                "n_rx_ok": 1,
                "n_rx_nok": 0,
                "n_rx_ignored": 0,
                "pkt_status": 0x01,
            },
        ]
        payload = _build_payload(snap, entries)
        fake.queue_response(Response.DEBUG_SLAVE, seq=radio._seq, payload=payload)
        result = radio.debug_slave()
        assert len(result.entries) == 2
        assert result.entries[0].event_counter == 1
        assert result.entries[0].n_rx_ok == 0
        assert result.entries[1].event_counter == 2
        assert result.entries[1].chan == 10
        assert result.entries[1].n_rx_ok == 1
        assert result.entries[1].pkt_status == 0x01

    def test_truncated_header_raises(self):
        from feralrf.exceptions import ProtocolError

        radio, fake = _radio_with_fake_serial()
        # 20 bytes — short of the 51-byte F20.a.1.e header
        fake.queue_response(Response.DEBUG_SLAVE, seq=radio._seq, payload=b"\x00" * 20)
        with pytest.raises(ProtocolError, match="too short"):
            radio.debug_slave()

    def test_count_truncated_by_payload(self):
        """If header says count=5 but only 2 entries' worth of bytes follow,
        parser should return the 2 actually-present entries (silent truncation)."""
        radio, fake = _radio_with_fake_serial()
        snap = {
            "access_addr": 0xAAAAAAAA,
            "crc_init": 0x00BBBBBB,
            "win_offset": 0,
            "hop_interval": 24,
            "latency": 0,
            "superv_timeout": 100,
            "hop_increment": 7,
            "connect_ind_end_rat": 0,
            "first_anchor_rat": 0,
            "f21_last_status": 0x1404,
            "peripheral_active_at_handoff": 1,
            "extract_call_count": 1,
            "extract_entries_seen": 1,
            "extract_first_pdu_type": 0x05,
            "advertise_iterations": 1,
        }
        # _build_payload writes len(entries) as count; we want count=5 with only
        # 2 entries on the wire.
        two_entries = [
            {
                "event_counter": 1,
                "chan": 5,
                "anchor_rat": 0,
                "actual_start_rat": 0,
                "status": 0,
                "n_rx_ok": 0,
                "n_rx_nok": 0,
                "n_rx_ignored": 0,
                "pkt_status": 0,
            },
            {
                "event_counter": 2,
                "chan": 5,
                "anchor_rat": 0,
                "actual_start_rat": 0,
                "status": 0,
                "n_rx_ok": 0,
                "n_rx_nok": 0,
                "n_rx_ignored": 0,
                "pkt_status": 0,
            },
        ]
        full = bytearray(_build_payload(snap, two_entries))
        # Override the count byte at offset 50 to claim 5 entries, but leave
        # only 2 on wire.
        full[50] = 5
        fake.queue_response(Response.DEBUG_SLAVE, seq=radio._seq, payload=bytes(full))
        result = radio.debug_slave()
        assert len(result.entries) == 2
        assert result.entries[0].event_counter == 1
        assert result.entries[1].event_counter == 2

    def test_trace_fields_round_trip(self):
        """F20.a.1.d — assert the 8 trace fields (renamed f21_last_status + new f21_first_nonzero_status + f21_adv_a) parse from the wire."""
        radio, fake = _radio_with_fake_serial()
        snap = {
            "access_addr": 0xDEADBEEF,
            "crc_init": 0x00ABCDEF,
            "win_offset": 5,
            "hop_interval": 24,
            "latency": 0,
            "superv_timeout": 100,
            "hop_increment": 7,
            "connect_ind_end_rat": 0,
            "first_anchor_rat": 0,
            "f21_last_status": 0x1404,
            "peripheral_active_at_handoff": 1,
            "extract_call_count": 3,
            "extract_entries_seen": 5,
            "extract_first_pdu_type": 0x05,
            "advertise_iterations": 42,
        }
        payload = _build_payload(snap, [])
        fake.queue_response(Response.DEBUG_SLAVE, seq=radio._seq, payload=payload)
        result = radio.debug_slave()
        assert result.f21_last_status == 0x1404
        assert result.peripheral_active_at_handoff == 1
        assert result.extract_call_count == 3
        assert result.extract_entries_seen == 5
        assert result.extract_first_pdu_type == 0x05
        assert result.advertise_iterations == 42
        # Sentinel: 0xFF "no entries seen yet" should round-trip.
        snap2 = dict(snap)
        snap2["extract_first_pdu_type"] = 0xFF
        payload2 = _build_payload(snap2, [])
        fake.queue_response(Response.DEBUG_SLAVE, seq=radio._seq, payload=payload2)
        result2 = radio.debug_slave()
        assert result2.extract_first_pdu_type == 0xFF

    def test_f21_first_nonzero_status_round_trip(self):
        """F20.a.1.d — parser exposes f21_first_nonzero_status (u16 LE off 33)."""
        radio, fake = _radio_with_fake_serial()
        snap = {
            "access_addr": 0x0,
            "crc_init": 0x0,
            "win_offset": 0,
            "hop_interval": 0,
            "latency": 0,
            "superv_timeout": 0,
            "hop_increment": 0,
            "connect_ind_end_rat": 0,
            "first_anchor_rat": 0,
            "f21_last_status": 0x1404,
            "f21_first_nonzero_status": 0x1402,  # BLE_DONE_NOSYNC mid-loop
            "advertise_iterations": 100,
        }
        payload = _build_payload(snap, [])
        fake.queue_response(Response.DEBUG_SLAVE, seq=radio._seq, payload=payload)
        result = radio.debug_slave()
        assert result.f21_first_nonzero_status == 0x1402
        assert result.f21_last_status == 0x1404

    def test_f21_adv_a_round_trip(self):
        """F20.a.1.d — parser exposes the 6-byte AdvA used by CMD_BLE_ADV (off 35)."""
        radio, fake = _radio_with_fake_serial()
        expected_adv_a = bytes([0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF])
        snap = {
            "access_addr": 0x0,
            "crc_init": 0x0,
            "win_offset": 0,
            "hop_interval": 0,
            "latency": 0,
            "superv_timeout": 0,
            "hop_increment": 0,
            "connect_ind_end_rat": 0,
            "first_anchor_rat": 0,
            "f21_adv_a": expected_adv_a,
        }
        payload = _build_payload(snap, [])
        fake.queue_response(Response.DEBUG_SLAVE, seq=radio._seq, payload=payload)
        result = radio.debug_slave()
        assert result.f21_adv_a == expected_adv_a

    def test_f21_hw_counters_round_trip(self):
        """F20.a.1.e — parser exposes the 4 HW counters (off 41-49, 4×u16 LE)."""
        radio, fake = _radio_with_fake_serial()
        snap = {
            "access_addr": 0x0,
            "crc_init": 0x0,
            "win_offset": 0,
            "hop_interval": 0,
            "latency": 0,
            "superv_timeout": 0,
            "hop_increment": 0,
            "connect_ind_end_rat": 0,
            "first_anchor_rat": 0,
            "f21_total_tx_adv_ind": 200,
            "f21_total_rx_connect_req": 0,
            "f21_total_rx_ignored": 3,
            "f21_total_rx_nok": 1,
        }
        payload = _build_payload(snap, [])
        fake.queue_response(Response.DEBUG_SLAVE, seq=radio._seq, payload=payload)
        result = radio.debug_slave()
        assert result.f21_total_tx_adv_ind == 200
        assert result.f21_total_rx_connect_req == 0
        assert result.f21_total_rx_ignored == 3
        assert result.f21_total_rx_nok == 1

    def test_f21_last_rssi_negative(self):
        """F20.a.1.e — parser decodes f21_last_rssi as signed int8 (off 49)."""
        radio, fake = _radio_with_fake_serial()
        snap = {
            "access_addr": 0x0,
            "crc_init": 0x0,
            "win_offset": 0,
            "hop_interval": 0,
            "latency": 0,
            "superv_timeout": 0,
            "hop_increment": 0,
            "connect_ind_end_rat": 0,
            "first_anchor_rat": 0,
            "f21_last_rssi": -47,
        }
        payload = _build_payload(snap, [])
        fake.queue_response(Response.DEBUG_SLAVE, seq=radio._seq, payload=payload)
        result = radio.debug_slave()
        assert result.f21_last_rssi == -47

    def test_f21_hw_counters_saturate(self):
        """F20.a.1.e — counters cap at 0xFFFF (saturating, not wrapping)."""
        radio, fake = _radio_with_fake_serial()
        snap = {
            "access_addr": 0x0,
            "crc_init": 0x0,
            "win_offset": 0,
            "hop_interval": 0,
            "latency": 0,
            "superv_timeout": 0,
            "hop_increment": 0,
            "connect_ind_end_rat": 0,
            "first_anchor_rat": 0,
            "f21_total_tx_adv_ind": 0xFFFF,
        }
        payload = _build_payload(snap, [])
        fake.queue_response(Response.DEBUG_SLAVE, seq=radio._seq, payload=payload)
        result = radio.debug_slave()
        assert result.f21_total_tx_adv_ind == 0xFFFF
