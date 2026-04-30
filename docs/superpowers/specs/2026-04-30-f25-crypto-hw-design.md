# F25 — Crypto HW expuesto — Design Spec

> **Date:** 2026-04-30
> **Branch:** `feature/f25-crypto-hw`
> **Phase:** F25 (Bloque D — chip API completeness) of plan-v2
> **Author:** Sabas + Claude (brainstorming session)
> **Tag al cerrar:** `v2.0-f25`

---

## 1. Goal & scope

Exponer los motores de criptografía hardware del CC1352P7 como API del cliente Python. Cubre **TRNG, AES-128 (ECB/CCM/CTR/CBC/GCM), SHA-256, ECDH (P-256 + Curve25519), ECDSA (P-256 + Curve25519)** en una sola pasada de implementación.

Alineado con el goal de Sabas: API completa del CC1352, no security toolkit. Cada primitiva es exposición directa de capability del silicio que hoy es inaccesible desde el host.

### In scope (v2.0-f25)

- **TRNG**: fix del bug PERIPH power domain (resuelve R6 / `feedback_trng_hang.md` permanentemente). Reemplaza el work-around xorshift32. `random_bytes(n)` para 1 ≤ n ≤ 240.
- **AES-128**: ECB, CCM, CTR, CBC, GCM — modo one-shot, ≤240 B input por call.
- **SHA-256** — one-shot, ≤240 B input por call.
- **ECDH** — P-256 + Curve25519 (sujeto a confirmación T6/T7 que TI driver expone Curve25519; si no, se difiere a F25.b sin bloquear closure).
- **ECDSA** — P-256 + Curve25519 (mismo caveat). Sign + verify.

### Out of scope (deferred)

- **Streaming AES/SHA** (>240 B inputs) → F25.b. Patrón init/update/final con sesiones por canal. Hoy no hay caso de uso real ≥240 B en el codebase.
- **Curvas P-224 / P-384 / P-521** → F25.b si surge necesidad. P-224 está deprecated, P-384/521 son nicho embedded.
- **Key slots / persistent key storage** → F25.b. Hoy keys son per-call.
- **AES-256** — el chip soporta solo AES-128 en HW.
- **ChaCha20 / Poly1305** — no en HW; software impl no agrega API surface del chip.
- **FIPS compliance / side-channel hardening** — el CC1352P7 no es Secure Element certificado; F25 documenta esto como expected limit.

## 2. Brainstorm decisions

| # | Decisión | Rationale |
|---|---|---|
| 1 | Scope: full chip-API coverage (no strict subset) | Cubre toda la cripto HW en una pasada en lugar de fragmentar por release. Delta de scope (~400 LOC adicionales sobre strict) es bajo vs el costo de duplicar plumbing COBS + tests en futuras fases. |
| 2 | One-shot only, ≤240 B per call (no streaming) | El protocolo COBS tiene `PROTOCOL_MAX_PAYLOAD=255` B. Para los usos reales en CC1352 (BLE LL ≤27 B, Zigbee NWK ≤120 B, custom protocols ≤200 B) one-shot basta. Streaming agrega estado, edge-cases de sesiones huérfanas, fairness — diferido a F25.b. |
| 3 | Curvas: P-256 + Curve25519 | P-256 cubre BLE-SC, Apple/Google attestation, Matter. Curve25519 cubre Signal/Wireguard/OpenSSH/firmware OTA modernos. P-224 deprecated, P-384/521 nicho. |
| 4 | Arquitectura: módulo `crypto_engine` con TI drivers thin (open/close per-call) | `command_processor.c` ya está cerca del límite de tamaño legible (~450 LOC). Módulo aparte mantiene boundary limpio, testeable en isolation, extensible a streaming/key-slots en F25.b sin tocar el dispatcher. Per-call open/close añade ~100 µs por op (irrelevante vs típico BLE LL packet ~1 ms). |
| 5 | Sync polling (no async / no callbacks) | Power policy ya está disabled en este firmware (memoria `feedback_tirtos_rf_rules.md`). Polling es el patrón más simple y consistente con el resto del codebase. |
| 6 | Per-call key passing (no slot/handle) | Stateless. Cada call carga key + data + opera + retorna. Si llegara a haber alto throughput con misma key, F25.b agrega slots. |
| 7 | Errores → `CryptoError(RadioError)` | Patrón existente en `python/feralrf/exceptions.py`. Mapping desde firmware error codes vía COBS RSP_ERROR. |

