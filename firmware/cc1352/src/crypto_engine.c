/*
 * FeralRF CC1352 - Crypto engine module implementation
 *
 * Each primitive opens the corresponding TI driver with
 * RETURN_BEHAVIOR_POLLING, executes, closes. Per-call only.
 */

#include "crypto_engine.h"

#include <ti/drivers/AESCBC.h>
#include <ti/drivers/AESCCM.h>
#include <ti/drivers/AESCTR.h>
#include <ti/drivers/AESECB.h>
#include <ti/drivers/AESGCM.h>
#include <ti/drivers/cryptoutils/cryptokey/CryptoKeyPlaintext.h>
#include <ti/drivers/cryptoutils/ecc/ECCParams.h>
#include <ti/drivers/ECDH.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/drivers/SHA2.h>
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

crypto_engine_status_t crypto_engine_random(uint8_t n, uint8_t *out) {
    if (n == 0u || n > 240u || out == NULL) {
        return CRYPTO_BAD_PARAM;
    }
    if (!s_initialized)
        return CRYPTO_NOT_INITIALIZED;

    TRNG_Params params;
    TRNG_Params_init(&params);
    params.returnBehavior = TRNG_RETURN_BEHAVIOR_POLLING;

    TRNG_Handle handle = TRNG_open(CONFIG_TRNG_0, &params);
    if (handle == NULL) {
        return CRYPTO_HW_ERROR;
    }

    int_fast16_t rc = TRNG_getRandomBytes(handle, out, (size_t)n);
    TRNG_close(handle);

    if (rc != TRNG_STATUS_SUCCESS) {
        return CRYPTO_HW_ERROR;
    }
    return CRYPTO_OK;
}

crypto_engine_status_t crypto_engine_aes_ecb(uint8_t op, const uint8_t key[16],
                                             const uint8_t in[16], uint8_t out[16]) {
    if (key == NULL || in == NULL || out == NULL || op > 1u) {
        return CRYPTO_BAD_PARAM;
    }
    if (!s_initialized)
        return CRYPTO_NOT_INITIALIZED;

    AESECB_Params params;
    AESECB_Params_init(&params);
    params.returnBehavior = AESECB_RETURN_BEHAVIOR_POLLING;

    AESECB_Handle h = AESECB_open(CONFIG_AESECB_0, &params);
    if (h == NULL)
        return CRYPTO_HW_ERROR;

    CryptoKey ck;
    CryptoKeyPlaintext_initKey(&ck, (uint8_t *)key, 16);

    AESECB_Operation oper;
    AESECB_Operation_init(&oper);
    oper.key = &ck;
    oper.input = (uint8_t *)in;
    oper.output = out;
    oper.inputLength = 16;

    int_fast16_t rc;
    if (op == 0u) {
        rc = AESECB_oneStepEncrypt(h, &oper);
    } else {
        rc = AESECB_oneStepDecrypt(h, &oper);
    }

    AESECB_close(h);
    return (rc == AESECB_STATUS_SUCCESS) ? CRYPTO_OK : CRYPTO_HW_ERROR;
}

crypto_engine_status_t crypto_engine_aes_ctr(uint8_t op, const uint8_t key[16],
                                             const uint8_t iv[16], const uint8_t *in, size_t len,
                                             uint8_t *out) {
    if (key == NULL || iv == NULL || in == NULL || out == NULL || op > 1u || len == 0u) {
        return CRYPTO_BAD_PARAM;
    }
    if (!s_initialized)
        return CRYPTO_NOT_INITIALIZED;

    AESCTR_Params params;
    AESCTR_Params_init(&params);
    params.returnBehavior = AESCTR_RETURN_BEHAVIOR_POLLING;

    AESCTR_Handle h = AESCTR_open(CONFIG_AESCTR_0, &params);
    if (h == NULL)
        return CRYPTO_HW_ERROR;

    CryptoKey ck;
    CryptoKeyPlaintext_initKey(&ck, (uint8_t *)key, 16);

    AESCTR_OneStepOperation oper;
    AESCTR_OneStepOperation_init(&oper);
    oper.key = &ck;
    oper.input = (uint8_t *)in;
    oper.output = out;
    oper.inputLength = len;
    oper.initialCounter = (uint8_t *)iv;

    int_fast16_t rc;
    if (op == 0u) {
        rc = AESCTR_oneStepEncrypt(h, &oper);
    } else {
        rc = AESCTR_oneStepDecrypt(h, &oper);
    }

    AESCTR_close(h);
    return (rc == AESCTR_STATUS_SUCCESS) ? CRYPTO_OK : CRYPTO_HW_ERROR;
}

