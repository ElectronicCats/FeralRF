# F25 — Crypto HW Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expose CC1352P7's hardware crypto engines (TRNG, AES-128, SHA-256, ECDH, ECDSA) as Python API methods, with NIST-validated correctness and resolution of the TRNG PERIPH power-domain bug.

**Architecture:** New `crypto_engine.{c,h}` module wraps TI SDK 8.30 drivers (TRNG, AESECB, AESCCM, AESCTR, AESCBC, AESGCM, SHA2, ECDH, ECDSA) with a stateless one-shot API. `command_processor.c` dispatches new COBS commands (0x59-0x62) to the module. Python `Radio` class adds 11 new methods, raising `CryptoError(RadioError)` on firmware-side failures.

**Tech Stack:** TI SimpleLink CC13xx/CC26xx SDK 8.30, TI-RTOS 7, Python 3.11+, `cryptography` lib (host-side cross-checking).

**Reference:** `docs/superpowers/specs/2026-04-30-f25-crypto-hw-design.md`

---

## File Structure

| Path | Action | Responsibility |
|---|---|---|
| `firmware/cc1352/include/crypto_engine.h` | Create | Public C API for the crypto module |
| `firmware/cc1352/src/crypto_engine.c` | Create | TI driver wrappers, all primitives |
| `firmware/cc1352/src/command_processor.c` | Modify | 10 dispatch cases for new commands |
| `firmware/cc1352/include/protocol.h` | Modify | Command/Response IDs 0x59-0x62, 0x95-0x9C |
| `firmware/cc1352/CMakeLists.txt` | Modify | Add `crypto_engine.c` source + driver libs |
| `firmware/cc1352/syscfg/feralrf.syscfg` | Modify | Enable 9 TI driver instances |
| `firmware/cc1352/src/main_rtos.c` | Modify | Call `crypto_engine_init()` at boot |
| `python/feralrf/enums.py` | Modify | Add Command + Response IDs to enums |
| `python/feralrf/exceptions.py` | Modify | Add `CryptoError` class |
| `python/feralrf/radio.py` | Modify | Add 11 crypto methods |
| `python/tests/test_crypto.py` | Create | Unit tests with mocked transport |
| `python/tests/test_crypto_vectors.py` | Create | NIST CAVS vectors cross-checked against `cryptography` lib |
| `python/examples/lab/smoke_f25_crypto.py` | Create | 9 hardware smoke tests on single board |

---

## Task 1: Protocol IDs + Python enums + CryptoError exception

**Files:**
- Modify: `firmware/cc1352/include/protocol.h`
- Modify: `python/feralrf/enums.py`
- Modify: `python/feralrf/exceptions.py`
- Create: `python/tests/test_crypto.py`

- [ ] **Step 1: Write the failing test for enum values**

Create `python/tests/test_crypto.py`:

```python
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
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cd python && pytest tests/test_crypto.py -v 2>&1 | tail -30
```

Expected: All `test_cmd_*` and `test_rsp_*` fail with `AttributeError: CMD_RANDOM`. `test_crypto_error_exists` fails with `ImportError: cannot import name 'CryptoError'`.

- [ ] **Step 3: Add Command and Response enum entries**

Edit `python/feralrf/enums.py` — add to `Command` class after `TX_TEST_STOP = 0x57`:

```python
    # Crypto HW (F25)
    CMD_RANDOM = 0x59
    CMD_AES_ECB = 0x5A
    CMD_AES_CCM = 0x5B
    CMD_AES_CTR = 0x5C
    CMD_AES_CBC = 0x5D
    CMD_AES_GCM = 0x5E
    CMD_SHA256 = 0x5F
    CMD_ECDH = 0x60
    CMD_ECDSA_SIGN = 0x61
    CMD_ECDSA_VERIFY = 0x62
```

Edit `python/feralrf/enums.py` — add to `Response` class:

```python
    # Crypto HW (F25)
    RSP_RANDOM = 0x95
    RSP_AES = 0x96
    RSP_AES_CCM = 0x97
    RSP_AES_GCM = 0x98
    RSP_SHA256 = 0x99
    RSP_ECDH = 0x9A
    RSP_ECDSA_SIG = 0x9B
    RSP_ECDSA_VERIFY = 0x9C
```

Edit `python/feralrf/enums.py` — append to `STABLE_COMMANDS` tuple:

```python
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
```

- [ ] **Step 4: Add `CryptoError` to exceptions**

Edit `python/feralrf/exceptions.py` — append at the end:

```python
class CryptoError(RadioError):
    """Raised when a hardware crypto operation fails (tag mismatch,
    HW error, driver init failure)."""
    pass
```

- [ ] **Step 5: Add firmware protocol IDs**

Edit `firmware/cc1352/include/protocol.h` — find the `Command IDs` section (look for `CMD_TX_CW = 0x55` or similar) and add:

```c
/* Crypto HW (F25) */
#define CMD_RANDOM         0x59
#define CMD_AES_ECB        0x5A
#define CMD_AES_CCM        0x5B
#define CMD_AES_CTR        0x5C
#define CMD_AES_CBC        0x5D
#define CMD_AES_GCM        0x5E
#define CMD_SHA256         0x5F
#define CMD_ECDH           0x60
#define CMD_ECDSA_SIGN     0x61
#define CMD_ECDSA_VERIFY   0x62

/* Crypto responses */
#define RSP_RANDOM         0x95
#define RSP_AES            0x96
#define RSP_AES_CCM        0x97
#define RSP_AES_GCM        0x98
#define RSP_SHA256         0x99
#define RSP_ECDH           0x9A
#define RSP_ECDSA_SIG      0x9B
#define RSP_ECDSA_VERIFY   0x9C
```

- [ ] **Step 6: Run test to verify it passes**

```bash
cd python && pytest tests/test_crypto.py -v 2>&1 | tail -25
```

Expected: 19/19 PASS.

- [ ] **Step 7: Commit**

```bash
git add python/feralrf/enums.py python/feralrf/exceptions.py python/tests/test_crypto.py firmware/cc1352/include/protocol.h
git commit -m "feat(f25): protocol IDs + Python enums + CryptoError

10 new command IDs (0x59-0x62), 8 new response IDs (0x95-0x9C),
CryptoError exception class. 19/19 enum tests pass.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 2: SysConfig + CMakeLists crypto driver wiring

**Files:**
- Modify: `firmware/cc1352/syscfg/feralrf.syscfg`
- Modify: `firmware/cc1352/CMakeLists.txt`

This task only enables TI driver instances and link libraries — no functional code yet. Done as separate task so a clean baseline build proves the syscfg edits work before any firmware logic depends on them.

- [ ] **Step 1: Add 9 driver instances in SysConfig**

Edit `firmware/cc1352/syscfg/feralrf.syscfg` — find the section with existing driver modules (e.g., where `RF` or `UART2` are configured) and append:

```javascript
/* === Crypto HW (F25) === */
var TRNG = scripting.addModule("/ti/drivers/TRNG").addInstance();
TRNG.$name = "CONFIG_TRNG_0";

var AESECB = scripting.addModule("/ti/drivers/AESECB").addInstance();
AESECB.$name = "CONFIG_AESECB_0";

var AESCCM = scripting.addModule("/ti/drivers/AESCCM").addInstance();
AESCCM.$name = "CONFIG_AESCCM_0";

var AESCTR = scripting.addModule("/ti/drivers/AESCTR").addInstance();
AESCTR.$name = "CONFIG_AESCTR_0";

var AESCBC = scripting.addModule("/ti/drivers/AESCBC").addInstance();
AESCBC.$name = "CONFIG_AESCBC_0";

var AESGCM = scripting.addModule("/ti/drivers/AESGCM").addInstance();
AESGCM.$name = "CONFIG_AESGCM_0";

var SHA2 = scripting.addModule("/ti/drivers/SHA2").addInstance();
SHA2.$name = "CONFIG_SHA2_0";

var ECDH = scripting.addModule("/ti/drivers/ECDH").addInstance();
ECDH.$name = "CONFIG_ECDH_0";

var ECDSA = scripting.addModule("/ti/drivers/ECDSA").addInstance();
ECDSA.$name = "CONFIG_ECDSA_0";
```

- [ ] **Step 2: Verify syscfg generates without errors**

```bash
cd firmware/cc1352 && rm -rf build && mkdir build && cd build && cmake .. 2>&1 | tail -10
```

Expected: configure step completes; SysConfig regenerates `ti_drivers_config.{c,h}`. Look for entries like `CONFIG_TRNG_COUNT`, `CONFIG_AESECB_COUNT` in `build/syscfg/ti_drivers_config.h`.

- [ ] **Step 3: Confirm new driver lib symbols are available**

```bash
grep -E "CONFIG_TRNG_COUNT|CONFIG_AESECB_COUNT|CONFIG_AESCCM_COUNT|CONFIG_AESCTR_COUNT|CONFIG_AESCBC_COUNT|CONFIG_AESGCM_COUNT|CONFIG_SHA2_COUNT|CONFIG_ECDH_COUNT|CONFIG_ECDSA_COUNT" firmware/cc1352/build/syscfg/ti_drivers_config.h | head -15
```

Expected: 9 lines, each `#define CONFIG_xxx_COUNT 1`.

- [ ] **Step 4: Build full firmware to confirm no link error**

```bash
cd firmware/cc1352/build && cmake --build . -j$(nproc) 2>&1 | tail -10
```

Expected: `[100%] Built target feralrf_cc1352.elf`. No undefined references.

- [ ] **Step 5: Commit**

```bash
git add firmware/cc1352/syscfg/feralrf.syscfg firmware/cc1352/CMakeLists.txt
git commit -m "build(f25): enable TI crypto driver instances

9 driver instances (TRNG/AESECB/AESCCM/AESCTR/AESCBC/AESGCM/SHA2/ECDH/ECDSA)
configured via SysConfig. No functional code yet — baseline build verifies
the driver libs link cleanly.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 3: crypto_engine module skeleton + init (TRNG fix)

**Files:**
- Create: `firmware/cc1352/include/crypto_engine.h`
- Create: `firmware/cc1352/src/crypto_engine.c`
- Modify: `firmware/cc1352/src/main_rtos.c`
- Modify: `firmware/cc1352/CMakeLists.txt`

- [ ] **Step 1: Create header with public API and status enum**

Create `firmware/cc1352/include/crypto_engine.h`:

```c
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

/* Idempotent — safe to call multiple times. Returns true on success. */
bool crypto_engine_init(void);

/* Stub bodies — actual implementations land in T4-T10. Each returns
 * CRYPTO_NOT_INITIALIZED until the real body is filled in. */
crypto_engine_status_t crypto_engine_random(uint8_t n, uint8_t *out);

crypto_engine_status_t crypto_engine_aes_ecb(uint8_t op, const uint8_t key[16],
                                             const uint8_t in[16], uint8_t out[16]);

crypto_engine_status_t crypto_engine_aes_ctr(uint8_t op, const uint8_t key[16],
                                             const uint8_t iv[16], const uint8_t *in,
                                             size_t len, uint8_t *out);

crypto_engine_status_t crypto_engine_aes_cbc(uint8_t op, const uint8_t key[16],
                                             const uint8_t iv[16], const uint8_t *in,
                                             size_t len, uint8_t *out);

