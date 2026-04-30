/*
 * FeralRF CC1352 - Crypto engine module
 *
 * Stateless one-shot wrappers over TI SDK crypto drivers (TRNG, AES-128,
 * SHA-256, ECDH, ECDSA). All primitives open the underlying TI driver,
 * execute, close, and return. Per-call key passing — no slots, no
 * persistent handles.
 *
 * Initialize once at boot via crypto_engine_init() before any other
 * module calls into the API.
 */

#ifndef CRYPTO_ENGINE_H
#define CRYPTO_ENGINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    CRYPTO_OK = 0,
    CRYPTO_BAD_PARAM = 1,
    CRYPTO_TAG_MISMATCH = 2,
    CRYPTO_HW_ERROR = 3,
    CRYPTO_NOT_INITIALIZED = 4,
    CRYPTO_UNSUPPORTED_CURVE = 5,
} crypto_engine_status_t;

typedef enum {
    CRYPTO_CURVE_P256 = 0,
    CRYPTO_CURVE_25519 = 1,
} crypto_curve_t;

bool crypto_engine_init(void);

crypto_engine_status_t crypto_engine_random(uint8_t n, uint8_t *out);

crypto_engine_status_t crypto_engine_aes_ecb(uint8_t op, const uint8_t key[16],
                                             const uint8_t in[16], uint8_t out[16]);

crypto_engine_status_t crypto_engine_aes_ctr(uint8_t op, const uint8_t key[16],
                                             const uint8_t iv[16], const uint8_t *in, size_t len,
                                             uint8_t *out);

crypto_engine_status_t crypto_engine_aes_cbc(uint8_t op, const uint8_t key[16],
                                             const uint8_t iv[16], const uint8_t *in, size_t len,
                                             uint8_t *out);

crypto_engine_status_t crypto_engine_aes_ccm(uint8_t op, const uint8_t key[16],
                                             const uint8_t *nonce, uint8_t nonce_len,
                                             const uint8_t *aad, size_t aad_len, const uint8_t *in,
                                             size_t pt_len, uint8_t tag_len, uint8_t *out,
                                             uint8_t *tag);

crypto_engine_status_t crypto_engine_aes_gcm(uint8_t op, const uint8_t key[16],
                                             const uint8_t iv[12], const uint8_t *aad,
                                             size_t aad_len, const uint8_t *in, size_t pt_len,
                                             uint8_t *out, uint8_t tag[16]);

crypto_engine_status_t crypto_engine_sha256(const uint8_t *in, size_t len, uint8_t out[32]);

crypto_engine_status_t crypto_engine_ecdh(crypto_curve_t curve, const uint8_t priv[32],
                                          const uint8_t *peer_pub, size_t peer_pub_len,
                                          uint8_t shared[32]);

crypto_engine_status_t crypto_engine_ecdsa_sign(crypto_curve_t curve, const uint8_t priv[32],
                                                const uint8_t hash[32], uint8_t sig[64]);

crypto_engine_status_t crypto_engine_ecdsa_verify(crypto_curve_t curve, const uint8_t *pub,
                                                  size_t pub_len, const uint8_t hash[32],
                                                  const uint8_t sig[64], bool *valid);

#endif /* CRYPTO_ENGINE_H */