crypto_engine_status_t crypto_engine_aes_cbc(uint8_t op, const uint8_t key[16],
                                             const uint8_t iv[16], const uint8_t *in, size_t len,
                                             uint8_t *out) {
    if (key == NULL || iv == NULL || in == NULL || out == NULL || op > 1u || len == 0u ||
        (len % 16u) != 0u) {
        return CRYPTO_BAD_PARAM;
    }
    if (!s_initialized)
        return CRYPTO_NOT_INITIALIZED;

    AESCBC_Params params;
    AESCBC_Params_init(&params);
    params.returnBehavior = AESCBC_RETURN_BEHAVIOR_POLLING;

    AESCBC_Handle h = AESCBC_open(CONFIG_AESCBC_0, &params);
    if (h == NULL)
        return CRYPTO_HW_ERROR;

    CryptoKey ck;
    CryptoKeyPlaintext_initKey(&ck, (uint8_t *)key, 16);

    AESCBC_OneStepOperation oper;
    AESCBC_OneStepOperation_init(&oper);
    oper.key = &ck;
    oper.input = (uint8_t *)in;
    oper.output = out;
    oper.inputLength = len;
    oper.iv = (uint8_t *)iv;

    int_fast16_t rc;
    if (op == 0u) {
        rc = AESCBC_oneStepEncrypt(h, &oper);
    } else {
        rc = AESCBC_oneStepDecrypt(h, &oper);
    }

    AESCBC_close(h);
    return (rc == AESCBC_STATUS_SUCCESS) ? CRYPTO_OK : CRYPTO_HW_ERROR;
}

crypto_engine_status_t crypto_engine_aes_ccm(uint8_t op, const uint8_t key[16],
                                             const uint8_t *nonce, uint8_t nonce_len,
                                             const uint8_t *aad, size_t aad_len, const uint8_t *in,
                                             size_t pt_len, uint8_t tag_len, uint8_t *out,
                                             uint8_t *tag) {
    if (key == NULL || nonce == NULL || out == NULL || tag == NULL || op > 1u) {
        return CRYPTO_BAD_PARAM;
    }
    if (nonce_len < 7u || nonce_len > 13u)
        return CRYPTO_BAD_PARAM;
    if (tag_len != 8u && tag_len != 16u)
        return CRYPTO_BAD_PARAM;
    if (!s_initialized)
        return CRYPTO_NOT_INITIALIZED;

    AESCCM_Params params;
    AESCCM_Params_init(&params);
    params.returnBehavior = AESCCM_RETURN_BEHAVIOR_POLLING;

    AESCCM_Handle h = AESCCM_open(CONFIG_AESCCM_0, &params);
    if (h == NULL)
        return CRYPTO_HW_ERROR;

    CryptoKey ck;
    CryptoKeyPlaintext_initKey(&ck, (uint8_t *)key, 16);

    AESCCM_OneStepOperation oper;
    AESCCM_OneStepOperation_init(&oper);
    oper.key = &ck;
    oper.aad = (uint8_t *)aad;
    oper.aadLength = aad_len;
    oper.input = (uint8_t *)in;
    oper.output = out;
    oper.inputLength = pt_len;
    oper.nonce = (uint8_t *)nonce;
    oper.nonceLength = nonce_len;
    oper.mac = tag;
    oper.macLength = tag_len;

    int_fast16_t rc;
    if (op == 0u) {
        rc = AESCCM_oneStepEncrypt(h, &oper);
    } else {
        rc = AESCCM_oneStepDecrypt(h, &oper);
    }

    AESCCM_close(h);
    if (rc == AESCCM_STATUS_SUCCESS)
        return CRYPTO_OK;
    if (rc == AESCCM_STATUS_MAC_INVALID)
        return CRYPTO_TAG_MISMATCH;
    return CRYPTO_HW_ERROR;
}

