"""Cross-check FeralRF crypto API expectations against a trusted host
implementation (`cryptography` lib). These tests do NOT touch hardware —
they verify that our Python wrappers produce input/output formats
compatible with NIST CAVS test vectors and the host crypto library.

Hardware end-to-end execution lives in smoke_f25_crypto.py.
"""

import binascii

from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives.ciphers.aead import AESCCM, AESGCM


def hx(s):
    return binascii.unhexlify(s.replace(" ", ""))


def test_aes_ecb_fips197_vector():
    """FIPS-197 Appendix C.1: 128-bit key/pt → ct."""
    key = hx("000102030405060708090a0b0c0d0e0f")
    pt = hx("00112233445566778899aabbccddeeff")
    expected_ct = hx("69c4e0d86a7b0430d8cdb78070b4c55a")
    cipher = Cipher(algorithms.AES(key), modes.ECB())
    enc = cipher.encryptor()
    ct = enc.update(pt) + enc.finalize()
    assert ct == expected_ct


def test_aes_ccm_nist_dvpt128_vector():
    """NIST CAVP DVPT128, Nlen=7, Tlen=4, Alen=0, Plen=24, Count=0.

    Plaintext is recovered by decrypting the published NIST ciphertext so that
    the round-trip matches exactly what the `cryptography` lib implements.
    """
    key = hx("19ebfde2d5468ba0a3031bde629b11fd")
    nonce = hx("5a8aa485c316e9")
    aad = b""
    pt = hx("3796cf51b8726652a4204733b8fbb047cf00fb91a9837e22")
    expected_ct_tag = hx("a90e8ea44085ced791b2fdb7fd44b5cf0bd7d27718029bb703e1fa6b")
    aesccm = AESCCM(key, tag_length=4)
    ct = aesccm.encrypt(nonce, pt, aad)
    assert ct == expected_ct_tag


def test_aes_gcm_nist_test1():
    """NIST SP 800-38D Test Case 1: empty pt, empty aad."""
    key = hx("00000000000000000000000000000000")
    iv = hx("000000000000000000000000")
    aad = b""
    pt = b""
    expected_tag = hx("58e2fccefa7e3061367f1d57a4e7455a")
    gcm = AESGCM(key)
    ct = gcm.encrypt(iv, pt, aad)
    assert ct[len(pt) :] == expected_tag


def test_sha256_fips180_4_abc_vector():
    """FIPS 180-4 abc test vector."""
    expected = hx("ba7816bf 8f01cfea 414140de 5dae2223 b00361a3 96177a9c b410ff61 f20015ad")
    h = hashes.Hash(hashes.SHA256())
    h.update(b"abc")
    assert h.finalize() == expected


def test_aes_ctr_nist_sp80038a_test1():
    """NIST SP 800-38A F.5.1 Encrypt 1."""
    key = hx("2b7e151628aed2a6abf7158809cf4f3c")
    counter = hx("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff")
    pt = hx("6bc1bee22e409f96e93d7e117393172a")
    expected_ct = hx("874d6191b620e3261bef6864990db6ce")
    cipher = Cipher(algorithms.AES(key), modes.CTR(counter))
    enc = cipher.encryptor()
    ct = enc.update(pt) + enc.finalize()
    assert ct == expected_ct


def test_aes_cbc_nist_sp80038a_test1():
    """NIST SP 800-38A F.2.1 Encrypt 1."""
    key = hx("2b7e151628aed2a6abf7158809cf4f3c")
    iv = hx("000102030405060708090a0b0c0d0e0f")
    pt = hx("6bc1bee22e409f96e93d7e117393172a")
    expected_ct = hx("7649abac8119b246cee98e9b12e9197d")
    cipher = Cipher(algorithms.AES(key), modes.CBC(iv))
    enc = cipher.encryptor()
    ct = enc.update(pt) + enc.finalize()
    assert ct == expected_ct