## 3. API

### 3.1 Python — `feralrf.Radio` métodos nuevos

```python
# RNG
data = radio.random_bytes(n)                       # 1 ≤ n ≤ 240; returns bytes

# AES-128 — key: 16 B; one-shot ≤240 B (CBC requires multiple of 16)
ct = radio.aes_encrypt(key, plaintext, mode='ecb'|'ctr'|'cbc',
                       iv=None)                    # iv: 16 B for ctr/cbc
pt = radio.aes_decrypt(key, ciphertext, mode=..., iv=...)

# AES-CCM
ct, tag = radio.aes_ccm_encrypt(key, nonce, aad, plaintext,
                                tag_len=8|16)
pt = radio.aes_ccm_decrypt(key, nonce, aad, ciphertext, tag,
                           tag_len=8|16)           # raises CryptoError on tag fail

# AES-GCM
ct, tag = radio.aes_gcm_encrypt(key, iv, aad, plaintext)
pt = radio.aes_gcm_decrypt(key, iv, aad, ciphertext, tag)

# SHA-256
digest = radio.sha256(data)                        # ≤240 B → 32 B

# ECDH
shared = radio.ecdh(my_priv, peer_pub,
                    curve='p256'|'curve25519')     # → 32 B shared secret

# ECDSA
sig = radio.ecdsa_sign(priv, msg_hash, curve=...)  # → 64 B sig
ok = radio.ecdsa_verify(pub, msg_hash, sig, curve=...)  # → bool
```

Errores:
- `ValueError` para parámetros locales mal formados (longitud key, tag_len inválido, curve unknown).
- `CryptoError(RadioError)` para fallos retornados por firmware (tag mismatch, hardware error, driver init failure).

### 3.2 Firmware — Command IDs

| Cmd | ID | Payload (post header) | Response | Resp data |
|---|---|---|---|---|
| `CMD_RANDOM` | 0x59 | `n:u8` | `RSP_RANDOM` (0x95) | n bytes |
| `CMD_AES_ECB` | 0x5A | `op:u8 \| key[16] \| data[16]` | `RSP_AES` (0x96) | 16 B |
| `CMD_AES_CCM` | 0x5B | `op:u8 \| key[16] \| nonce_len:u8 \| nonce \| aad_len:u16_le \| pt_len:u16_le \| tag_len:u8 \| aad \| data \| tag (op=1)` | `RSP_AES_CCM` (0x97) | ct + tag (encrypt) / pt (decrypt) |
| `CMD_AES_CTR` | 0x5C | `op:u8 \| key[16] \| iv[16] \| data` | `RSP_AES` (0x96) | data length unchanged |
| `CMD_AES_CBC` | 0x5D | `op:u8 \| key[16] \| iv[16] \| data` | `RSP_AES` (0x96) | data length (must be multiple of 16) |
| `CMD_AES_GCM` | 0x5E | `op:u8 \| key[16] \| iv[12] \| aad_len:u16_le \| pt_len:u16_le \| aad \| data \| tag (op=1)` | `RSP_AES_GCM` (0x98) | ct + tag(16) (encrypt) / pt (decrypt) |
| `CMD_SHA256` | 0x5F | `data` | `RSP_SHA256` (0x99) | 32 B |
| `CMD_ECDH` | 0x60 | `curve:u8 \| my_priv[32] \| peer_pub[64 for p256, 32 for curve25519]` | `RSP_ECDH` (0x9A) | 32 B |
| `CMD_ECDSA_SIGN` | 0x61 | `curve:u8 \| priv[32] \| hash[32]` | `RSP_ECDSA_SIG` (0x9B) | 64 B (r:32 \| s:32) |
| `CMD_ECDSA_VERIFY` | 0x62 | `curve:u8 \| pub[64 or 32] \| hash[32] \| sig[64]` | `RSP_ECDSA_VERIFY` (0x9C) | 1 B (0=invalid, 1=valid) |

