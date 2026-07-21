from feralrf.enums import Response
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


def test_read_one_packet_skips_async_error_and_returns_next_packet():
    # Async RxStreamError frame (RSP_ERROR, seq=0, payload=error_code+context)
    # followed by a real RX_PACKET frame; read_one_packet() should skip the
    # error and return the Packet.
    error_frame = build_frame(Response.ERROR, 0, bytes([0x06, 0x02]))
    stream = error_frame + _rx_frame(seq=0)
    r = Radio(port="x")
    r._serial = FakeSerial(stream)
    pkt = r.read_one_packet(timeout=0.5)
    assert isinstance(pkt, Packet)
    assert pkt.data == b"\x03\x08\xff\xff"
