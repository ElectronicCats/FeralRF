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


def test_aes_ecb_invalid_key_raises():
    from feralrf import Radio

    radio = Radio(port="dummy")
    with pytest.raises(ValueError, match="key.*16"):
        radio.aes_encrypt(b"\x00" * 8, b"\x00" * 16, mode="ecb")


def test_aes_ecb_invalid_data_raises():
    from feralrf import Radio

    radio = Radio(port="dummy")
    with pytest.raises(ValueError, match="data.*16|block"):
        radio.aes_encrypt(b"\x00" * 16, b"\x00" * 8, mode="ecb")


def test_aes_unknown_mode_raises():
    from feralrf import Radio

    radio = Radio(port="dummy")
    with pytest.raises(ValueError, match="mode"):
        radio.aes_encrypt(b"\x00" * 16, b"\x00" * 16, mode="foo")


def test_aes_ctr_requires_iv():
    from feralrf import Radio

    radio = Radio(port="dummy")
    with pytest.raises(ValueError, match="iv"):
        radio.aes_encrypt(b"\x00" * 16, b"\x00" * 16, mode="ctr")


def test_aes_ecb_sends_correct_frame(monkeypatch):
    from feralrf import Radio
    from feralrf.enums import Command, Response

    radio = Radio(port="dummy")
    sent = []
    monkeypatch.setattr(radio, "_send_command", lambda c, p=b"": sent.append((c, bytes(p))))
    monkeypatch.setattr(
        radio,
        "_read_response",
        lambda timeout=1.0, expected=None: (Response.RSP_AES, 0, b"\x99" * 16),
    )

    key = b"\x00" * 16
    pt = b"\x11" * 16
    radio.aes_encrypt(key, pt, mode="ecb")

    cmd, payload = sent[0]
    assert cmd == Command.CMD_AES_ECB
    assert payload == bytes([0x00]) + key + pt  # op=encrypt


def test_aes_ccm_invalid_tag_len_raises():
    from feralrf import Radio

    radio = Radio(port="dummy")
    with pytest.raises(ValueError, match="tag_len"):
        radio.aes_ccm_encrypt(b"\x00" * 16, b"\x00" * 13, b"", b"hi", tag_len=12)


def test_aes_ccm_invalid_nonce_len_raises():
    from feralrf import Radio

    radio = Radio(port="dummy")
    with pytest.raises(ValueError, match="nonce"):
        radio.aes_ccm_encrypt(b"\x00" * 16, b"\x00" * 6, b"", b"hi", tag_len=8)


def test_aes_ccm_encrypt_sends_correct_frame(monkeypatch):
    from feralrf import Radio
    from feralrf.enums import Command, Response

    radio = Radio(port="dummy")
    sent = []
    monkeypatch.setattr(radio, "_send_command", lambda c, p=b"": sent.append((c, bytes(p))))
    monkeypatch.setattr(
        radio,
        "_read_response",
        lambda timeout=1.0, expected=None: (Response.RSP_AES_CCM, 0, b"\xCC" * 4 + b"\xAA" * 8),
    )

    ct, tag = radio.aes_ccm_encrypt(
        key=b"\x11" * 16, nonce=b"\x22" * 13, aad=b"AB", plaintext=b"\x33" * 4, tag_len=8
    )
    assert ct == b"\xCC" * 4
    assert tag == b"\xAA" * 8
    cmd, payload = sent[0]
    assert cmd == Command.CMD_AES_CCM
    assert payload[0] == 0x00  # encrypt
    assert payload[1:17] == b"\x11" * 16  # key
    assert payload[17] == 13  # nonce_len
    assert payload[18:31] == b"\x22" * 13  # nonce
    assert payload[31:33] == bytes([2, 0])  # aad_len LE u16
    assert payload[33:35] == bytes([4, 0])  # pt_len LE u16
    assert payload[35] == 8  # tag_len
    assert payload[36:38] == b"AB"  # aad
    assert payload[38:42] == b"\x33" * 4  # pt


def test_aes_ccm_decrypt_tag_mismatch_raises(monkeypatch):
    from feralrf import Radio
    from feralrf.enums import Response
    from feralrf.exceptions import CryptoError

    radio = Radio(port="dummy")
    monkeypatch.setattr(radio, "_send_command", lambda c, p=b"": None)
    monkeypatch.setattr(
        radio,
        "_read_response",
        lambda timeout=1.0, expected=None: (Response.ERROR, 0x10, b"\x02"),  # err=2 in payload[0]
    )
    with pytest.raises(CryptoError, match="tag"):
        radio.aes_ccm_decrypt(
            key=b"\x00" * 16,
            nonce=b"\x00" * 13,
            aad=b"",
            ciphertext=b"\x00" * 4,
            tag=b"\xFF" * 8,
            tag_len=8,
        )


def test_aes_gcm_invalid_iv_raises():
    from feralrf import Radio

    radio = Radio(port="dummy")
    with pytest.raises(ValueError, match="iv"):
        radio.aes_gcm_encrypt(b"\x00" * 16, b"\x00" * 8, b"", b"x")