crypto_engine_status_t crypto_engine_aes_gcm(uint8_t op, const uint8_t key[16],
                                             const uint8_t iv[12], const uint8_t *aad,
                                             size_t aad_len, const uint8_t *in, size_t pt_len,
                                             uint8_t *out, uint8_t tag[16]) {
    if (key == NULL || iv == NULL || out == NULL || tag == NULL || op > 1u) {
        return CRYPTO_BAD_PARAM;
    }
    if (!s_initialized)
        return CRYPTO_NOT_INITIALIZED;

    AESGCM_Params params;
    AESGCM_Params_init(&params);
    params.returnBehavior = AESGCM_RETURN_BEHAVIOR_POLLING;

    AESGCM_Handle h = AESGCM_open(CONFIG_AESGCM_0, &params);
    if (h == NULL)
        return CRYPTO_HW_ERROR;

    CryptoKey ck;
    CryptoKeyPlaintext_initKey(&ck, (uint8_t *)key, 16);

    AESGCM_OneStepOperation oper;
    AESGCM_OneStepOperation_init(&oper);
    oper.key = &ck;
    oper.aad = (uint8_t *)aad;
    oper.aadLength = aad_len;
    oper.input = (uint8_t *)in;
    oper.output = out;
    oper.inputLength = pt_len;
    oper.iv = (uint8_t *)iv;
    oper.ivLength = 12;
    oper.mac = tag;
    oper.macLength = 16;

    int_fast16_t rc;
    if (op == 0u) {
        rc = AESGCM_oneStepEncrypt(h, &oper);
    } else {
        rc = AESGCM_oneStepDecrypt(h, &oper);
    }

    AESGCM_close(h);
    if (rc == AESGCM_STATUS_SUCCESS)
        return CRYPTO_OK;
    if (rc == AESGCM_STATUS_MAC_INVALID)
        return CRYPTO_TAG_MISMATCH;
    return CRYPTO_HW_ERROR;
}

crypto_engine_status_t crypto_engine_sha256(const uint8_t *in, size_t len, uint8_t out[32]) {
    if (out == NULL)
        return CRYPTO_BAD_PARAM;
    if (len > 0u && in == NULL)
        return CRYPTO_BAD_PARAM;
    if (!s_initialized)
        return CRYPTO_NOT_INITIALIZED;

    SHA2_Params params;
    SHA2_Params_init(&params);
    params.returnBehavior = SHA2_RETURN_BEHAVIOR_POLLING;
    params.hashType = SHA2_HASH_TYPE_256;

    SHA2_Handle h = SHA2_open(CONFIG_SHA2_0, &params);
    if (h == NULL)
        return CRYPTO_HW_ERROR;

    int_fast16_t rc = SHA2_hashData(h, (uint8_t *)in, len, out);
    SHA2_close(h);

    return (rc == SHA2_STATUS_SUCCESS) ? CRYPTO_OK : CRYPTO_HW_ERROR;
}