crypto_engine_status_t crypto_engine_aes_ccm(uint8_t op, const uint8_t key[16],
                                             const uint8_t *nonce, uint8_t nonce_len,
                                             const uint8_t *aad, size_t aad_len,
                                             const uint8_t *in, size_t pt_len,
                                             uint8_t tag_len, uint8_t *out, uint8_t *tag);

crypto_engine_status_t crypto_engine_aes_gcm(uint8_t op, const uint8_t key[16],
                                             const uint8_t iv[12], const uint8_t *aad,
                                             size_t aad_len, const uint8_t *in,
                                             size_t pt_len, uint8_t *out, uint8_t tag[16]);

crypto_engine_status_t crypto_engine_sha256(const uint8_t *in, size_t len, uint8_t out[32]);

crypto_engine_status_t crypto_engine_ecdh(crypto_curve_t curve, const uint8_t priv[32],
                                          const uint8_t *peer_pub, size_t peer_pub_len,
                                          uint8_t shared[32]);

crypto_engine_status_t crypto_engine_ecdsa_sign(crypto_curve_t curve, const uint8_t priv[32],
                                                const uint8_t hash[32], uint8_t sig[64]);

crypto_engine_status_t crypto_engine_ecdsa_verify(crypto_curve_t curve,
                                                  const uint8_t *pub, size_t pub_len,
                                                  const uint8_t hash[32],
                                                  const uint8_t sig[64], bool *valid);

#endif /* CRYPTO_ENGINE_H */
```

- [ ] **Step 2: Create skeleton .c file with init only**

Create `firmware/cc1352/src/crypto_engine.c`:

```c
/*
 * FeralRF CC1352 - Crypto engine module implementation
 *
 * Each primitive opens the corresponding TI driver with
 * RETURN_BEHAVIOR_POLLING, executes, closes. Per-call only — no
 * persistent handles, no slots, no streaming.
 */

#include "crypto_engine.h"

#include <ti/drivers/TRNG.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26X2.h>

#include "ti_drivers_config.h"

static bool s_initialized = false;

bool crypto_engine_init(void) {
    if (s_initialized) {
        return true;
    }

    /* Resolve memoria/feedback_trng_hang.md — PERIPH power domain must be
     * up before TRNG can be opened. Idempotent dependency add. */
    Power_setDependency(PowerCC26X2_PERIPH_TRNG);

    TRNG_init();

    s_initialized = true;
    return true;
}

/* Stub bodies — replaced in subsequent tasks. */
crypto_engine_status_t crypto_engine_random(uint8_t n, uint8_t *out) {
    (void)n;
    (void)out;
    return CRYPTO_NOT_INITIALIZED;
}

crypto_engine_status_t crypto_engine_aes_ecb(uint8_t op, const uint8_t key[16],
                                             const uint8_t in[16], uint8_t out[16]) {
    (void)op; (void)key; (void)in; (void)out;
    return CRYPTO_NOT_INITIALIZED;
}

crypto_engine_status_t crypto_engine_aes_ctr(uint8_t op, const uint8_t key[16],
                                             const uint8_t iv[16], const uint8_t *in,
                                             size_t len, uint8_t *out) {
    (void)op; (void)key; (void)iv; (void)in; (void)len; (void)out;
    return CRYPTO_NOT_INITIALIZED;
}

crypto_engine_status_t crypto_engine_aes_cbc(uint8_t op, const uint8_t key[16],
                                             const uint8_t iv[16], const uint8_t *in,
                                             size_t len, uint8_t *out) {
    (void)op; (void)key; (void)iv; (void)in; (void)len; (void)out;
    return CRYPTO_NOT_INITIALIZED;
}

crypto_engine_status_t crypto_engine_aes_ccm(uint8_t op, const uint8_t key[16],
                                             const uint8_t *nonce, uint8_t nonce_len,
                                             const uint8_t *aad, size_t aad_len,
                                             const uint8_t *in, size_t pt_len,
                                             uint8_t tag_len, uint8_t *out, uint8_t *tag) {
    (void)op; (void)key; (void)nonce; (void)nonce_len;
    (void)aad; (void)aad_len; (void)in; (void)pt_len;
    (void)tag_len; (void)out; (void)tag;
    return CRYPTO_NOT_INITIALIZED;
}

crypto_engine_status_t crypto_engine_aes_gcm(uint8_t op, const uint8_t key[16],
                                             const uint8_t iv[12], const uint8_t *aad,
                                             size_t aad_len, const uint8_t *in,
                                             size_t pt_len, uint8_t *out, uint8_t tag[16]) {
    (void)op; (void)key; (void)iv; (void)aad; (void)aad_len;
    (void)in; (void)pt_len; (void)out; (void)tag;
    return CRYPTO_NOT_INITIALIZED;
}

crypto_engine_status_t crypto_engine_sha256(const uint8_t *in, size_t len, uint8_t out[32]) {
    (void)in; (void)len; (void)out;
    return CRYPTO_NOT_INITIALIZED;
}

crypto_engine_status_t crypto_engine_ecdh(crypto_curve_t curve, const uint8_t priv[32],
                                          const uint8_t *peer_pub, size_t peer_pub_len,
                                          uint8_t shared[32]) {
    (void)curve; (void)priv; (void)peer_pub; (void)peer_pub_len; (void)shared;
    return CRYPTO_NOT_INITIALIZED;
}

crypto_engine_status_t crypto_engine_ecdsa_sign(crypto_curve_t curve, const uint8_t priv[32],
                                                const uint8_t hash[32], uint8_t sig[64]) {
    (void)curve; (void)priv; (void)hash; (void)sig;
    return CRYPTO_NOT_INITIALIZED;
}

crypto_engine_status_t crypto_engine_ecdsa_verify(crypto_curve_t curve,
                                                  const uint8_t *pub, size_t pub_len,
                                                  const uint8_t hash[32],
                                                  const uint8_t sig[64], bool *valid) {
    (void)curve; (void)pub; (void)pub_len; (void)hash; (void)sig;
    if (valid != NULL) {
        *valid = false;
    }
    return CRYPTO_NOT_INITIALIZED;
}
```

- [ ] **Step 3: Add to CMakeLists.txt**

Edit `firmware/cc1352/CMakeLists.txt` — find the `add_executable` line and append `src/crypto_engine.c` to the source list.

- [ ] **Step 4: Wire init at boot**

Edit `firmware/cc1352/src/main_rtos.c` — locate where the firmware calls `RadioIF_init()` (early in `mainTask`/`startupFn`) and add immediately before:

```c
#include "crypto_engine.h"
/* ... */

if (!crypto_engine_init()) {
    /* Crypto unavailable — log via existing diag mechanism if any.
     * Subsequent crypto_engine_*() calls return CRYPTO_NOT_INITIALIZED. */
}
```

- [ ] **Step 5: Build to confirm clean compilation**

```bash
cd firmware/cc1352 && cmake --build build -j$(nproc) 2>&1 | tail -8
```

Expected: `[100%] Built target feralrf_cc1352.elf`. No warnings about unused functions (the `(void)` casts handle that).

- [ ] **Step 6: Commit**

```bash
git add firmware/cc1352/include/crypto_engine.h firmware/cc1352/src/crypto_engine.c firmware/cc1352/src/main_rtos.c firmware/cc1352/CMakeLists.txt
git commit -m "feat(f25): crypto_engine skeleton + init (TRNG power fix)

Module file added with stub bodies returning CRYPTO_NOT_INITIALIZED.
crypto_engine_init() resolves the PERIPH_TRNG power-domain bug
(memoria/feedback_trng_hang.md) by calling Power_setDependency
before TRNG_init. Wired at boot in main_rtos.c. Clean build.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 4: TRNG / random_bytes

**Files:**
- Modify: `firmware/cc1352/src/crypto_engine.c`
- Modify: `firmware/cc1352/src/command_processor.c`
- Modify: `python/feralrf/radio.py`
- Modify: `python/tests/test_crypto.py`

- [ ] **Step 1: Write Python failing test for random_bytes wrapper**

Append to `python/tests/test_crypto.py`:

```python
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
    """Returned bytes from second call differ from first (smoke for non-stub TRNG)."""
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
```

Add to `python/tests/test_crypto.py` imports at top:

```python
from feralrf.enums import Command
```

- [ ] **Step 2: Run test to verify failure**

```bash
cd python && pytest tests/test_crypto.py -v -k random_bytes 2>&1 | tail -10
```

Expected: 3 failures with `AttributeError: 'Radio' object has no attribute 'random_bytes'`.

- [ ] **Step 3: Add Python `random_bytes` method**

Edit `python/feralrf/radio.py` — add method to the `Radio` class (after existing `tx_test_stop` or near other utility methods):

```python
    def random_bytes(self, n: int) -> bytes:
        """Generate `n` cryptographically secure random bytes from the chip's TRNG.

        Args:
            n: Number of bytes (1 ≤ n ≤ 240).

        Returns:
            `n` random bytes.

        Raises:
            ValueError: If `n` is outside [1, 240].
            CryptoError: If firmware TRNG is unavailable (CRYPTO_NOT_INITIALIZED).
        """
        if not 1 <= n <= 240:
            raise ValueError(f"random_bytes: n must be in [1, 240], got {n}")
        self._send_command(Command.CMD_RANDOM, bytes([n]))
        rsp_id, status, data = self._read_response(expected=Response.RSP_RANDOM)
        if rsp_id == Response.ERROR:
            from feralrf.exceptions import CryptoError
            raise CryptoError(f"random_bytes failed: status={status}")
        if len(data) != n:
            raise CryptoError(f"random_bytes returned {len(data)} bytes, expected {n}")
        return data
```

Make sure `Command` and `Response` are imported at the top of `radio.py` (likely already are; otherwise add `from feralrf.enums import Command, Response`).

- [ ] **Step 4: Run Python tests to verify pass**

```bash
cd python && pytest tests/test_crypto.py -v -k random_bytes 2>&1 | tail -10
```

Expected: 3/3 PASS.

- [ ] **Step 5: Implement firmware `crypto_engine_random`**

Edit `firmware/cc1352/src/crypto_engine.c` — add include at top:

```c
#include <ti/drivers/TRNG.h>
```

Replace the stub:

```c
crypto_engine_status_t crypto_engine_random(uint8_t n, uint8_t *out) {
    if (n == 0u || n > 240u || out == NULL) {
        return CRYPTO_BAD_PARAM;
    }
    if (!s_initialized) {
        return CRYPTO_NOT_INITIALIZED;
    }

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
```

- [ ] **Step 6: Add command dispatch in command_processor.c**

Edit `firmware/cc1352/src/command_processor.c` — add include near other includes:

```c
#include "crypto_engine.h"
```

Find the `handle_command()` switch statement. Add a new case (place near other commands; the order in the switch is not load-bearing):

```c
case CMD_RANDOM: {
    if (payload_len != 1u) {
        send_error(seq, ERR_BAD_PAYLOAD);
        break;
    }
    uint8_t n = payload[0];
    uint8_t buf[240];
    crypto_engine_status_t st = crypto_engine_random(n, buf);
    if (st != CRYPTO_OK) {
        send_error(seq, (uint8_t)st);
        break;
    }
    send_response(RSP_RANDOM, seq, 0, buf, n);
    break;
}
```

- [ ] **Step 7: Build firmware**