def test_aes_gcm_encrypt_roundtrip(monkeypatch):
    from feralrf import Radio
    from feralrf.enums import Command, Response

    radio = Radio(port="dummy")
    sent = []
    monkeypatch.setattr(radio, "_send_command", lambda c, p=b"": sent.append((c, bytes(p))))
    monkeypatch.setattr(
        radio,
        "_read_response",
        lambda timeout=1.0, expected=None: (Response.RSP_AES_GCM, 0, b"\xCC" * 4 + b"\xAA" * 16),
    )

    ct, tag = radio.aes_gcm_encrypt(
        key=b"\x11" * 16, iv=b"\x22" * 12, aad=b"AB", plaintext=b"\x33" * 4
    )
    assert ct == b"\xCC" * 4
    assert tag == b"\xAA" * 16
    cmd, payload = sent[0]
    assert cmd == Command.CMD_AES_GCM
    assert payload[0] == 0x00
    assert payload[1:17] == b"\x11" * 16
    assert payload[17:29] == b"\x22" * 12


def test_sha256_oversize_raises():
    from feralrf import Radio

    radio = Radio(port="dummy")
    with pytest.raises(ValueError, match="240"):
        radio.sha256(b"\x00" * 241)


def test_sha256_sends_correct_frame(monkeypatch):
    from feralrf import Radio
    from feralrf.enums import Command, Response

    radio = Radio(port="dummy")
    sent = []
    monkeypatch.setattr(radio, "_send_command", lambda c, p=b"": sent.append((c, bytes(p))))
    monkeypatch.setattr(
        radio,
        "_read_response",
        lambda timeout=1.0, expected=None: (Response.RSP_SHA256, 0, bytes(range(32))),
    )

    digest = radio.sha256(b"abc")
    assert digest == bytes(range(32))
    cmd, payload = sent[0]
    assert cmd == Command.CMD_SHA256
    assert payload == b"abc"


def test_ecdh_invalid_priv_raises():
    from feralrf import Radio

    radio = Radio(port="dummy")
    with pytest.raises(ValueError, match="priv"):
        radio.ecdh(my_priv=b"\x00" * 31, peer_pub=b"\x00" * 64, curve="p256")


def test_ecdh_unknown_curve_raises():
    from feralrf import Radio

    radio = Radio(port="dummy")
    with pytest.raises(ValueError, match="curve"):
        radio.ecdh(my_priv=b"\x00" * 32, peer_pub=b"\x00" * 64, curve="p999")


def test_ecdh_p256_sends_correct_frame(monkeypatch):
    from feralrf import Radio
    from feralrf.enums import Command, Response

    radio = Radio(port="dummy")
    sent = []
    monkeypatch.setattr(radio, "_send_command", lambda c, p=b"": sent.append((c, bytes(p))))
    monkeypatch.setattr(
        radio,
        "_read_response",
        lambda timeout=1.0, expected=None: (Response.RSP_ECDH, 0, b"\xAB" * 32),
    )

    shared = radio.ecdh(my_priv=b"\x11" * 32, peer_pub=b"\x22" * 64, curve="p256")
    assert shared == b"\xAB" * 32
    cmd, payload = sent[0]
    assert cmd == Command.CMD_ECDH
    assert payload[0] == 0x00  # P-256
    assert payload[1:33] == b"\x11" * 32
    assert payload[33:97] == b"\x22" * 64


def test_ecdh_curve25519_pub_length():
    from feralrf import Radio

    radio = Radio(port="dummy")
    with pytest.raises(ValueError, match="32"):
        radio.ecdh(my_priv=b"\x00" * 32, peer_pub=b"\x00" * 64, curve="curve25519")


def test_ecdsa_sign_invalid_priv_raises():
    from feralrf import Radio

    radio = Radio(port="dummy")
    with pytest.raises(ValueError, match="priv"):
        radio.ecdsa_sign(priv=b"\x00" * 31, msg_hash=b"\x00" * 32, curve="p256")


def test_ecdsa_sign_invalid_hash_raises():
    from feralrf import Radio

    radio = Radio(port="dummy")
    with pytest.raises(ValueError, match="hash"):
        radio.ecdsa_sign(priv=b"\x00" * 32, msg_hash=b"\x00" * 16, curve="p256")


def test_ecdsa_sign_sends_correct_frame(monkeypatch):
    from feralrf import Radio
    from feralrf.enums import Command, Response

    radio = Radio(port="dummy")
    sent = []
    monkeypatch.setattr(radio, "_send_command", lambda c, p=b"": sent.append((c, bytes(p))))
    monkeypatch.setattr(
        radio,
        "_read_response",
        lambda timeout=1.0, expected=None: (Response.RSP_ECDSA_SIG, 0, b"\xAB" * 32 + b"\xCD" * 32),
    )

    sig = radio.ecdsa_sign(priv=b"\x11" * 32, msg_hash=b"\x22" * 32, curve="p256")
    assert len(sig) == 64
    cmd, payload = sent[0]
    assert cmd == Command.CMD_ECDSA_SIGN
    assert payload[0] == 0x00


def test_ecdsa_verify_returns_bool(monkeypatch):
    from feralrf import Radio
    from feralrf.enums import Response

    radio = Radio(port="dummy")
    monkeypatch.setattr(radio, "_send_command", lambda c, p=b"": None)
    monkeypatch.setattr(
        radio,
        "_read_response",
        lambda timeout=1.0, expected=None: (Response.RSP_ECDSA_VERIFY, 0, b"\x01"),
    )
    assert (
        radio.ecdsa_verify(pub=b"\x00" * 64, msg_hash=b"\x00" * 32, sig=b"\x00" * 64, curve="p256")
        is True
    )
    monkeypatch.setattr(
        radio,
        "_read_response",
        lambda timeout=1.0, expected=None: (Response.RSP_ECDSA_VERIFY, 0, b"\x00"),
    )
    assert (
        radio.ecdsa_verify(pub=b"\x00" * 64, msg_hash=b"\x00" * 32, sig=b"\x00" * 64, curve="p256")
        is False
    )
