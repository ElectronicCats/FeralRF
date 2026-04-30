/*
 * FeralRF CC1352 - Crypto engine module implementation
 *
 * Each primitive opens the corresponding TI driver with
 * RETURN_BEHAVIOR_POLLING, executes, closes. Per-call only.
 */

#include "crypto_engine.h"

#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/drivers/TRNG.h>

#include "ti_drivers_config.h"

static bool s_initialized = false;

bool crypto_engine_init(void) {
    if (s_initialized) {
        return true;
    }

    /* Resolve memoria/feedback_trng_hang.md — PERIPH power domain must be
     * up before TRNG can be opened. Idempotent dependency add. */
    Power_setDependency(PowerCC26XX_PERIPH_TRNG);

    TRNG_init();

    s_initialized = true;
    return true;
}

/* Stub bodies — replaced in subsequent tasks (T4-T10). */
crypto_engine_status_t crypto_engine_random(uint8_t n, uint8_t *out) {
    (void)n;
    (void)out;
    return CRYPTO_NOT_INITIALIZED;
}

crypto_engine_status_t crypto_engine_aes_ecb(uint8_t op, const uint8_t key[16],
                                             const uint8_t in[16], uint8_t out[16]) {
    (void)op;
    (void)key;
    (void)in;
    (void)out;
    return CRYPTO_NOT_INITIALIZED;
}

crypto_engine_status_t crypto_engine_aes_ctr(uint8_t op, const uint8_t key[16],
                                             const uint8_t iv[16], const uint8_t *in, size_t len,
                                             uint8_t *out) {
    (void)op;
    (void)key;
    (void)iv;
    (void)in;
    (void)len;
    (void)out;
    return CRYPTO_NOT_INITIALIZED;
}

crypto_engine_status_t crypto_engine_aes_cbc(uint8_t op, const uint8_t key[16],
                                             const uint8_t iv[16], const uint8_t *in, size_t len,
                                             uint8_t *out) {
    (void)op;
    (void)key;
    (void)iv;
    (void)in;
    (void)len;
    (void)out;
    return CRYPTO_NOT_INITIALIZED;
}

crypto_engine_status_t crypto_engine_aes_ccm(uint8_t op, const uint8_t key[16],
                                             const uint8_t *nonce, uint8_t nonce_len,
                                             const uint8_t *aad, size_t aad_len, const uint8_t *in,
                                             size_t pt_len, uint8_t tag_len, uint8_t *out,
                                             uint8_t *tag) {
    (void)op;
    (void)key;
    (void)nonce;
    (void)nonce_len;
    (void)aad;
    (void)aad_len;
    (void)in;
    (void)pt_len;
    (void)tag_len;
    (void)out;
    (void)tag;
    return CRYPTO_NOT_INITIALIZED;
}

crypto_engine_status_t crypto_engine_aes_gcm(uint8_t op, const uint8_t key[16],
                                             const uint8_t iv[12], const uint8_t *aad,
                                             size_t aad_len, const uint8_t *in, size_t pt_len,
                                             uint8_t *out, uint8_t tag[16]) {
    (void)op;
    (void)key;
    (void)iv;
    (void)aad;
    (void)aad_len;
    (void)in;
    (void)pt_len;
    (void)out;
    (void)tag;
    return CRYPTO_NOT_INITIALIZED;
}

crypto_engine_status_t crypto_engine_sha256(const uint8_t *in, size_t len, uint8_t out[32]) {
    (void)in;
    (void)len;
    (void)out;
    return CRYPTO_NOT_INITIALIZED;
}

crypto_engine_status_t crypto_engine_ecdh(crypto_curve_t curve, const uint8_t priv[32],
                                          const uint8_t *peer_pub, size_t peer_pub_len,
                                          uint8_t shared[32]) {
    (void)curve;
    (void)priv;
    (void)peer_pub;
    (void)peer_pub_len;
    (void)shared;
    return CRYPTO_NOT_INITIALIZED;
}

crypto_engine_status_t crypto_engine_ecdsa_sign(crypto_curve_t curve, const uint8_t priv[32],
                                                const uint8_t hash[32], uint8_t sig[64]) {
    (void)curve;
    (void)priv;
    (void)hash;
    (void)sig;
    return CRYPTO_NOT_INITIALIZED;
}

crypto_engine_status_t crypto_engine_ecdsa_verify(crypto_curve_t curve, const uint8_t *pub,
                                                  size_t pub_len, const uint8_t hash[32],
                                                  const uint8_t sig[64], bool *valid) {
    (void)curve;
    (void)pub;
    (void)pub_len;
    (void)hash;
    (void)sig;
    if (valid != NULL) {
        *valid = false;
    }
    return CRYPTO_NOT_INITIALIZED;
}