`op:u8`: 0=encrypt, 1=decrypt (AES family).
`curve:u8`: 0=P-256, 1=Curve25519.
Endianness: Little-endian for `aad_len`, `pt_len`. Big-endian for crypto byte arrays (network order, matches NIST/RFC convention).

Nota: `CMD_PKA_ECDH` original de plan-v2 (0x5C) se reubica a 0x60 para mantener AES family contigua (0x5A-0x5E). Documentado en plan-v2 update.

## 4. Firmware architecture

```
┌─────────────────────────────────────────────────────┐
│  python/feralrf/radio.py                            │
│    Radio.random_bytes / aes_*_encrypt / sha256 /    │
│    ecdh / ecdsa_sign / ecdsa_verify                 │
│  python/feralrf/exceptions.py                       │
│    CryptoError(RadioError)                          │
└──────────────────────┬──────────────────────────────┘
                       │ COBS frames
                       ▼
┌─────────────────────────────────────────────────────┐
│  firmware/cc1352/src/command_processor.c            │
│    handle_command() switch — 10 nuevos dispatch     │
│    cases mapeando CMD_RANDOM..CMD_ECDSA_VERIFY a    │
│    crypto_engine API.                               │
└──────────────────────┬──────────────────────────────┘
                       │ in-process call
                       ▼
┌─────────────────────────────────────────────────────┐
│  firmware/cc1352/src/crypto_engine.c                │
│    crypto_engine_init() — power up PERIPH domain,   │
│      driver init. Llamado desde RadioIF_init.       │
│    crypto_engine_random(n, out)                     │
│    crypto_engine_aes_ecb(op, key, in, out)          │
│    crypto_engine_aes_ccm(op, key, nonce, ..., out)  │
│    crypto_engine_aes_ctr(op, key, iv, in, out)      │
│    crypto_engine_aes_cbc(op, key, iv, in, out)      │
│    crypto_engine_aes_gcm(op, key, iv, ..., out)     │
│    crypto_engine_sha256(in, len, digest)            │
│    crypto_engine_ecdh(curve, priv, pub, shared)     │
│    crypto_engine_ecdsa_sign(curve, priv, hash, sig) │
│    crypto_engine_ecdsa_verify(...)                  │
└──────────────────────┬──────────────────────────────┘
                       │ TI driver calls (RETURN_BEHAVIOR_POLLING)
                       ▼
┌─────────────────────────────────────────────────────┐
│  TI SDK 8.30 drivers                                │
│    TRNG, AESECB, AESCCM, AESCTR, AESCBC, AESGCM,    │
│    SHA2, ECDH, ECDSA                                │
└─────────────────────────────────────────────────────┘
```

### 4.1 `crypto_engine` boundaries

- **Pure**: ningún acceso a UART, packet_queue, RF state, COBS. Solo TI driverlib calls + return codes.
- **Sync polling**: cada operación abre el driver TI con `RETURN_BEHAVIOR_POLLING`, ejecuta, cierra. Sin callbacks, sin tasks asincrónicos.
- **Stateless**: no hay handles persistentes; cada call es atómico.
- **Error mapping**: TI driver returns → `crypto_engine_status_t` enum:
  - `CRYPTO_OK` (0)
  - `CRYPTO_BAD_PARAM` (1) — invalid length, curve, mode
  - `CRYPTO_TAG_MISMATCH` (2) — AES-CCM/GCM tag verify failed
  - `CRYPTO_HW_ERROR` (3) — driver-level fail (e.g. TRNG entropy insufficient)
  - `CRYPTO_NOT_INITIALIZED` (4) — `crypto_engine_init` failed at boot
  - `CRYPTO_UNSUPPORTED_CURVE` (5) — Curve25519 if TI driver doesn't expose it

### 4.2 TRNG fix (R6 resolution)

Actualmente `start_trng()` cuelga porque PERIPH power domain no está habilitado. `crypto_engine_init()` (llamado al boot, p. ej. desde `RadioIF_init` o nuevo init point en `main_rtos.c`) hace:

```c
Power_setDependency(PowerCC26XX_PERIPH_TRNG);
TRNG_init();
```

Esto se ejecuta una sola vez al boot. Borra/reemplaza el work-around xorshift32 cuando exista en `radio_if.c` u otro punto (memoria `feedback_trng_hang.md` se actualiza tras closure).

### 4.3 SysConfig changes