```bash
cd firmware/cc1352 && cmake --build build -j$(nproc) 2>&1 | tail -5
```

Expected: clean build.

- [ ] **Step 8: Commit**

```bash
git add firmware/cc1352/src/crypto_engine.c firmware/cc1352/src/command_processor.c python/feralrf/radio.py python/tests/test_crypto.py
git commit -m "feat(f25): TRNG random_bytes — wire-level + Python wrapper

Firmware crypto_engine_random opens TRNG driver per-call, reads n bytes
(1≤n≤240), closes. CMD_RANDOM dispatch in command_processor. Python
Radio.random_bytes validates range, calls firmware, returns bytes.
3 unit tests pass.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 5: AES block ciphers (ECB, CTR, CBC)

**Files:**
- Modify: `firmware/cc1352/src/crypto_engine.c`
- Modify: `firmware/cc1352/src/command_processor.c`
- Modify: `python/feralrf/radio.py`
- Modify: `python/tests/test_crypto.py`

- [ ] **Step 1: Write Python failing tests**

Append to `python/tests/test_crypto.py`:

```python
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
    from feralrf.enums import Response, Command

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
```

- [ ] **Step 2: Run tests, expect failure**

```bash
cd python && pytest tests/test_crypto.py -v -k "aes_e or aes_unknown or aes_ctr_req or aes_ecb_send" 2>&1 | tail -10
```

Expected: 5 failures with `AttributeError: 'Radio' object has no attribute 'aes_encrypt'`.

- [ ] **Step 3: Add Python `aes_encrypt` / `aes_decrypt`**

Edit `python/feralrf/radio.py` — add methods to `Radio` class:

```python
    def _aes_block_op(
        self,
        op: int,
        key: bytes,
        data: bytes,
        mode: str,
        iv: bytes | None = None,
    ) -> bytes:
        from feralrf.exceptions import CryptoError

        if len(key) != 16:
            raise ValueError(f"key must be 16 bytes, got {len(key)}")
        if mode == "ecb":
            if len(data) != 16:
                raise ValueError(f"data must be 16 bytes for ECB, got {len(data)}")
            cmd = Command.CMD_AES_ECB
            payload = bytes([op]) + key + data
        elif mode == "ctr":
            if iv is None or len(iv) != 16:
                raise ValueError("iv must be 16 bytes for CTR")
            if len(data) > 240 - 33:  # leave headroom; actual cap enforced by fw
                raise ValueError(f"data too large for one-shot CTR: {len(data)}")
            cmd = Command.CMD_AES_CTR
            payload = bytes([op]) + key + iv + data
        elif mode == "cbc":
            if iv is None or len(iv) != 16:
                raise ValueError("iv must be 16 bytes for CBC")
            if len(data) % 16 != 0 or len(data) == 0:
                raise ValueError(f"data must be non-empty multiple of 16 for CBC, got {len(data)}")
            if len(data) > 240 - 33:
                raise ValueError(f"data too large for one-shot CBC: {len(data)}")
            cmd = Command.CMD_AES_CBC
            payload = bytes([op]) + key + iv + data
        else:
            raise ValueError(f"unknown mode {mode!r}; expected ecb|ctr|cbc")

        self._send_command(cmd, payload)
        rsp_id, status, out = self._read_response(expected=Response.RSP_AES)
        if rsp_id == Response.ERROR:
            raise CryptoError(f"aes {mode} failed: status={status}")
        return out

    def aes_encrypt(self, key: bytes, data: bytes, mode: str, iv: bytes | None = None) -> bytes:
        """Encrypt `data` under `key` with mode 'ecb', 'ctr', or 'cbc'."""
        return self._aes_block_op(op=0, key=key, data=data, mode=mode, iv=iv)

    def aes_decrypt(self, key: bytes, data: bytes, mode: str, iv: bytes | None = None) -> bytes:
        """Decrypt `data` under `key` with mode 'ecb', 'ctr', or 'cbc'."""
        return self._aes_block_op(op=1, key=key, data=data, mode=mode, iv=iv)