crypto_engine_status_t crypto_engine_ecdh(crypto_curve_t curve, const uint8_t priv[32],
                                          const uint8_t *peer_pub, size_t peer_pub_len,
                                          uint8_t shared[32]) {
    if (priv == NULL || peer_pub == NULL || shared == NULL)
        return CRYPTO_BAD_PARAM;
    if (!s_initialized)
        return CRYPTO_NOT_INITIALIZED;

    const ECCParams_CurveParams *cparams;
    bool is_montgomery;

    if (curve == CRYPTO_CURVE_P256) {
        cparams = &ECCParams_NISTP256;
        is_montgomery = false;
        /* TI big-endian P-256 public key: [0x04 | X(32) | Y(32)] = 65 bytes.
         * Protocol passes raw X||Y (64 bytes); validate that. */
        if (peer_pub_len != 64u)
            return CRYPTO_BAD_PARAM;
    } else if (curve == CRYPTO_CURVE_25519) {
        cparams = &ECCParams_Curve25519;
        is_montgomery = true;
        /* Curve25519 X-only little-endian: 32 bytes. */
        if (peer_pub_len != 32u)
            return CRYPTO_BAD_PARAM;
    } else {
        return CRYPTO_UNSUPPORTED_CURVE;
    }

    ECDH_Params params;
    ECDH_Params_init(&params);
    params.returnBehavior = ECDH_RETURN_BEHAVIOR_POLLING;

    ECDH_Handle h = ECDH_open(CONFIG_ECDH_0, &params);
    if (h == NULL)
        return CRYPTO_HW_ERROR;

    CryptoKey priv_key;
    CryptoKeyPlaintext_initKey(&priv_key, (uint8_t *)priv, 32u);

    int_fast16_t rc;
    crypto_engine_status_t ret;

    if (!is_montgomery) {
        /* P-256: construct 65-byte [0x04 | X | Y] peer public key buffer. */
        uint8_t peer_buf[65];
        peer_buf[0] = 0x04u;
        for (size_t i = 0u; i < 64u; i++) {
            peer_buf[1u + i] = peer_pub[i];
        }
        CryptoKey peer_key;
        CryptoKeyPlaintext_initKey(&peer_key, peer_buf, sizeof(peer_buf));

        /* Shared secret output: [0x04 | X(32) | Y(32)] = 65 bytes. */
        uint8_t shared_buf[65];
        CryptoKey shared_key;
        CryptoKeyPlaintext_initBlankKey(&shared_key, shared_buf, sizeof(shared_buf));

        ECDH_OperationComputeSharedSecret oper;
        ECDH_OperationComputeSharedSecret_init(&oper);
        oper.curve = cparams;
        oper.myPrivateKey = &priv_key;
        oper.theirPublicKey = &peer_key;
        oper.sharedSecret = &shared_key;
        oper.keyMaterialEndianness = ECDH_BIG_ENDIAN_KEY;

        rc = ECDH_computeSharedSecret(h, &oper);
        ECDH_close(h);

        if (rc != ECDH_STATUS_SUCCESS) {
            ret = (rc == ECDH_STATUS_PUBLIC_KEY_NOT_ON_CURVE) ? CRYPTO_BAD_PARAM : CRYPTO_HW_ERROR;
        } else {
            /* X coordinate is bytes [1..32] of the 65-byte output. */
            for (size_t i = 0u; i < 32u; i++) {
                shared[i] = shared_buf[1u + i];
            }
            ret = CRYPTO_OK;
        }
    } else {
        /* Curve25519: X-only little-endian format. */
        CryptoKey peer_key;
        CryptoKeyPlaintext_initKey(&peer_key, (uint8_t *)peer_pub, 32u);

        uint8_t shared_buf[32];
        CryptoKey shared_key;
        CryptoKeyPlaintext_initBlankKey(&shared_key, shared_buf, sizeof(shared_buf));

        ECDH_OperationComputeSharedSecret oper;
        ECDH_OperationComputeSharedSecret_init(&oper);
        oper.curve = cparams;
        oper.myPrivateKey = &priv_key;
        oper.theirPublicKey = &peer_key;
        oper.sharedSecret = &shared_key;
        oper.keyMaterialEndianness = ECDH_LITTLE_ENDIAN_KEY;

        rc = ECDH_computeSharedSecret(h, &oper);
        ECDH_close(h);

        if (rc != ECDH_STATUS_SUCCESS) {
            ret = (rc == ECDH_STATUS_PUBLIC_KEY_NOT_ON_CURVE) ? CRYPTO_BAD_PARAM : CRYPTO_HW_ERROR;
        } else {
            for (size_t i = 0u; i < 32u; i++) {
                shared[i] = shared_buf[i];
            }
            ret = CRYPTO_OK;
        }
    }

    return ret;
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