Habilitar drivers TI necesarios en `firmware/cc1352/syscfg/feralrf.syscfg`:

```js
scripting.addModule("/ti/drivers/TRNG").addInstance().$name = "CONFIG_TRNG_0";
scripting.addModule("/ti/drivers/AESECB").addInstance().$name = "CONFIG_AESECB_0";
scripting.addModule("/ti/drivers/AESCCM").addInstance().$name = "CONFIG_AESCCM_0";
scripting.addModule("/ti/drivers/AESCTR").addInstance().$name = "CONFIG_AESCTR_0";
scripting.addModule("/ti/drivers/AESCBC").addInstance().$name = "CONFIG_AESCBC_0";
scripting.addModule("/ti/drivers/AESGCM").addInstance().$name = "CONFIG_AESGCM_0";
scripting.addModule("/ti/drivers/SHA2").addInstance().$name = "CONFIG_SHA2_0";
scripting.addModule("/ti/drivers/ECDH").addInstance().$name = "CONFIG_ECDH_0";
scripting.addModule("/ti/drivers/ECDSA").addInstance().$name = "CONFIG_ECDSA_0";
```

CMakeLists.txt linkear los drivers correspondientes (TI provee libs precompiladas en SDK 8.30).

## 5. Validation strategy

### 5.1 Unit tests Python (no hardware)

- `python/tests/test_crypto.py` — método signatures, error paths (lengths inválidas, curves desconocidos), mocked transport.
- `python/tests/test_crypto_vectors.py` — NIST CAVS test vectors hardcoded, validados contra Python `cryptography` lib (cross-reference, no hardware).

### 5.2 Hardware smoke — `python/examples/lab/smoke_f25_crypto.py`

9 tests, single board (no 2-board OTA needed):

1. **TRNG basic** — `random_bytes(240)` returns 240 B, no all-zero, second call ≠ first call. Sample 1 KB total, run monobit + runs tests (NIST SP 800-22 simplified).
2. **AES-ECB** — NIST FIPS-197 Appendix C vector (key=2b7e..., pt=6bc1..., ct=3ad7...).
3. **AES-CCM** — RFC 3610 Test Vector #1 (key, nonce, aad, pt → ct + tag).
4. **AES-CTR** — NIST SP 800-38A Test 1.
5. **AES-CBC** — NIST SP 800-38A Test 1.
6. **AES-GCM** — NIST SP 800-38D Test Case 1.
7. **SHA-256** — NIST FIPS 180-4 "abc" test vector (digest = ba7816bf...).
8. **ECDH P-256** — generate keypair en host (`cryptography`), envío `peer_pub` al chip + `my_priv` al chip; verify shared secret matches host computation. Mismo round-trip Curve25519.
9. **ECDSA P-256 + Curve25519** — sign en chip, verify en host (`cryptography`); también sign en host, verify en chip. Round-trip ambas curvas.

Closure: 9/9 PASS.

### 5.3 Manual checkpoint
Ninguno — todo desde host. F25 cierra wire-level + suite + smoke.

## 6. File layout

| Path | Acción | LOC est. |
|---|---|---|
| `firmware/cc1352/include/crypto_engine.h` | crear | ~70 |
| `firmware/cc1352/src/crypto_engine.c` | crear | ~450 |
| `firmware/cc1352/src/command_processor.c` | modificar (10 dispatch cases) | +120 |
| `firmware/cc1352/include/protocol.h` | añadir CMD/RSP IDs | +20 |
| `firmware/cc1352/CMakeLists.txt` | añadir `crypto_engine.c` + linkear drivers cripto | +5 |
| `firmware/cc1352/syscfg/feralrf.syscfg` | habilitar 9 TI driver instances | +15 |
| `python/feralrf/enums.py` | + Command IDs 0x59-0x62, + Response IDs 0x95-0x9C | +15 |
| `python/feralrf/exceptions.py` | + `CryptoError(RadioError)` | +5 |
| `python/feralrf/radio.py` | + 11 métodos (random_bytes, aes_encrypt/decrypt, aes_ccm_encrypt/decrypt, aes_gcm_encrypt/decrypt, sha256, ecdh, ecdsa_sign, ecdsa_verify) | +250 |
| `python/tests/test_crypto.py` | unit tests (mocked) | +200 |
| `python/tests/test_crypto_vectors.py` | NIST CAVS vectors | +250 |
| `python/examples/lab/smoke_f25_crypto.py` | hardware smoke 9 tests | +180 |

