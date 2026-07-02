from feralrf.enums import Command, Response
from feralrf.protocol import build_frame
from feralrf.radio import Packet, Radio
from tests.test_radio_strict_responses import FakeSerial  # reuse existing fake


def _rx_frame(seq, data=b"\x03\x08\xff\xff", ts=7, ch=11, rssi=200, lqi=100, crc_ok=1):
    payload = ts.to_bytes(8, "little") + bytes([ch, rssi, lqi, crc_ok, len(data)]) + data
    return build_frame(Response.RX_PACKET, seq, payload)


def test_read_one_packet_returns_packet():
    r = Radio(port="x")
    r._serial = FakeSerial(_rx_frame(seq=0))
    pkt = r.read_one_packet(timeout=0.5)
    assert isinstance(pkt, Packet)
    assert pkt.data == b"\x03\x08\xff\xff"
    assert pkt.channel == 11
    assert pkt.crc_ok is True
    assert pkt.rssi_dbm == 200 - 256  # signed int8


def test_read_one_packet_timeout_returns_none():
    r = Radio(port="x")
    r._serial = FakeSerial(b"")
    assert r.read_one_packet(timeout=0.05) is None
