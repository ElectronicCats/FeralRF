"""Unit tests for F25 crypto HW primitives.

Hardware-free contract tests: command IDs, payload builders, error paths.
Hardware end-to-end coverage lives in python/examples/lab/smoke_f25_crypto.py.
NIST CAVS vector cross-checks live in test_crypto_vectors.py.
"""

import pytest

from feralrf.enums import STABLE_COMMANDS, Command, Response


def test_cmd_random_id():
    assert Command.CMD_RANDOM == 0x59


def test_cmd_aes_ecb_id():
    assert Command.CMD_AES_ECB == 0x5A


def test_cmd_aes_ccm_id():
    assert Command.CMD_AES_CCM == 0x5B


def test_cmd_aes_ctr_id():
    assert Command.CMD_AES_CTR == 0x5C


def test_cmd_aes_cbc_id():
    assert Command.CMD_AES_CBC == 0x5D


def test_cmd_aes_gcm_id():
    assert Command.CMD_AES_GCM == 0x5E


def test_cmd_sha256_id():
    assert Command.CMD_SHA256 == 0x5F


def test_cmd_ecdh_id():
    assert Command.CMD_ECDH == 0x60


def test_cmd_ecdsa_sign_id():
    assert Command.CMD_ECDSA_SIGN == 0x61


def test_cmd_ecdsa_verify_id():
    assert Command.CMD_ECDSA_VERIFY == 0x62


def test_rsp_random_id():
    assert Response.RSP_RANDOM == 0x95


def test_rsp_aes_id():
    assert Response.RSP_AES == 0x96


def test_rsp_aes_ccm_id():
    assert Response.RSP_AES_CCM == 0x97


def test_rsp_aes_gcm_id():
    assert Response.RSP_AES_GCM == 0x98


def test_rsp_sha256_id():
    assert Response.RSP_SHA256 == 0x99


def test_rsp_ecdh_id():
    assert Response.RSP_ECDH == 0x9A


def test_rsp_ecdsa_sig_id():
    assert Response.RSP_ECDSA_SIG == 0x9B


def test_rsp_ecdsa_verify_id():
    assert Response.RSP_ECDSA_VERIFY == 0x9C


def test_crypto_commands_in_stable():
    for cmd in (
        Command.CMD_RANDOM,
        Command.CMD_AES_ECB,
        Command.CMD_AES_CCM,
        Command.CMD_AES_CTR,
        Command.CMD_AES_CBC,
        Command.CMD_AES_GCM,
        Command.CMD_SHA256,
        Command.CMD_ECDH,
        Command.CMD_ECDSA_SIGN,
        Command.CMD_ECDSA_VERIFY,
    ):
        assert cmd in STABLE_COMMANDS


def test_crypto_error_exists():
    from feralrf.exceptions import CryptoError, RadioError

    assert issubclass(CryptoError, RadioError)
    err = CryptoError("test")
    assert str(err) == "test"


def test_random_bytes_sends_correct_frame(monkeypatch):
    """random_bytes(n) issues CMD_RANDOM with payload [n]."""
    from feralrf import Radio
    from feralrf.enums import Response

    radio = Radio(port="dummy")
    sent = []

    def fake_send(cmd, payload=b""):
        sent.append((cmd, bytes(payload)))

    def fake_read(timeout=1.0, expected=None):
        return (Response.RSP_RANDOM, 0, b"\x01\x02\x03\x04\x05")

    monkeypatch.setattr(radio, "_send_command", fake_send)
    monkeypatch.setattr(radio, "_read_response", fake_read)

    out = radio.random_bytes(5)
    assert out == b"\x01\x02\x03\x04\x05"
    assert sent[0] == (Command.CMD_RANDOM, b"\x05")


def test_random_bytes_invalid_n_raises():
    """random_bytes(0) and random_bytes(241) raise ValueError."""
    from feralrf import Radio

    radio = Radio(port="dummy")
    with pytest.raises(ValueError, match="1.*240"):
        radio.random_bytes(0)
    with pytest.raises(ValueError, match="1.*240"):
        radio.random_bytes(241)


def test_random_bytes_two_calls_differ(monkeypatch):
    """Returned bytes from second call differ from first."""
    from feralrf import Radio
    from feralrf.enums import Response

    radio = Radio(port="dummy")
    responses = [
        b"\xAA\xBB\xCC\xDD\xEE\xFF\x11\x22",
        b"\x33\x44\x55\x66\x77\x88\x99\x00",
    ]
    monkeypatch.setattr(radio, "_send_command", lambda c, p=b"": None)
    monkeypatch.setattr(
        radio,
        "_read_response",
        lambda timeout=1.0, expected=None: (Response.RSP_RANDOM, 0, responses.pop(0)),
    )
    a = radio.random_bytes(8)
    b = radio.random_bytes(8)
    assert a != b