**Total: ~1580 LOC.** Comparable a F8 (GATT client).

## 7. Risks

| # | Risk | Mitigation |
|---|---|---|
| f25-r1 | TI driver init (TRNG/AES/PKA) puede fallar si Power policy choca con configuración actual del firmware (memoria: PowerCC26X2 standby disabled) | `crypto_engine_init()` testea cada driver al boot; si falla, log + responder ERROR `CRYPTO_NOT_INITIALIZED` a comandos cripto en lugar de cuelgue. |
| f25-r2 | NIST CAVS vectors pueden no transferir 1-a-1 (endianness, padding) entre TI driverlib y Python `cryptography` | Cross-check con vectores conocidos al T2 (primer AES test); si discrepancia, documentar el byte order del firmware y normalizar en Python wrapper. |
| f25-r3 | ECDH/ECDSA Curve25519 — TI ECDH driver soporta P-curves; Curve25519 puede requerir wrapper adicional o no estar disponible en SDK 8.30 | Verificar al inicio (T6/T7). Si TI no lo expone directo, marcar Curve25519 como deferred a F25.b sin bloquear closure (P-256 sí cubre el grueso). Spec §1 ya documenta el caveat. |
| f25-r4 | ECDSA en CC1352 puede tener issues conocidos de side-channel (TI errata) | Documentar en spec §1; F25 declara explícitamente "no FIPS / no side-channel hardened" como out-of-scope. |
| f25-r5 | `crypto_engine` open/close per-call agrega ~100 µs latencia. Si combinado con BLE LL crypto (encryption per-packet) genera bottleneck | Aceptable para v2.0; si surge issue empírico, F25.b agrega persistent driver pool (enfoque β del brainstorm). |
| f25-r6 | Workspace WIP del usuario en `command_processor.c` y `radio_if.h` (typo `driverlib / rf_mailbox.h`) puede bloquear pre-commit | Stash WIP antes de cada commit, pop después (pattern memoria `feedback_workflow.md`). |
| f25-r7 | CMD_PKA_ECDH originalmente reservado en plan-v2 a 0x5C; F25 lo mueve a 0x60 para mantener AES family contigua | Bajo impacto: plan-v2 commands no se han implementado todavía. Se actualiza plan-v2 spec con la reasignación cuando F25 cierre. |

## 8. Closure criteria

- [ ] Unit tests Python (mocked + NIST vectors cross-checked) — `test_crypto.py` + `test_crypto_vectors.py` PASS.
- [ ] Hardware smoke `smoke_f25_crypto.py` 9/9 PASS sobre una board.
- [ ] TRNG: monobit + runs tests sobre 1 KB output PASS.
- [ ] Suite Python full sin regresión.
- [ ] No regresión en F22 smoke (5/5).
- [ ] No regresión en F9 hot-switch (smoke sanity 1 cycle).
- [ ] Pre-commit clean.
- [ ] Plan en `docs/superpowers/plans/2026-04-30-f25-crypto-hw-plan.md` covered checkbox by checkbox.
- [ ] Memory entry `project_f25_done.md` escrita.
- [ ] Memoria `feedback_trng_hang.md` actualizada (work-around xorshift removed, TRNG fixed).
- [ ] Commit en `feature/f25-crypto-hw`. Tag `v2.0-f25` después de smoke PASS. FF a `feature/ti-rtos-migration` per project pattern.

## 9. Open questions

Ninguna. Curve25519 support real se confirma en T6/T7 (driver-level test); si TI no lo expone, se marca como deferred en spec §1 sin re-litigar.

## 10. Plan-v2 update

Cuando F25 cierre, actualizar plan-v2 §F25 entry para reflejar:
- Scope ampliado a full chip-API (vs strict TRNG+ECB+CCM+ECDH original).
- Command ID assignments finales (0x59-0x62 con ECDH movido de 0x5C a 0x60).
- 9 primitivas en lugar de 4.

---

**Next step:** writing-plans skill → step-by-step implementation plan en TDD (test-first per task).