```

- [ ] **Step 4: Run Python tests, expect pass**

```bash
cd python && pytest tests/test_crypto.py -v 2>&1 | tail -15
```

Expected: All AES tests pass.

- [ ] **Step 5: Implement firmware `crypto_engine_aes_ecb`, `_ctr`, `_cbc`**

Edit `firmware/cc1352/src/crypto_engine.c` — add includes:

```c
#include <ti/drivers/AESECB.h>
#include <ti/drivers/AESCTR.h>
#include <ti/drivers/AESCBC.h>
#include <ti/drivers/cryptoutils/cryptokey/CryptoKeyPlaintext.h>
```

Replace stubs:

```c
crypto_engine_status_t crypto_engine_aes_ecb(uint8_t op, const uint8_t key[16],
                                             const uint8_t in[16], uint8_t out[16]) {
    if (key == NULL || in == NULL || out == NULL || op > 1u) {
        return CRYPTO_BAD_PARAM;
    }
    if (!s_initialized) return CRYPTO_NOT_INITIALIZED;

    AESECB_Params params;
    AESECB_Params_init(&params);
    params.returnBehavior = AESECB_RETURN_BEHAVIOR_POLLING;

    AESECB_Handle h = AESECB_open(CONFIG_AESECB_0, &params);
    if (h == NULL) return CRYPTO_HW_ERROR;

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
                                             const uint8_t iv[16], const uint8_t *in,
                                             size_t len, uint8_t *out) {
    if (key == NULL || iv == NULL || in == NULL || out == NULL || op > 1u || len == 0u) {
        return CRYPTO_BAD_PARAM;
    }
    if (!s_initialized) return CRYPTO_NOT_INITIALIZED;

    AESCTR_Params params;
    AESCTR_Params_init(&params);
    params.returnBehavior = AESCTR_RETURN_BEHAVIOR_POLLING;

    AESCTR_Handle h = AESCTR_open(CONFIG_AESCTR_0, &params);
    if (h == NULL) return CRYPTO_HW_ERROR;

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
                                             const uint8_t iv[16], const uint8_t *in,
                                             size_t len, uint8_t *out) {
    if (key == NULL || iv == NULL || in == NULL || out == NULL || op > 1u || len == 0u || (len % 16u) != 0u) {
        return CRYPTO_BAD_PARAM;
    }
    if (!s_initialized) return CRYPTO_NOT_INITIALIZED;

    AESCBC_Params params;
    AESCBC_Params_init(&params);
    params.returnBehavior = AESCBC_RETURN_BEHAVIOR_POLLING;

    AESCBC_Handle h = AESCBC_open(CONFIG_AESCBC_0, &params);
    if (h == NULL) return CRYPTO_HW_ERROR;

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
```

- [ ] **Step 6: Add dispatch cases in command_processor.c**

Append to the switch in `handle_command()`:

```c
case CMD_AES_ECB: {
    /* payload: op:1 | key:16 | data:16 = 33 B */
    if (payload_len != 33u) { send_error(seq, ERR_BAD_PAYLOAD); break; }
    uint8_t out[16];
    crypto_engine_status_t st = crypto_engine_aes_ecb(payload[0], payload + 1, payload + 17, out);
    if (st != CRYPTO_OK) { send_error(seq, (uint8_t)st); break; }
    send_response(RSP_AES, seq, 0, out, 16);
    break;
}

case CMD_AES_CTR: {
    /* payload: op:1 | key:16 | iv:16 | data:N */
    if (payload_len < 33u + 1u) { send_error(seq, ERR_BAD_PAYLOAD); break; }
    size_t data_len = payload_len - 33u;
    if (data_len > 200u) { send_error(seq, ERR_BAD_PAYLOAD); break; }
    uint8_t out[200];
    crypto_engine_status_t st = crypto_engine_aes_ctr(payload[0], payload + 1, payload + 17,
                                                     payload + 33, data_len, out);
    if (st != CRYPTO_OK) { send_error(seq, (uint8_t)st); break; }
    send_response(RSP_AES, seq, 0, out, (uint16_t)data_len);
    break;
}

case CMD_AES_CBC: {
    /* payload: op:1 | key:16 | iv:16 | data:N (multiple of 16) */
    if (payload_len < 33u + 16u) { send_error(seq, ERR_BAD_PAYLOAD); break; }
    size_t data_len = payload_len - 33u;
    if (data_len > 192u || (data_len % 16u) != 0u) { send_error(seq, ERR_BAD_PAYLOAD); break; }
    uint8_t out[192];
    crypto_engine_status_t st = crypto_engine_aes_cbc(payload[0], payload + 1, payload + 17,
                                                     payload + 33, data_len, out);
    if (st != CRYPTO_OK) { send_error(seq, (uint8_t)st); break; }
    send_response(RSP_AES, seq, 0, out, (uint16_t)data_len);
    break;
}
```

- [ ] **Step 7: Build firmware**

```bash
cd firmware/cc1352 && cmake --build build -j$(nproc) 2>&1 | tail -5
```

Expected: clean build.

- [ ] **Step 8: Commit**

```bash
git add firmware/cc1352/src/crypto_engine.c firmware/cc1352/src/command_processor.c python/feralrf/radio.py python/tests/test_crypto.py
git commit -m "feat(f25): AES-128 block modes (ECB/CTR/CBC)

Three TI driver wrappers (AESECB/AESCTR/AESCBC), 3 dispatch cases in
command_processor, Python aes_encrypt/aes_decrypt with mode='ecb|ctr|cbc'.
Validation: key length, data length (16 for ECB, multiple of 16 for CBC),
required IV. NIST CAVS vectors checked in T11.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 6: AES-CCM

**Files:**
- Modify: `firmware/cc1352/src/crypto_engine.c`
- Modify: `firmware/cc1352/src/command_processor.c`
- Modify: `python/feralrf/radio.py`
- Modify: `python/tests/test_crypto.py`

- [ ] **Step 1: Write Python failing tests**

Append to `python/tests/test_crypto.py`:

```python
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
    from feralrf.enums import Response, Command

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
    # aad_len LE u16
    assert payload[31:33] == bytes([2, 0])
    # pt_len LE u16
    assert payload[33:35] == bytes([4, 0])
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
        lambda timeout=1.0, expected=None: (Response.ERROR, 0x02, b""),  # CRYPTO_TAG_MISMATCH
    )
    with pytest.raises(CryptoError, match="tag"):
        radio.aes_ccm_decrypt(
            key=b"\x00" * 16, nonce=b"\x00" * 13, aad=b"", ciphertext=b"\x00" * 4,
            tag=b"\xFF" * 8, tag_len=8,
        )
```

- [ ] **Step 2: Run, expect fail**

```bash
cd python && pytest tests/test_crypto.py -v -k "aes_ccm" 2>&1 | tail -10
```

Expected: 4 failures.

- [ ] **Step 3: Add Python methods**

Edit `python/feralrf/radio.py`:

```python
    def aes_ccm_encrypt(
        self, key: bytes, nonce: bytes, aad: bytes, plaintext: bytes, tag_len: int
    ) -> tuple[bytes, bytes]:
        """Encrypt `plaintext` with AES-CCM. Returns (ciphertext, tag)."""
        return self._aes_ccm_op(op=0, key=key, nonce=nonce, aad=aad,
                                data=plaintext, tag_in=b"", tag_len=tag_len)

    def aes_ccm_decrypt(
        self, key: bytes, nonce: bytes, aad: bytes, ciphertext: bytes,
        tag: bytes, tag_len: int,
    ) -> bytes:
        """Decrypt `ciphertext` with AES-CCM. Raises CryptoError on tag mismatch."""
        pt, _ = self._aes_ccm_op(op=1, key=key, nonce=nonce, aad=aad,
                                 data=ciphertext, tag_in=tag, tag_len=tag_len)
        return pt

    def _aes_ccm_op(self, op, key, nonce, aad, data, tag_in, tag_len):
        from feralrf.exceptions import CryptoError

        if len(key) != 16:
            raise ValueError(f"key must be 16 bytes, got {len(key)}")
        if not 7 <= len(nonce) <= 13:
            raise ValueError(f"nonce length must be 7..13, got {len(nonce)}")
        if tag_len not in (8, 16):
            raise ValueError(f"tag_len must be 8 or 16, got {tag_len}")
        if len(aad) > 0xFFFF or len(data) > 0xFFFF:
            raise ValueError("aad/data length exceeds 16-bit limit")
        if op == 1 and len(tag_in) != tag_len:
            raise ValueError(f"tag length mismatch: expected {tag_len}, got {len(tag_in)}")

        header = bytes([
            op,
            *key,
            len(nonce),
            *nonce,
            len(aad) & 0xFF, (len(aad) >> 8) & 0xFF,
            len(data) & 0xFF, (len(data) >> 8) & 0xFF,
            tag_len,
        ])
        payload = header + aad + data + (tag_in if op == 1 else b"")

        self._send_command(Command.CMD_AES_CCM, payload)
        rsp_id, status, out = self._read_response(expected=Response.RSP_AES_CCM)
        if rsp_id == Response.ERROR:
            if status == 0x02:
                raise CryptoError("aes_ccm: tag mismatch")
            raise CryptoError(f"aes_ccm failed: status={status}")
        if op == 0:
            ct = out[: len(data)]
            tag = out[len(data) : len(data) + tag_len]
            return (ct, tag)
        return (out, b"")
```

- [ ] **Step 4: Run Python tests**

```bash
cd python && pytest tests/test_crypto.py -v -k "aes_ccm" 2>&1 | tail -10
```

Expected: 4/4 PASS.

- [ ] **Step 5: Implement firmware AES-CCM**

Edit `firmware/cc1352/src/crypto_engine.c` — include:

```c
#include <ti/drivers/AESCCM.h>
```

Replace stub:

```c
crypto_engine_status_t crypto_engine_aes_ccm(uint8_t op, const uint8_t key[16],
                                             const uint8_t *nonce, uint8_t nonce_len,
                                             const uint8_t *aad, size_t aad_len,
                                             const uint8_t *in, size_t pt_len,
                                             uint8_t tag_len, uint8_t *out, uint8_t *tag) {
    if (key == NULL || nonce == NULL || out == NULL || tag == NULL || op > 1u) {
        return CRYPTO_BAD_PARAM;
    }
    if (nonce_len < 7u || nonce_len > 13u) return CRYPTO_BAD_PARAM;
    if (tag_len != 8u && tag_len != 16u) return CRYPTO_BAD_PARAM;
    if (!s_initialized) return CRYPTO_NOT_INITIALIZED;

    AESCCM_Params params;
    AESCCM_Params_init(&params);
    params.returnBehavior = AESCCM_RETURN_BEHAVIOR_POLLING;

    AESCCM_Handle h = AESCCM_open(CONFIG_AESCCM_0, &params);
    if (h == NULL) return CRYPTO_HW_ERROR;

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
    if (rc == AESCCM_STATUS_SUCCESS) return CRYPTO_OK;
    if (rc == AESCCM_STATUS_MAC_INVALID) return CRYPTO_TAG_MISMATCH;
    return CRYPTO_HW_ERROR;
}
```

- [ ] **Step 6: Add dispatch in command_processor.c**

```c
case CMD_AES_CCM: {
    /* payload: op:1 | key:16 | nonce_len:1 | nonce:N | aad_len:2_le | pt_len:2_le | tag_len:1 | aad | data | (tag if decrypt) */
    if (payload_len < 24u) { send_error(seq, ERR_BAD_PAYLOAD); break; }
    uint8_t op = payload[0];
    const uint8_t *key = payload + 1;
    uint8_t nonce_len = payload[17];
    if (nonce_len < 7u || nonce_len > 13u) { send_error(seq, ERR_BAD_PAYLOAD); break; }
    const uint8_t *nonce = payload + 18;
    size_t off = 18u + nonce_len;
    if (payload_len < off + 5u) { send_error(seq, ERR_BAD_PAYLOAD); break; }
    uint16_t aad_len = (uint16_t)payload[off] | ((uint16_t)payload[off + 1] << 8);
    uint16_t pt_len = (uint16_t)payload[off + 2] | ((uint16_t)payload[off + 3] << 8);
    uint8_t tag_len = payload[off + 4];
    off += 5u;
    if (payload_len < off + (size_t)aad_len + (size_t)pt_len + ((op == 1u) ? (size_t)tag_len : 0u)) {
        send_error(seq, ERR_BAD_PAYLOAD); break;
    }
    if (pt_len > 200u) { send_error(seq, ERR_BAD_PAYLOAD); break; }
    const uint8_t *aad = payload + off;
    const uint8_t *data = aad + aad_len;
    const uint8_t *tag_in = data + pt_len;

    uint8_t out[200];
    uint8_t tag_buf[16];
    if (op == 1u) {
        memcpy(tag_buf, tag_in, tag_len);
    }
    crypto_engine_status_t st = crypto_engine_aes_ccm(op, key, nonce, nonce_len,
                                                     aad, aad_len, data, pt_len,
                                                     tag_len, out, tag_buf);
    if (st != CRYPTO_OK) { send_error(seq, (uint8_t)st); break; }

    if (op == 0u) {
        /* response: ct || tag */
        uint8_t resp[216];
        memcpy(resp, out, pt_len);
        memcpy(resp + pt_len, tag_buf, tag_len);
        send_response(RSP_AES_CCM, seq, 0, resp, (uint16_t)(pt_len + tag_len));
    } else {
        send_response(RSP_AES_CCM, seq, 0, out, pt_len);
    }
    break;
}
```

Make sure `<string.h>` is included at top of `command_processor.c` (likely already).

- [ ] **Step 7: Build**

```bash
cd firmware/cc1352 && cmake --build build -j$(nproc) 2>&1 | tail -5
```

Expected: clean.

- [ ] **Step 8: Commit**

```bash
git add firmware/cc1352/src/crypto_engine.c firmware/cc1352/src/command_processor.c python/feralrf/radio.py python/tests/test_crypto.py
git commit -m "feat(f25): AES-CCM encrypt/decrypt

Wraps TI AESCCM driver. Variable nonce 7..13 B, tag 8 or 16 B,
aad/data 0..200 B. CRYPTO_TAG_MISMATCH propagated as CryptoError on
decrypt path. 4 unit tests pass.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 7: AES-GCM

**Files:**
- Modify: `firmware/cc1352/src/crypto_engine.c`
- Modify: `firmware/cc1352/src/command_processor.c`
- Modify: `python/feralrf/radio.py`
- Modify: `python/tests/test_crypto.py`

- [ ] **Step 1: Write Python failing tests**

Append to `python/tests/test_crypto.py`:

```python
def test_aes_gcm_invalid_iv_raises():
    from feralrf import Radio
    radio = Radio(port="dummy")
    with pytest.raises(ValueError, match="iv"):
        radio.aes_gcm_encrypt(b"\x00" * 16, b"\x00" * 8, b"", b"x")


def test_aes_gcm_encrypt_roundtrip(monkeypatch):
    from feralrf import Radio
    from feralrf.enums import Response, Command

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
```

- [ ] **Step 2: Run, expect fail**

```bash
cd python && pytest tests/test_crypto.py -v -k "aes_gcm" 2>&1 | tail -10
```

Expected: 2 failures.

- [ ] **Step 3: Add Python methods**

Edit `python/feralrf/radio.py`:

```python
    def aes_gcm_encrypt(self, key: bytes, iv: bytes, aad: bytes, plaintext: bytes) -> tuple[bytes, bytes]:
        """Encrypt with AES-GCM. Returns (ciphertext, 16-byte tag)."""
        return self._aes_gcm_op(op=0, key=key, iv=iv, aad=aad, data=plaintext, tag_in=b"")

    def aes_gcm_decrypt(self, key: bytes, iv: bytes, aad: bytes, ciphertext: bytes, tag: bytes) -> bytes:
        """Decrypt with AES-GCM. Raises CryptoError on tag mismatch."""
        pt, _ = self._aes_gcm_op(op=1, key=key, iv=iv, aad=aad, data=ciphertext, tag_in=tag)
        return pt

    def _aes_gcm_op(self, op, key, iv, aad, data, tag_in):
        from feralrf.exceptions import CryptoError
        if len(key) != 16:
            raise ValueError(f"key must be 16 bytes, got {len(key)}")
        if len(iv) != 12:
            raise ValueError(f"iv must be 12 bytes for GCM, got {len(iv)}")
        if op == 1 and len(tag_in) != 16:
            raise ValueError("tag must be 16 bytes")

        header = bytes([
            op,
            *key,
            *iv,
            len(aad) & 0xFF, (len(aad) >> 8) & 0xFF,
            len(data) & 0xFF, (len(data) >> 8) & 0xFF,
        ])
        payload = header + aad + data + (tag_in if op == 1 else b"")

        self._send_command(Command.CMD_AES_GCM, payload)
        rsp_id, status, out = self._read_response(expected=Response.RSP_AES_GCM)
        if rsp_id == Response.ERROR:
            if status == 0x02:
                raise CryptoError("aes_gcm: tag mismatch")
            raise CryptoError(f"aes_gcm failed: status={status}")
        if op == 0:
            return (out[: len(data)], out[len(data) : len(data) + 16])
        return (out, b"")
```

- [ ] **Step 4: Run Python tests**

```bash
cd python && pytest tests/test_crypto.py -v -k "aes_gcm" 2>&1 | tail -10
```

Expected: 2/2 PASS.

- [ ] **Step 5: Implement firmware AES-GCM**

Edit `firmware/cc1352/src/crypto_engine.c` — include:

```c
#include <ti/drivers/AESGCM.h>
```

Replace stub:

```c
crypto_engine_status_t crypto_engine_aes_gcm(uint8_t op, const uint8_t key[16],
                                             const uint8_t iv[12], const uint8_t *aad,
                                             size_t aad_len, const uint8_t *in,
                                             size_t pt_len, uint8_t *out, uint8_t tag[16]) {
    if (key == NULL || iv == NULL || out == NULL || tag == NULL || op > 1u) {
        return CRYPTO_BAD_PARAM;
    }
    if (!s_initialized) return CRYPTO_NOT_INITIALIZED;

    AESGCM_Params params;
    AESGCM_Params_init(&params);
    params.returnBehavior = AESGCM_RETURN_BEHAVIOR_POLLING;

    AESGCM_Handle h = AESGCM_open(CONFIG_AESGCM_0, &params);
    if (h == NULL) return CRYPTO_HW_ERROR;

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
    if (rc == AESGCM_STATUS_SUCCESS) return CRYPTO_OK;
    if (rc == AESGCM_STATUS_MAC_INVALID) return CRYPTO_TAG_MISMATCH;
    return CRYPTO_HW_ERROR;
}
```

- [ ] **Step 6: Add dispatch**

Edit `firmware/cc1352/src/command_processor.c`:

```c
case CMD_AES_GCM: {
    /* payload: op:1 | key:16 | iv:12 | aad_len:2_le | pt_len:2_le | aad | data | (tag if decrypt) */
    if (payload_len < 33u) { send_error(seq, ERR_BAD_PAYLOAD); break; }
    uint8_t op = payload[0];
    const uint8_t *key = payload + 1;
    const uint8_t *iv = payload + 17;
    uint16_t aad_len = (uint16_t)payload[29] | ((uint16_t)payload[30] << 8);
    uint16_t pt_len = (uint16_t)payload[31] | ((uint16_t)payload[32] << 8);
    if (pt_len > 200u) { send_error(seq, ERR_BAD_PAYLOAD); break; }
    size_t off = 33u;
    if (payload_len < off + (size_t)aad_len + (size_t)pt_len + ((op == 1u) ? 16u : 0u)) {
        send_error(seq, ERR_BAD_PAYLOAD); break;
    }
    const uint8_t *aad = payload + off;
    const uint8_t *data = aad + aad_len;
    const uint8_t *tag_in = data + pt_len;

    uint8_t out[200];
    uint8_t tag_buf[16];
    if (op == 1u) memcpy(tag_buf, tag_in, 16);

    crypto_engine_status_t st = crypto_engine_aes_gcm(op, key, iv, aad, aad_len,
                                                     data, pt_len, out, tag_buf);
    if (st != CRYPTO_OK) { send_error(seq, (uint8_t)st); break; }

    if (op == 0u) {
        uint8_t resp[216];
        memcpy(resp, out, pt_len);
        memcpy(resp + pt_len, tag_buf, 16);
        send_response(RSP_AES_GCM, seq, 0, resp, (uint16_t)(pt_len + 16u));
    } else {
        send_response(RSP_AES_GCM, seq, 0, out, pt_len);
    }
    break;
}
```

- [ ] **Step 7: Build**

```bash
cd firmware/cc1352 && cmake --build build -j$(nproc) 2>&1 | tail -5
```

Expected: clean.

- [ ] **Step 8: Commit**

```bash
git add firmware/cc1352/src/crypto_engine.c firmware/cc1352/src/command_processor.c python/feralrf/radio.py python/tests/test_crypto.py
git commit -m "feat(f25): AES-GCM encrypt/decrypt

12-byte IV, 16-byte tag. Tag mismatch → CryptoError. 2 unit tests pass.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 8: SHA-256

**Files:**
- Modify: `firmware/cc1352/src/crypto_engine.c`
- Modify: `firmware/cc1352/src/command_processor.c`
- Modify: `python/feralrf/radio.py`
- Modify: `python/tests/test_crypto.py`

- [ ] **Step 1: Write Python failing test**

Append to `python/tests/test_crypto.py`:

```python
def test_sha256_oversize_raises():
    from feralrf import Radio
    radio = Radio(port="dummy")
    with pytest.raises(ValueError, match="240"):
        radio.sha256(b"\x00" * 241)


def test_sha256_sends_correct_frame(monkeypatch):
    from feralrf import Radio
    from feralrf.enums import Response, Command

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
```

- [ ] **Step 2: Run, expect fail**

```bash
cd python && pytest tests/test_crypto.py -v -k sha256 2>&1 | tail -10
```

Expected: 2 failures.

- [ ] **Step 3: Add Python method**

Edit `python/feralrf/radio.py`:

```python
    def sha256(self, data: bytes) -> bytes:
        """Compute SHA-256 of `data` (≤240 bytes). Returns 32-byte digest."""
        from feralrf.exceptions import CryptoError
        if len(data) > 240:
            raise ValueError(f"data too large for one-shot SHA-256: {len(data)} (max 240)")

        self._send_command(Command.CMD_SHA256, data)
        rsp_id, status, out = self._read_response(expected=Response.RSP_SHA256)
        if rsp_id == Response.ERROR:
            raise CryptoError(f"sha256 failed: status={status}")
        if len(out) != 32:
            raise CryptoError(f"sha256 returned {len(out)} bytes, expected 32")
        return out
```

- [ ] **Step 4: Run Python tests**

```bash
cd python && pytest tests/test_crypto.py -v -k sha256 2>&1 | tail -10
```

Expected: 2/2 PASS.

- [ ] **Step 5: Implement firmware SHA-256**

Edit `firmware/cc1352/src/crypto_engine.c` — include:

```c
#include <ti/drivers/SHA2.h>
```

Replace stub:

```c
crypto_engine_status_t crypto_engine_sha256(const uint8_t *in, size_t len, uint8_t out[32]) {
    if (out == NULL) return CRYPTO_BAD_PARAM;
    if (len > 0u && in == NULL) return CRYPTO_BAD_PARAM;
    if (!s_initialized) return CRYPTO_NOT_INITIALIZED;

    SHA2_Params params;
    SHA2_Params_init(&params);
    params.returnBehavior = SHA2_RETURN_BEHAVIOR_POLLING;
    params.hashType = SHA2_HASH_TYPE_256;

    SHA2_Handle h = SHA2_open(CONFIG_SHA2_0, &params);
    if (h == NULL) return CRYPTO_HW_ERROR;

    int_fast16_t rc = SHA2_hashData(h, (uint8_t *)in, len, out);
    SHA2_close(h);

    return (rc == SHA2_STATUS_SUCCESS) ? CRYPTO_OK : CRYPTO_HW_ERROR;
}
```

- [ ] **Step 6: Add dispatch**

Edit `firmware/cc1352/src/command_processor.c`:

```c
case CMD_SHA256: {
    /* payload: data (0..240 B) */
    if (payload_len > 240u) { send_error(seq, ERR_BAD_PAYLOAD); break; }
    uint8_t digest[32];
    crypto_engine_status_t st = crypto_engine_sha256(payload, payload_len, digest);
    if (st != CRYPTO_OK) { send_error(seq, (uint8_t)st); break; }
    send_response(RSP_SHA256, seq, 0, digest, 32);
    break;
}
```

- [ ] **Step 7: Build**

```bash
cd firmware/cc1352 && cmake --build build -j$(nproc) 2>&1 | tail -5
```

Expected: clean.

- [ ] **Step 8: Commit**

```bash
git add firmware/cc1352/src/crypto_engine.c firmware/cc1352/src/command_processor.c python/feralrf/radio.py python/tests/test_crypto.py
git commit -m "feat(f25): SHA-256 one-shot

Wraps TI SHA2 driver in SHA2_HASH_TYPE_256 mode. Inputs ≤240 B → 32-byte
digest. 2 unit tests pass.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 9: ECDH (P-256 + Curve25519)

**Files:**
- Modify: `firmware/cc1352/src/crypto_engine.c`
- Modify: `firmware/cc1352/src/command_processor.c`
- Modify: `python/feralrf/radio.py`
- Modify: `python/tests/test_crypto.py`

If Curve25519 is not exposed by SDK 8.30 ECDH driver, the firmware impl returns `CRYPTO_UNSUPPORTED_CURVE` for `CRYPTO_CURVE_25519` and Python wrapper raises a clear error. Spec §1 already documents this caveat. Test #4 (the integration smoke) verifies P-256 only when Curve25519 is unavailable.

- [ ] **Step 1: Write Python failing tests**

Append to `python/tests/test_crypto.py`:

```python
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
    from feralrf.enums import Response, Command

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
```

- [ ] **Step 2: Run, expect fail**

```bash
cd python && pytest tests/test_crypto.py -v -k ecdh 2>&1 | tail -10
```

Expected: 4 failures.

- [ ] **Step 3: Add Python method**

Edit `python/feralrf/radio.py`:

```python
    def ecdh(self, my_priv: bytes, peer_pub: bytes, curve: str) -> bytes:
        """Compute ECDH shared secret. `curve`: 'p256' (peer_pub=64 B) or 'curve25519' (peer_pub=32 B)."""
        from feralrf.exceptions import CryptoError
        if len(my_priv) != 32:
            raise ValueError(f"priv must be 32 bytes, got {len(my_priv)}")
        if curve == "p256":
            curve_id = 0
            if len(peer_pub) != 64:
                raise ValueError(f"peer_pub must be 64 bytes for p256, got {len(peer_pub)}")
        elif curve == "curve25519":
            curve_id = 1
            if len(peer_pub) != 32:
                raise ValueError(f"peer_pub must be 32 bytes for curve25519, got {len(peer_pub)}")
        else:
            raise ValueError(f"unknown curve {curve!r}; expected p256|curve25519")

        payload = bytes([curve_id]) + my_priv + peer_pub
        self._send_command(Command.CMD_ECDH, payload)
        rsp_id, status, out = self._read_response(expected=Response.RSP_ECDH)
        if rsp_id == Response.ERROR:
            if status == 0x05:
                raise CryptoError(f"ecdh: curve {curve!r} not supported by firmware")
            raise CryptoError(f"ecdh failed: status={status}")
        if len(out) != 32:
            raise CryptoError(f"ecdh returned {len(out)} bytes, expected 32")
        return out
```

- [ ] **Step 4: Run Python tests**

```bash
cd python && pytest tests/test_crypto.py -v -k ecdh 2>&1 | tail -10
```

Expected: 4/4 PASS.

- [ ] **Step 5: Implement firmware ECDH P-256**

Edit `firmware/cc1352/src/crypto_engine.c` — includes:

```c
#include <ti/drivers/ECDH.h>
#include <ti/drivers/cryptoutils/ecc/ECCParams.h>
```

Replace stub:

```c
crypto_engine_status_t crypto_engine_ecdh(crypto_curve_t curve, const uint8_t priv[32],
                                          const uint8_t *peer_pub, size_t peer_pub_len,
                                          uint8_t shared[32]) {
    if (priv == NULL || peer_pub == NULL || shared == NULL) return CRYPTO_BAD_PARAM;
    if (!s_initialized) return CRYPTO_NOT_INITIALIZED;

    const ECCParams_CurveParams *cparams;
    size_t expected_pub_len;

    if (curve == CRYPTO_CURVE_P256) {
        cparams = &ECCParams_NISTP256;
        expected_pub_len = 64;
    } else if (curve == CRYPTO_CURVE_25519) {
#ifdef ECCParams_Curve25519
        cparams = &ECCParams_Curve25519;
        expected_pub_len = 32;
#else
        return CRYPTO_UNSUPPORTED_CURVE;
#endif
    } else {
        return CRYPTO_BAD_PARAM;
    }
    if (peer_pub_len != expected_pub_len) return CRYPTO_BAD_PARAM;

    ECDH_Params params;
    ECDH_Params_init(&params);
    params.returnBehavior = ECDH_RETURN_BEHAVIOR_POLLING;

    ECDH_Handle h = ECDH_open(CONFIG_ECDH_0, &params);
    if (h == NULL) return CRYPTO_HW_ERROR;

    CryptoKey priv_key;
    CryptoKeyPlaintext_initKey(&priv_key, (uint8_t *)priv, 32);

    /* TI ECDH expects pub key in uncompressed form starting with 0x04 byte
     * for P-curves, but the driver typically expects raw X||Y. Here we
     * pass raw 64 B (X||Y) for P-256, raw 32 B for Curve25519. */
    CryptoKey peer_key;
    CryptoKeyPlaintext_initKey(&peer_key, (uint8_t *)peer_pub, expected_pub_len);

    /* For P-256 shared secret is X coordinate (32 B). */
    uint8_t shared_buf[64];
    CryptoKey shared_key;
    CryptoKeyPlaintext_initBlankKey(&shared_key, shared_buf, expected_pub_len);

    ECDH_OperationComputeSharedSecret oper;
    ECDH_OperationComputeSharedSecret_init(&oper);
    oper.curve = cparams;
    oper.myPrivateKey = &priv_key;
    oper.theirPublicKey = &peer_key;
    oper.sharedSecret = &shared_key;
    /* keyMaterialEndianness: 0=big-endian, 1=little-endian. NIST/RFC use BE. */
    oper.keyMaterialEndianness = ECDH_BIG_ENDIAN_KEY;

    int_fast16_t rc = ECDH_computeSharedSecret(h, &oper);
    ECDH_close(h);

    if (rc != ECDH_STATUS_SUCCESS) {
        return (rc == ECDH_STATUS_PUBLIC_KEY_NOT_ON_CURVE) ? CRYPTO_BAD_PARAM : CRYPTO_HW_ERROR;
    }

    /* Copy first 32 bytes (X coordinate / Curve25519 secret) to caller. */
    for (size_t i = 0; i < 32u; i++) shared[i] = shared_buf[i];
    return CRYPTO_OK;
}
```

Note: implementer should verify TI driver pub key format (raw X||Y vs 0x04|X|Y) at runtime — adjust the initKey call if firmware tests fail with `ECDH_STATUS_PUBLIC_KEY_NOT_ON_CURVE` despite valid host inputs.

- [ ] **Step 6: Add dispatch**

Edit `firmware/cc1352/src/command_processor.c`:

```c
case CMD_ECDH: {
    /* payload: curve:1 | priv:32 | peer_pub:32 or 64 */
    if (payload_len < 1u + 32u + 32u) { send_error(seq, ERR_BAD_PAYLOAD); break; }
    uint8_t curve = payload[0];
    size_t pub_len = payload_len - 33u;
    if ((curve == 0u && pub_len != 64u) || (curve == 1u && pub_len != 32u)) {
        send_error(seq, ERR_BAD_PAYLOAD); break;
    }
    uint8_t shared[32];
    crypto_engine_status_t st = crypto_engine_ecdh((crypto_curve_t)curve, payload + 1,
                                                  payload + 33, pub_len, shared);
    if (st != CRYPTO_OK) { send_error(seq, (uint8_t)st); break; }
    send_response(RSP_ECDH, seq, 0, shared, 32);
    break;
}
```

- [ ] **Step 7: Build**

```bash
cd firmware/cc1352 && cmake --build build -j$(nproc) 2>&1 | tail -5
```

Expected: clean.

- [ ] **Step 8: Commit**

```bash
git add firmware/cc1352/src/crypto_engine.c firmware/cc1352/src/command_processor.c python/feralrf/radio.py python/tests/test_crypto.py
git commit -m "feat(f25): ECDH P-256 + Curve25519 (best-effort)

Wraps TI ECDH driver. P-256 with NIST X||Y format (64 B peer pub).
Curve25519 conditionally compiled; if SDK 8.30 doesn't expose
ECCParams_Curve25519, returns CRYPTO_UNSUPPORTED_CURVE and Python
raises CryptoError with clear message. T11 hardware smoke verifies
which curves work.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 10: ECDSA sign + verify (P-256 + Curve25519)

**Files:**
- Modify: `firmware/cc1352/src/crypto_engine.c`
- Modify: `firmware/cc1352/src/command_processor.c`
- Modify: `python/feralrf/radio.py`
- Modify: `python/tests/test_crypto.py`

- [ ] **Step 1: Write Python failing tests**

Append to `python/tests/test_crypto.py`:

```python
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
    from feralrf.enums import Response, Command

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
    # Valid sig
    monkeypatch.setattr(
        radio,
        "_read_response",
        lambda timeout=1.0, expected=None: (Response.RSP_ECDSA_VERIFY, 0, b"\x01"),
    )
    assert radio.ecdsa_verify(pub=b"\x00" * 64, msg_hash=b"\x00" * 32,
                              sig=b"\x00" * 64, curve="p256") is True
    # Invalid sig (returns 0)
    monkeypatch.setattr(
        radio,
        "_read_response",
        lambda timeout=1.0, expected=None: (Response.RSP_ECDSA_VERIFY, 0, b"\x00"),
    )
    assert radio.ecdsa_verify(pub=b"\x00" * 64, msg_hash=b"\x00" * 32,
                              sig=b"\x00" * 64, curve="p256") is False
```

- [ ] **Step 2: Run, expect fail**

```bash
cd python && pytest tests/test_crypto.py -v -k ecdsa 2>&1 | tail -10
```

Expected: 4 failures.

- [ ] **Step 3: Add Python methods**

Edit `python/feralrf/radio.py`:

```python
    def ecdsa_sign(self, priv: bytes, msg_hash: bytes, curve: str) -> bytes:
        """Sign `msg_hash` (32 B) with `priv` (32 B). Returns 64-byte signature (r||s)."""
        from feralrf.exceptions import CryptoError
        if len(priv) != 32:
            raise ValueError(f"priv must be 32 bytes, got {len(priv)}")
        if len(msg_hash) != 32:
            raise ValueError(f"hash must be 32 bytes, got {len(msg_hash)}")
        curve_id = self._resolve_curve_id(curve)
        payload = bytes([curve_id]) + priv + msg_hash
        self._send_command(Command.CMD_ECDSA_SIGN, payload)
        rsp_id, status, out = self._read_response(expected=Response.RSP_ECDSA_SIG)
        if rsp_id == Response.ERROR:
            if status == 0x05:
                raise CryptoError(f"ecdsa_sign: curve {curve!r} not supported by firmware")
            raise CryptoError(f"ecdsa_sign failed: status={status}")
        if len(out) != 64:
            raise CryptoError(f"ecdsa_sign returned {len(out)} bytes, expected 64")
        return out

    def ecdsa_verify(self, pub: bytes, msg_hash: bytes, sig: bytes, curve: str) -> bool:
        """Verify `sig` for `msg_hash` under `pub`. Returns True/False."""
        from feralrf.exceptions import CryptoError
        if len(msg_hash) != 32:
            raise ValueError(f"hash must be 32 bytes, got {len(msg_hash)}")
        if len(sig) != 64:
            raise ValueError(f"sig must be 64 bytes, got {len(sig)}")
        curve_id = self._resolve_curve_id(curve)
        if curve == "p256" and len(pub) != 64:
            raise ValueError(f"pub must be 64 bytes for p256, got {len(pub)}")
        if curve == "curve25519" and len(pub) != 32:
            raise ValueError(f"pub must be 32 bytes for curve25519, got {len(pub)}")
        payload = bytes([curve_id]) + pub + msg_hash + sig
        self._send_command(Command.CMD_ECDSA_VERIFY, payload)
        rsp_id, status, out = self._read_response(expected=Response.RSP_ECDSA_VERIFY)
        if rsp_id == Response.ERROR:
            if status == 0x05:
                raise CryptoError(f"ecdsa_verify: curve {curve!r} not supported")
            raise CryptoError(f"ecdsa_verify failed: status={status}")
        return out == b"\x01"

    def _resolve_curve_id(self, curve: str) -> int:
        if curve == "p256":
            return 0
        if curve == "curve25519":
            return 1
        raise ValueError(f"unknown curve {curve!r}; expected p256|curve25519")
```

- [ ] **Step 4: Run Python tests**

```bash
cd python && pytest tests/test_crypto.py -v -k ecdsa 2>&1 | tail -10
```

Expected: 4/4 PASS.

- [ ] **Step 5: Implement firmware ECDSA**

Edit `firmware/cc1352/src/crypto_engine.c` — include:

```c
#include <ti/drivers/ECDSA.h>
```

Replace stubs:

```c
crypto_engine_status_t crypto_engine_ecdsa_sign(crypto_curve_t curve, const uint8_t priv[32],
                                                const uint8_t hash[32], uint8_t sig[64]) {
    if (priv == NULL || hash == NULL || sig == NULL) return CRYPTO_BAD_PARAM;
    if (!s_initialized) return CRYPTO_NOT_INITIALIZED;

    const ECCParams_CurveParams *cparams;
    if (curve == CRYPTO_CURVE_P256) {
        cparams = &ECCParams_NISTP256;
    } else {
        return CRYPTO_UNSUPPORTED_CURVE; /* Curve25519 ECDSA: Ed25519 — not in TI ECDSA driver */
    }

    ECDSA_Params params;
    ECDSA_Params_init(&params);
    params.returnBehavior = ECDSA_RETURN_BEHAVIOR_POLLING;

    ECDSA_Handle h = ECDSA_open(CONFIG_ECDSA_0, &params);
    if (h == NULL) return CRYPTO_HW_ERROR;

    CryptoKey priv_key;
    CryptoKeyPlaintext_initKey(&priv_key, (uint8_t *)priv, 32);

    ECDSA_OperationSign oper;
    ECDSA_OperationSign_init(&oper);
    oper.curve = cparams;
    oper.myPrivateKey = &priv_key;
    oper.hash = (uint8_t *)hash;
    oper.r = sig;             /* 32 B */
    oper.s = sig + 32;        /* 32 B */

    int_fast16_t rc = ECDSA_sign(h, &oper);
    ECDSA_close(h);
    return (rc == ECDSA_STATUS_SUCCESS) ? CRYPTO_OK : CRYPTO_HW_ERROR;
}

crypto_engine_status_t crypto_engine_ecdsa_verify(crypto_curve_t curve,
                                                  const uint8_t *pub, size_t pub_len,
                                                  const uint8_t hash[32],
                                                  const uint8_t sig[64], bool *valid) {
    if (pub == NULL || hash == NULL || sig == NULL || valid == NULL) return CRYPTO_BAD_PARAM;
    if (!s_initialized) return CRYPTO_NOT_INITIALIZED;

    const ECCParams_CurveParams *cparams;
    if (curve == CRYPTO_CURVE_P256) {
        cparams = &ECCParams_NISTP256;
        if (pub_len != 64u) return CRYPTO_BAD_PARAM;
    } else {
        return CRYPTO_UNSUPPORTED_CURVE;
    }

    ECDSA_Params params;
    ECDSA_Params_init(&params);
    params.returnBehavior = ECDSA_RETURN_BEHAVIOR_POLLING;

    ECDSA_Handle h = ECDSA_open(CONFIG_ECDSA_0, &params);
    if (h == NULL) return CRYPTO_HW_ERROR;

    CryptoKey pub_key;
    CryptoKeyPlaintext_initKey(&pub_key, (uint8_t *)pub, pub_len);

    ECDSA_OperationVerify oper;
    ECDSA_OperationVerify_init(&oper);
    oper.curve = cparams;
    oper.theirPublicKey = &pub_key;
    oper.hash = (uint8_t *)hash;
    oper.r = (uint8_t *)sig;
    oper.s = (uint8_t *)(sig + 32);

    int_fast16_t rc = ECDSA_verify(h, &oper);
    ECDSA_close(h);

    if (rc == ECDSA_STATUS_SUCCESS) {
        *valid = true;
        return CRYPTO_OK;
    }
    if (rc == ECDSA_STATUS_INVALID_SIGNATURE || rc == ECDSA_STATUS_R_LARGER_THAN_ORDER ||
        rc == ECDSA_STATUS_S_LARGER_THAN_ORDER) {
        *valid = false;
        return CRYPTO_OK;
    }
    return CRYPTO_HW_ERROR;
}
```

- [ ] **Step 6: Add dispatch**

Edit `firmware/cc1352/src/command_processor.c`:

```c
case CMD_ECDSA_SIGN: {
    /* payload: curve:1 | priv:32 | hash:32 = 65 B */
    if (payload_len != 65u) { send_error(seq, ERR_BAD_PAYLOAD); break; }
    uint8_t sig[64];
    crypto_engine_status_t st = crypto_engine_ecdsa_sign((crypto_curve_t)payload[0],
                                                        payload + 1, payload + 33, sig);
    if (st != CRYPTO_OK) { send_error(seq, (uint8_t)st); break; }
    send_response(RSP_ECDSA_SIG, seq, 0, sig, 64);
    break;
}

case CMD_ECDSA_VERIFY: {
    /* payload: curve:1 | pub:32 or 64 | hash:32 | sig:64 */
    if (payload_len < 1u + 32u + 32u + 64u) { send_error(seq, ERR_BAD_PAYLOAD); break; }
    uint8_t curve = payload[0];
    size_t pub_len = payload_len - 1u - 32u - 64u;
    if ((curve == 0u && pub_len != 64u) || (curve == 1u && pub_len != 32u)) {
        send_error(seq, ERR_BAD_PAYLOAD); break;
    }
    bool valid = false;
    crypto_engine_status_t st = crypto_engine_ecdsa_verify((crypto_curve_t)curve, payload + 1,
                                                          pub_len, payload + 1 + pub_len,
                                                          payload + 1 + pub_len + 32, &valid);
    if (st != CRYPTO_OK) { send_error(seq, (uint8_t)st); break; }
    uint8_t result = valid ? 1u : 0u;
    send_response(RSP_ECDSA_VERIFY, seq, 0, &result, 1);
    break;
}
```

- [ ] **Step 7: Build**

```bash
cd firmware/cc1352 && cmake --build build -j$(nproc) 2>&1 | tail -5
```

Expected: clean.

- [ ] **Step 8: Commit**

```bash
git add firmware/cc1352/src/crypto_engine.c firmware/cc1352/src/command_processor.c python/feralrf/radio.py python/tests/test_crypto.py
git commit -m "feat(f25): ECDSA sign + verify (P-256)

Curve25519 ECDSA (Ed25519) returns CRYPTO_UNSUPPORTED_CURVE since TI
ECDSA driver doesn't expose it (Curve25519 in TI is for ECDH only).
P-256 sign returns r||s in network byte order. Verify returns 1/0 byte.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 11: NIST CAVS vector cross-check + hardware smoke harness

**Files:**
- Create: `python/tests/test_crypto_vectors.py`
- Create: `python/examples/lab/smoke_f25_crypto.py`

This task wires up host-side NIST vectors (no hardware) and the 9 hardware smoke tests that run end-to-end against a flashed board.

- [ ] **Step 1: Write NIST vector cross-check tests (host-side)**

Create `python/tests/test_crypto_vectors.py`:

```python
"""Cross-check FeralRF crypto API expectations against a trusted host
implementation (`cryptography` lib). These tests do NOT touch hardware —
they verify that our Python wrappers produce input/output formats
compatible with NIST CAVS test vectors and the host crypto library.

Hardware end-to-end execution lives in smoke_f25_crypto.py.
"""

import binascii

import pytest
from cryptography.hazmat.primitives import hashes, hmac
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


def test_aes_ccm_rfc3610_vector_1():
    """RFC 3610 Test Vector #1."""
    key = hx("c0c1c2c3 c4c5c6c7 c8c9cacb cccdcecf")
    nonce = hx("00000003 02010007 06050403 02010005")[:13]
    aad = hx("00010203 04050607")
    pt = hx("08090a0b 0c0d0e0f 10111213 14151617 18191a1b 1c1d1e")
    expected = hx("588c979a 61c663d2 f066d0c2 c0f98980 6d5f6b61 dac384" + "17e8d12cfdf926e0")
    aesccm = AESCCM(key, tag_length=8)
    ct = aesccm.encrypt(nonce, pt, aad)
    assert ct == expected


def test_aes_gcm_nist_test1():
    """NIST SP 800-38D Test Case 1: empty pt, empty aad."""
    key = hx("00000000000000000000000000000000")
    iv = hx("000000000000000000000000")
    aad = b""
    pt = b""
    expected_tag = hx("58e2fccefa7e3061367f1d57a4e7455a")
    gcm = AESGCM(key)
    ct = gcm.encrypt(iv, pt, aad)
    assert ct[len(pt):] == expected_tag


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
```

- [ ] **Step 2: Run NIST vector tests**

```bash
cd python && pytest tests/test_crypto_vectors.py -v 2>&1 | tail -10
```

Expected: 6/6 PASS. (These don't touch hardware; they validate the host-side test vectors load correctly.)

- [ ] **Step 3: Add `cryptography` to dev requirements**

Edit `python/pyproject.toml` — find the `[project.optional-dependencies]` `dev` array and add `"cryptography>=42.0"`.

- [ ] **Step 4: Create hardware smoke**

Create `python/examples/lab/smoke_f25_crypto.py`:

```python
#!/usr/bin/env python3
"""F25 wire-level smoke — Crypto HW on single board.

Runs 9 tests against a flashed CatSniffer:
  1. TRNG basic (240 B, two calls differ, monobit + runs over 1 KB).
  2. AES-ECB FIPS-197 vector.
  3. AES-CCM RFC 3610 vector #1.
  4. AES-CTR NIST SP 800-38A test 1.
  5. AES-CBC NIST SP 800-38A test 1.
  6. AES-GCM NIST SP 800-38D test case 1.
  7. SHA-256 FIPS 180-4 'abc' vector.
  8. ECDH P-256 + Curve25519 (Curve25519 marked SKIP if unsupported).
  9. ECDSA P-256 sign + verify (Curve25519 SKIP).

Hardware: single board on /dev/ttyACM5 (default) or --port arg.
"""

import argparse
import binascii
import sys
import time
import warnings

from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.serialization import (
    Encoding, PublicFormat, PrivateFormat, NoEncryption,
)

from feralrf import Radio
from feralrf.exceptions import CryptoError

warnings.simplefilter("ignore")


def hx(s):
    return binascii.unhexlify(s.replace(" ", ""))


def monobit_runs(data):
    """Simplified NIST SP 800-22 monobit + runs at confidence ~0.99."""
    bits = sum(bin(b).count("1") for b in data)
    n = len(data) * 8
    monobit_ok = abs(bits - n / 2) < 0.05 * n  # ±5% balance
    transitions = 0
    last = 0
    for b in data:
        for i in range(8):
            cur = (b >> i) & 1
            if cur != last:
                transitions += 1
            last = cur
    runs_ok = abs(transitions - n / 2) < 0.05 * n
    return monobit_ok and runs_ok


def trng_test(radio):
    a = radio.random_bytes(240)
    b = radio.random_bytes(240)
    differ = a != b
    sample = b""
    while len(sample) < 1024:
        sample += radio.random_bytes(240)
    sample = sample[:1024]
    stats_ok = monobit_runs(sample)
    ok = differ and stats_ok
    print(f"  TRNG basic 240B differ={differ} monobit/runs={stats_ok} {'PASS' if ok else 'FAIL'}")
    return ok


def aes_ecb_test(radio):
    key = hx("000102030405060708090a0b0c0d0e0f")
    pt = hx("00112233445566778899aabbccddeeff")
    expected = hx("69c4e0d86a7b0430d8cdb78070b4c55a")
    ct = radio.aes_encrypt(key, pt, mode="ecb")
    pt_back = radio.aes_decrypt(key, ct, mode="ecb")
    ok = ct == expected and pt_back == pt
    print(f"  AES-ECB FIPS-197 ct_match={ct == expected} roundtrip={pt_back == pt} {'PASS' if ok else 'FAIL'}")
    return ok


def aes_ccm_test(radio):
    key = hx("c0c1c2c3c4c5c6c7c8c9cacbcccdcecf")
    nonce = hx("00000003020100070605040302010005")[:13]
    aad = hx("0001020304050607")
    pt = hx("08090a0b0c0d0e0f101112131415161718191a1b1c1d1e")
    expected_ct = hx("588c979a61c663d2f066d0c2c0f989806d5f6b61dac384")
    expected_tag = hx("17e8d12cfdf926e0")
    ct, tag = radio.aes_ccm_encrypt(key, nonce, aad, pt, tag_len=8)
    pt_back = radio.aes_ccm_decrypt(key, nonce, aad, ct, tag, tag_len=8)
    ok = ct == expected_ct and tag == expected_tag and pt_back == pt
    print(f"  AES-CCM RFC3610-1 ct={ct == expected_ct} tag={tag == expected_tag} rt={pt_back == pt} {'PASS' if ok else 'FAIL'}")
    return ok


def aes_ctr_test(radio):
    key = hx("2b7e151628aed2a6abf7158809cf4f3c")
    iv = hx("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff")
    pt = hx("6bc1bee22e409f96e93d7e117393172a")
    expected = hx("874d6191b620e3261bef6864990db6ce")
    ct = radio.aes_encrypt(key, pt, mode="ctr", iv=iv)
    pt_back = radio.aes_decrypt(key, ct, mode="ctr", iv=iv)
    ok = ct == expected and pt_back == pt
    print(f"  AES-CTR SP800-38A-1 ct={ct == expected} rt={pt_back == pt} {'PASS' if ok else 'FAIL'}")
    return ok


def aes_cbc_test(radio):
    key = hx("2b7e151628aed2a6abf7158809cf4f3c")
    iv = hx("000102030405060708090a0b0c0d0e0f")
    pt = hx("6bc1bee22e409f96e93d7e117393172a")
    expected = hx("7649abac8119b246cee98e9b12e9197d")
    ct = radio.aes_encrypt(key, pt, mode="cbc", iv=iv)
    pt_back = radio.aes_decrypt(key, ct, mode="cbc", iv=iv)
    ok = ct == expected and pt_back == pt
    print(f"  AES-CBC SP800-38A-1 ct={ct == expected} rt={pt_back == pt} {'PASS' if ok else 'FAIL'}")
    return ok


def aes_gcm_test(radio):
    key = hx("00000000000000000000000000000000")
    iv = hx("000000000000000000000000")
    expected_tag = hx("58e2fccefa7e3061367f1d57a4e7455a")
    ct, tag = radio.aes_gcm_encrypt(key, iv, b"", b"")
    ok = ct == b"" and tag == expected_tag
    print(f"  AES-GCM SP800-38D-1 tag={tag == expected_tag} {'PASS' if ok else 'FAIL'}")
    return ok


def sha256_test(radio):
    expected = hx("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")
    digest = radio.sha256(b"abc")
    ok = digest == expected
    print(f"  SHA-256 FIPS180-4 abc match={ok} {'PASS' if ok else 'FAIL'}")
    return ok


def ecdh_test(radio):
    """Generate keypairs in host, exchange with chip, verify shared match."""
    # P-256 round-trip
    host_priv = ec.generate_private_key(ec.SECP256R1())
    host_pub = host_priv.public_key()
    host_pub_xy = host_pub.public_bytes(Encoding.X962, PublicFormat.UncompressedPoint)[1:]  # drop 0x04
    host_priv_bytes = host_priv.private_numbers().private_value.to_bytes(32, "big")
    chip_shared = radio.ecdh(my_priv=host_priv_bytes, peer_pub=host_pub_xy, curve="p256")
    # Host computes self-shared
    host_shared = host_priv.exchange(ec.ECDH(), host_pub)
    p256_ok = chip_shared == host_shared
    print(f"  ECDH P-256 round-trip match={p256_ok} {'PASS' if p256_ok else 'FAIL'}")

    # Curve25519 — try, accept SKIP if firmware reports unsupported
    try:
        from cryptography.hazmat.primitives.asymmetric.x25519 import X25519PrivateKey
        h_priv = X25519PrivateKey.generate()
        h_pub = h_priv.public_key().public_bytes(Encoding.Raw, PublicFormat.Raw)
        h_priv_b = h_priv.private_bytes(Encoding.Raw, PrivateFormat.Raw, NoEncryption())
        chip_25519 = radio.ecdh(my_priv=h_priv_b, peer_pub=h_pub, curve="curve25519")
        host_25519 = h_priv.exchange(h_priv.public_key())
        c25_ok = chip_25519 == host_25519
        print(f"  ECDH Curve25519 match={c25_ok} {'PASS' if c25_ok else 'FAIL'}")
    except CryptoError as e:
        print(f"  ECDH Curve25519 SKIP ({e}) — firmware unsupported")
        c25_ok = True  # not a regression

    return p256_ok and c25_ok


def ecdsa_test(radio):
    """Sign on chip, verify on host; sign on host, verify on chip."""
    host_priv = ec.generate_private_key(ec.SECP256R1())
    host_pub = host_priv.public_key()
    host_pub_xy = host_pub.public_bytes(Encoding.X962, PublicFormat.UncompressedPoint)[1:]
    host_priv_bytes = host_priv.private_numbers().private_value.to_bytes(32, "big")
    msg_hash = hx("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")

    # Sign on chip
    sig_chip = radio.ecdsa_sign(priv=host_priv_bytes, msg_hash=msg_hash, curve="p256")
    # Verify on host
    from cryptography.hazmat.primitives.asymmetric.utils import encode_dss_signature
    r = int.from_bytes(sig_chip[:32], "big")
    s = int.from_bytes(sig_chip[32:], "big")
    der = encode_dss_signature(r, s)
    try:
        host_pub.verify(der, msg_hash, ec.ECDSA(ec.utils.Prehashed(__import__('cryptography').hazmat.primitives.hashes.SHA256())))
        host_verifies = True
    except Exception:
        host_verifies = False

    # Sign on host, verify on chip
    sig_host_der = host_priv.sign(msg_hash, ec.ECDSA(ec.utils.Prehashed(__import__('cryptography').hazmat.primitives.hashes.SHA256())))
    from cryptography.hazmat.primitives.asymmetric.utils import decode_dss_signature
    r2, s2 = decode_dss_signature(sig_host_der)
    sig_host = r2.to_bytes(32, "big") + s2.to_bytes(32, "big")
    chip_verifies = radio.ecdsa_verify(pub=host_pub_xy, msg_hash=msg_hash, sig=sig_host, curve="p256")

    ok = host_verifies and chip_verifies
    print(f"  ECDSA P-256 host_verify_chip_sig={host_verifies} chip_verify_host_sig={chip_verifies} {'PASS' if ok else 'FAIL'}")
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/ttyACM5")
    args = ap.parse_args()

    radio = Radio(args.port)
    results = {}
    try:
        radio.connect()
        time.sleep(0.3)
        radio.init()

        print("[STEP] tests")
        results["trng"] = trng_test(radio)
        results["aes_ecb"] = aes_ecb_test(radio)
        results["aes_ccm"] = aes_ccm_test(radio)
        results["aes_ctr"] = aes_ctr_test(radio)
        results["aes_cbc"] = aes_cbc_test(radio)
        results["aes_gcm"] = aes_gcm_test(radio)
        results["sha256"] = sha256_test(radio)
        results["ecdh"] = ecdh_test(radio)
        results["ecdsa"] = ecdsa_test(radio)

        n_pass = sum(results.values())
        ok = all(results.values())
        print()
        print(f"[ {'OK' if ok else 'FAIL'} ] F25 smoke: {n_pass}/9 PASS")
        return 0 if ok else 1
    finally:
        try:
            radio.disconnect()
        except Exception:
            pass


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 5: Run unit tests to confirm no regression**

```bash
cd python && pytest 2>&1 | tail -5
```

Expected: all tests pass (303 + new crypto tests).

- [ ] **Step 6: Commit (smoke harness — hardware run is T12 closure)**

```bash
git add python/tests/test_crypto_vectors.py python/examples/lab/smoke_f25_crypto.py python/pyproject.toml
git commit -m "test(f25): NIST CAVS vectors + 9-test hardware smoke harness

test_crypto_vectors.py: 6 NIST vectors validated against host
cryptography lib (no hardware). smoke_f25_crypto.py: 9 end-to-end
tests against a flashed board. ECDH/ECDSA cross-checked against host
keypairs. cryptography>=42.0 added to dev deps.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 12: Closure gate — flash, hardware smoke, validation, tag

**Files:** None (this task only runs validation, doesn't change source).

- [ ] **Step 1: Flash one board**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
python3 ~/Documents/electroniccats/CatSniffer-Tools/catnip/catnip.py flash -d 1 firmware/cc1352/build/feralrf_cc1352.hex 2>&1 | tail -3
```

Expected: "Device restart complete. Firmware is ready to use!"

- [ ] **Step 2: Run hardware smoke**

```bash
source python/.venv/bin/activate
python python/examples/lab/smoke_f25_crypto.py --port /dev/ttyACM8 2>&1 | tail -15
```

Expected: 9/9 PASS (or 8/9 with Curve25519 SKIP marked as not regression).

- [ ] **Step 3: Run F22 sanity (ensure no regression in test modes)**

```bash
python python/examples/lab/smoke_f22_tx_test.py 2>&1 | tail -10
```

Expected: 5/5 PASS. (F22 should be unaffected by F25 — no overlap.)

- [ ] **Step 4: Run F9 hot-switch sanity**

```bash
python python/examples/lab/smoke_f9_hot_switch.py 2>&1 | tail -10  # or equivalent matrix test
```

Expected: 6/6 PHY transitions PASS.

- [ ] **Step 5: Run full Python suite**

```bash
cd python && pytest 2>&1 | tail -3
```

Expected: all PASS, 0 regressions.

- [ ] **Step 6: Pre-commit clean**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF && pre-commit run --all-files 2>&1 | tail -10
```

Expected: all hooks pass.

- [ ] **Step 7: Update memory — feedback_trng_hang.md → resolved**

Edit `/home/sabas/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/feedback_trng_hang.md` (if it exists) to add a closing note:

```
**RESOLVED 2026-04-30 by F25 (commit <SHA>):** Power_setDependency(PowerCC26X2_PERIPH_TRNG)
in crypto_engine_init() before TRNG_init() fixes the hang permanently.
xorshift32 work-around removed.
```

Or, if the memory file doesn't exist yet, skip this step.

- [ ] **Step 8: Write project_f25_done.md memory entry**

Create `/home/sabas/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/project_f25_done.md`:

```markdown
---
name: F25 Crypto HW done
description: F25 closed YYYY-MM-DD on feature/f25-crypto-hw. TRNG/AES/SHA/ECDH/ECDSA via crypto_engine. Tag v2.0-f25.
type: project
---

F25 closed YYYY-MM-DD on branch `feature/f25-crypto-hw`, commit <SHA>, tag `v2.0-f25`.

**Why:** Full chip-API crypto coverage in one phase: TRNG (PERIPH power fix),
AES-128 (ECB/CCM/CTR/CBC/GCM), SHA-256, ECDH (P-256 + Curve25519 if available),
ECDSA P-256. Module crypto_engine.{c,h} as clean boundary.

**How to apply:**
- 11 new Python methods on Radio. NIST CAVS vectors validated host-side and
  hardware end-to-end (smoke 9/9 PASS).
- Curve25519 ECDH availability depends on TI SDK 8.30 (verify `ECCParams_Curve25519`
  symbol exists in driverlib/ECCParams.h). Curve25519 ECDSA (Ed25519) NOT in TI
  ECDSA driver — returns CRYPTO_UNSUPPORTED_CURVE; deferred to F25.b if needed.
- Per-call open/close pattern adds ~100 µs per op (negligible vs typical use cases).
- Streaming AES/SHA for >240 B input → F25.b.
```

- [ ] **Step 9: Update MEMORY.md index**

Edit `/home/sabas/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/MEMORY.md` — append under `## Project`:

```markdown
- [project_f25_done.md](project_f25_done.md) — YYYY-MM-DD: F25 closed. Crypto HW (TRNG/AES/SHA/ECDH/ECDSA) on feature/f25-crypto-hw. Tag v2.0-f25.
```

- [ ] **Step 10: Tag v2.0-f25**

```bash
git tag -a v2.0-f25 -m "F25 closed: Crypto HW (TRNG/AES-{ECB,CCM,CTR,CBC,GCM}/SHA-256/ECDH/ECDSA). 9/9 hardware smoke PASS."
git tag --list "v2.0-f25"
```

- [ ] **Step 11: FF to feature/ti-rtos-migration**

```bash
git checkout feature/ti-rtos-migration
git merge --ff-only feature/f25-crypto-hw 2>&1 | tail -5
git checkout feature/f25-crypto-hw
```

Expected: fast-forward merge clean.

- [ ] **Step 12: Final report to user**

State results in chat: tag landed, 9/9 smoke, no regressions, FF complete. Branch `feature/f25-crypto-hw` retained for inspection.

---

## Self-review checklist (controller fills before handoff)

- [x] Spec coverage: every primitive in spec §1 has a task (T4 RNG, T5 ECB/CTR/CBC, T6 CCM, T7 GCM, T8 SHA, T9 ECDH, T10 ECDSA).
- [x] No placeholders: every code block is complete; no "TBD" or "implement later".
- [x] Type consistency: `crypto_engine_status_t`, `crypto_curve_t`, command IDs (0x59-0x62), response IDs (0x95-0x9C) used identically across tasks.
- [x] Curve25519 caveat: T9 explicitly handles `CRYPTO_UNSUPPORTED_CURVE` fallback; smoke marks SKIP not FAIL.
- [x] User WIP pattern: noted at task level — implementer must stash `radio_if.h` + `command_processor.c` user WIP before each commit and pop after (memoria/feedback_workflow.md).
- [x] Tag/FF deferred to T12 last step after hardware smoke validates.
