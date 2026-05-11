# SMP Phase A — Design Specification

**Date:** 2026-05-11
**Author:** Sabas (with brainstorming via superpowers)
**Status:** Awaiting user review before plan-writing
**Branch target:** new branch `feature/smp-phase-a` off main HEAD

## Purpose

Enable FeralRF to act as a BLE pairing **Initiator** (central role) against commercial BLE locks, supporting **Legacy Just Works** and **Legacy Passkey Entry** pairing methods. Additionally, provide a **Crackle-style offline decryptor** that derives Long-Term Keys from passive pcap captures of legacy pairing exchanges.

This is **Phase A** of the SMP scope. Phase B (LE Secure Connections, ECDH-based) is deferred to a separate spec.

## Why now

Active engagement to audit commercial BLE locks. F20.a.1 (peripheral GATT spoof) is paused at `v2.0-f20.a.1.e-partial` after 5 vueltas due to architectural incompatibility (CMD_BLE_ADV consumes CONNECT_IND internally). SMP + link encryption is the next critical capability — without it, FeralRF can connect as central but cannot read/write authenticated characteristics on paired locks. That blocks ~80% of useful audit work against modern locks.

## Scope

**In scope:**
- SMP Initiator role (FeralRF as central pairing TO peripheral lock)
- Legacy Just Works (TK = 0)
- Legacy Passkey Entry (TK = 6-digit PIN, supplied via Python API)
- Link Layer encryption setup (LL_ENC_REQ/RSP, LL_START_ENC_REQ/RSP, AES-CCM on TX/RX)
- Crackle-style offline decryption of captured legacy pairing pcaps
- In-memory bond storage (peer addr + LTK + EDIV + Rand)
- Failure path reporting (timeout, peer rejection, confirm mismatch, disconnect)

**Out of scope (deferred):**
- SMP Responder role (requires F20.a.1.g architectural pivot — paused)
- LE Secure Connections (ECDH P-256, f1/f2/f3/f4/f5/f6, AES-CMAC) — Phase B
- Numeric Comparison, OOB pairing methods
- Persistent bond storage (file-backed bond database) — Phase C
- Pairing as part of an automated reactive jamming attack chain

## Architecture

Three-tier hybrid: thin firmware (L2CAP framer + LL encryption setup), all SMP state machine in Python, separate Python module for offline Crackle decryption.

```
HOST (Python)                          │  CC1352 (firmware)
                                       │
┌─────────────────────────────────┐    │   ┌─────────────────────────────┐
│ feralrf.smp.SmpInitiator        │◀──▶│   │ l2cap_smp.c                 │
│  state machine + crypto helpers │    │   │  CID 0x0006 framer          │
└─────────────────────────────────┘    │   └─────────────────────────────┘
┌─────────────────────────────────┐    │   ┌─────────────────────────────┐
│ feralrf.crackle.CrackleDecoder  │    │   │ ll_enc.c                    │
│  offline pcap decryption        │    │   │  CMD_ENABLE_LL_ENC          │
└─────────────────────────────────┘    │   │  + LL opcode handlers       │
                                       │   └─────────────────────────────┘
```

Only `ll_enc.c` touches the TI radio. All SMP protocol state lives in Python where iteration on lock-specific quirks is fast.

## Components

### Firmware

**`firmware/cc1352/src/l2cap_smp.c` (~80 LOC)** — wire framing for L2CAP CID 0x0006, mirror of `att_client.c`'s CID 0x0004 plumbing.

- `void L2capSmp_handleRx(const uint8_t *l2cap_payload, uint16_t len)` — called from `ble_conn_mgr.c` L2CAP demux when CID==0x0006. Strips header, emits `RSP_SMP_PDU(seq=0, smp_pdu_bytes)` async event.
- `bool L2capSmp_tx(const uint8_t *smp_pdu, uint8_t pdu_len)` — wraps `[len:2LE][CID=0x0006:2LE][smp_pdu]` and queues to LL TX.

**`firmware/cc1352/src/ll_enc.c` (~120 LOC)** — encryption setup. Sole module that touches `Ble5_0_cmdBle5Master.pParams->encryption.*`.

- `bool LlEnc_start(const uint8_t ltk[16], const uint8_t ediv[2], const uint8_t rand[8])`:
  1. Verify `s_state.connected && !s_state.encrypted`.
  2. Generate `SKDm[8]`, `IVm[4]` via TRNG.
  3. TX `LL_ENC_REQ(Rand[8], EDIV[2], SKDm[8], IVm[4])`.
  4. RX `LL_ENC_RSP(SKDs[8], IVs[4])`.
  5. `SK = AES-128-ECB(LTK, SKDm||SKDs)` (16-byte input from concatenation).
  6. Configure TI radio: `pParams->encryption.{bEncryption=1, key=SK, ivM, ivS}`.
  7. RX `LL_START_ENC_REQ`, TX `LL_START_ENC_RSP`, RX `LL_START_ENC_RSP`.
  8. `s_state.encrypted = true`. Emit `RSP_ENC_ACTIVE`.
- Timeout: 10s. On timeout or `LL_REJECT_IND` → emit `RSP_ERROR(ENC_TIMEOUT|ENC_REJECTED)`.

**`firmware/cc1352/src/ble_conn_mgr.c` (modify, +20 LOC)** — extend L2CAP demux to route CID 0x0006 to `L2capSmp_handleRx`. Extend LL opcode dispatch to delegate `LL_ENC_REQ` (hook only, initiator doesn't send), `LL_ENC_RSP`, `LL_START_ENC_REQ`, `LL_START_ENC_RSP`, `LL_REJECT_IND`, `LL_REJECT_EXT_IND` to `ll_enc.c`.

**`firmware/cc1352/src/command_processor.c` (modify, +30 LOC)** — two new command IDs:
- `CMD_SMP_PDU(seq, pdu_bytes)` → `L2capSmp_tx(pdu_bytes)` → `RSP_ACK` or `RSP_ERROR(NO_CONNECTION|L2CAP_QUEUE_FULL)`.
- `CMD_ENABLE_LL_ENC(seq, LTK[16], EDIV[2], Rand[8])` → `LlEnc_start(...)` → async `RSP_ENC_ACTIVE` or `RSP_ERROR(INVALID_STATE|ENC_TIMEOUT|ENC_REJECTED)`.

### Python

**`python/feralrf/smp/__init__.py`** — re-exports public API.

**`python/feralrf/smp/state_machine.py` (~280 LOC)**
```python
class SmpInitiator:
    def __init__(
        self,
        radio: "Radio",
        io_caps: IoCaps = IoCaps.NoInputNoOutput,
        oob_flag: bool = False,
        mitm: bool = False,
        bonding: bool = True,
        max_key_size: int = 16,
        pdu_timeout: float = 30.0,
        pairing_timeout: float = 60.0,
    ): ...

    def pair_just_works(self) -> BondInfo: ...
    def pair_passkey(self, pin: int) -> BondInfo: ...  # pin in 0..999999
```

State enum: `IDLE → FEATURE_EXCHANGE → CONFIRM_EXCHANGE → RANDOM_EXCHANGE → ENCRYPTION_STARTED → KEY_DISTRIBUTION → BONDED | FAILED(reason)`.

**`python/feralrf/smp/crypto.py` (~80 LOC)** — pure functions:
- `e(k: bytes, plaintext: bytes, *, backend: str = "firmware") -> bytes` — AES-128-ECB single block. `backend="firmware"` uses `radio.aes_encrypt(mode='ecb')`; `backend="pure_python"` uses Python `cryptography` lib (offline Crackle path).
- `c1(k, r, pres, preq, iat, ia, rat, ra) -> bytes` — BT Core Spec Vol 3 Part H §2.2.3 confirm value.
- `s1(k, r1, r2) -> bytes` — BT Core Spec §2.2.4 short-term key.

**`python/feralrf/smp/pdu.py` (~120 LOC)** — dataclasses per BT Core Spec Vol 3 Part H §3.5:
- `PairingRequest`, `PairingResponse`, `PairingConfirm`, `PairingRandom`, `PairingFailed`
- `EncryptionInformation`, `MasterIdentification`, `IdentityInformation`, `IdentityAddressInformation`, `SigningInformation`
- Each: `to_bytes() -> bytes`, `@classmethod from_bytes(cls, raw: bytes) -> "Self"`, opcode constant.

**`python/feralrf/smp/bond.py` (~50 LOC)**
```python
@dataclass(frozen=True)
class BondInfo:
    peer_addr: bytes        # 6 LSB-first
    peer_addr_type: int     # 0 public, 1 random
    ltk: bytes              # 16
    ediv: bytes             # 2
    rand: bytes             # 8
    irk: Optional[bytes] = None      # 16
    csrk: Optional[bytes] = None     # 16
```

**`python/feralrf/crackle.py` (~200 LOC, standalone)**
```python
class CrackleDecoder:
    def __init__(self, *, passkey_range: range = range(1_000_000)): ...

    def decrypt_pcap(self, path: str | Path) -> Iterator[DecryptedPdu]: ...

@dataclass
class DecryptedPdu:
    timestamp: float
    role: Literal["master", "slave"]
    plaintext: bytes
```

Pure Python (no firmware dependency). Uses `python/feralrf/smp/crypto.py` with `backend="pure_python"` for portability.

**`python/feralrf/radio.py` (modify, +40 LOC)** — public wrapper methods:
- `radio.smp_pair_just_works(**kwargs) -> BondInfo` — instantiates `SmpInitiator`, runs.
- `radio.smp_pair_passkey(pin: int, **kwargs) -> BondInfo`
- `radio.smp_send_pdu(pdu_bytes: bytes)` — low-level for SmpInitiator internal use.
- `radio.smp_recv_pdu(timeout: float = 30.0) -> bytes`
- `radio.enable_ll_enc(ltk, ediv, rand) -> None` — wraps `CMD_ENABLE_LL_ENC`, waits for `RSP_ENC_ACTIVE`.

### Tests

| File | LOC | Coverage |
|------|-----|----------|
| `python/tests/test_smp_crypto.py` | 60 | c1, s1, e against BT Core Spec test vectors; both backends |
| `python/tests/test_smp_pdu.py` | 80 | Round-trip for each PDU type + malformed-input rejection |
| `python/tests/test_smp_state_machine.py` | 120 | Happy paths (JW + Passkey), all failure modes via MockSmpRadio |
| `python/tests/test_crackle.py` | 80 | JW decrypt, Passkey brute-force, LE SC rejection, incomplete-exchange error |
| `python/examples/smoke_smp_just_works.py` | 50 | Manual hardware smoke for engagement |
| `python/examples/smoke_smp_passkey.py` | 50 | Manual hardware smoke with `--pin` |
| `python/examples/smoke_crackle.py` | 30 | Run CrackleDecoder against a captured pcap |

Coverage target: >85% line coverage on `feralrf.smp.*` and `feralrf.crackle`. `pytest -m 'not hardware'` must be 100% green pre-commit.

## Data flow

### Just Works pairing
1. Host: `radio.smp_pair_just_works()` builds `PairingRequest(io_cap=NoInputNoOutput, oob=0, auth_req=Bonding)`, sends via `CMD_SMP_PDU`.
2. Firmware: `L2capSmp_tx` frames it and queues to LL.
3. Peer responds with `PairingResponse`; firmware delivers as `RSP_SMP_PDU` event.
4. Host: `TK = b"\x00" * 16`. Generates `Mrand`. Computes `Mconfirm = c1(TK, Mrand, pres, preq, iat, ia, rat, ra)`. Sends `PairingConfirm(Mconfirm)`.
5. Peer responds with `PairingConfirm(Sconfirm)`.
6. Host sends `PairingRandom(Mrand)`. Peer responds with `PairingRandom(Srand)`.
7. Host verifies `c1(TK, Srand, ...) == Sconfirm`. If mismatch → send `PairingFailed(0x04)` → raise `SmpFailure('confirm_mismatch')`.
8. Host computes `STK = s1(TK, Mrand, Srand)`. Calls `radio.enable_ll_enc(STK, EDIV=0, Rand=0)`.
9. Firmware `ll_enc.c` TXes `LL_ENC_REQ`, derives `SK = e(STK, SKDm||SKDs)`, configures radio encryption, completes `LL_START_ENC` handshake, emits `RSP_ENC_ACTIVE`.
10. Host enters `KEY_DISTRIBUTION` state. Waits up to 5s for encrypted `EncryptionInformation(LTK)` + `MasterIdentification(EDIV, Rand)` PDUs from peer. Returns `BondInfo`.

### Passkey Entry
Identical to Just Works, but `TK = pin.to_bytes(16, 'big')` (zero-padded BE 6-digit PIN, e.g. 123456 → `\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\xE2\x40`).

### Crackle offline
1. Load pcap from disk.
2. Filter for L2CAP CID 0x0006 frames → SMP exchange.
3. Parse `PairingRequest`, `PairingResponse`, `PairingConfirm` (both), `PairingRandom` (both).
4. Extract `ia, iat, ra, rat` from earlier `CONNECT_IND`.
5. Determine method: NoInputNoOutput on either side → Just Works (TK=0). Else → Passkey Entry, brute-force `pin in passkey_range` until `c1(TK=pin, Mrand, ...) == Mconfirm`.
6. Compute `STK = s1(TK, Mrand, Srand)`.
7. Find `LL_ENC_REQ` and `LL_ENC_RSP` in pcap (LL Control PDUs, not SMP). Extract `SKDm`, `SKDs`. Compute `SK = e(STK, SKDm||SKDs)`.
8. For each encrypted LL Data PDU after `LL_START_ENC_RSP`: decrypt with `AES-CCM(SK, nonce=packet_counter||IVm||IVs, aad=header)`.
9. Yield `DecryptedPdu(timestamp, role, plaintext)`.

## Error handling

### Transport
- `CMD_SMP_PDU` with no LL connection → `RSP_ERROR(NO_CONNECTION)`.
- `CMD_ENABLE_LL_ENC` before pairing complete → `RSP_ERROR(INVALID_STATE)`.
- L2CAP TX queue full → `RSP_ERROR(L2CAP_QUEUE_FULL)`.

### Protocol (peer-sent)
- `Pairing Failed` PDU: parsed per Vol 3 Part H §3.5.5. Mapped to `SmpFailure(reason)`:
  - 0x01 → `'passkey_wrong'`
  - 0x03 → `'auth_req_mismatch'`
  - 0x04 → `'confirm_value_failed'`
  - 0x05 → `'peer_no_smp'` (hint: lock may require LE SC → Phase B)
  - 0x08 → `'peer_unspecified'`
  - other → `'peer_reason_0x{code:02X}'`
- Unexpected opcode for current state → host sends `PairingFailed(UnspecifiedReason)` → `SmpFailure('state_violation')`.
- Bad PDU length → same.

### Crypto verification
- Sconfirm mismatch on Random exchange → host sends `PairingFailed(ConfirmValueFailed=0x04)` → `SmpFailure('confirm_mismatch')`. This is how wrong PIN manifests on initiator.

### Timing
- Per-PDU receive timeout: 30s (configurable). Exceeded → `SmpFailure('timeout', stage='<state>')`.
- Total pairing timeout: 60s wall clock. Exceeded → send `PairingFailed(UnspecifiedReason)` + `CMD_DISCONNECT` → `SmpFailure('total_timeout')`.

### Connection drop
- Firmware emits async `RSP_DISCONNECT(reason)`. Pending `_recv_*` calls in SmpInitiator raise `SmpFailure('disconnected', reason=...)`.

### LL encryption
- `LL_REJECT_IND` or `LL_REJECT_EXT_IND` from peer → firmware emits `RSP_ERROR(ENC_REJECTED, code)` → host raises `SmpFailure('ll_enc_rejected', error_code=...)`.
- 10s timeout in `ll_enc.c` if peer never sends `LL_START_ENC_REQ` → `RSP_ERROR(ENC_TIMEOUT)`.

### Bond key distribution
- Some locks pair with `Bonding=0` (session-only). Host times out the post-encryption key-distribution wait at 5s and accepts session-only bond (`BondInfo.ltk = STK`).

### Crackle-specific
- Passkey range exhausted → `CrackleError('passkey_not_in_range')`.
- Missing PDUs in capture → `CrackleError('incomplete_exchange', missing=[...])`.
- LE Secure Connections detected → `CrackleError('le_sc_not_supported', hint='Phase B required')`.

## Done criteria

**Phase A complete when:**
- All unit tests pass (`pytest -m 'not hardware'` → 100%).
- Coverage >85% on `feralrf.smp.*` and `feralrf.crackle`.
- BT Core Spec test vectors for c1, s1, e verified with both `firmware` and `pure_python` backends.
- Manual smoke against at least one real peripheral (engagement lock) succeeds for either Just Works or Passkey Entry, producing a valid `BondInfo`.
- Crackle decryptor successfully decrypts a synthetic pcap (test fixture) for both Just Works and Passkey paths.
- Pre-commit clean (clang-format, black, isort, flake8, mypy).
- Tag `v2.0-smp-phase-a` pushed.

## Risks & open questions

- **Lock-specific quirks**: vendors may deviate from BT Core Spec in subtle ways (timing, unsolicited PDUs, custom AuthReq combos). Mitigation: state machine in Python allows fast iteration. Each quirk gets a regression test.
- **Peer requires LE SC**: many modern locks reject legacy pairing entirely (`PairingFailed(NotSupported)`). Mitigation: detect early via AuthReq.SC bit in PairingResponse, fail with clear hint to Phase B.
- **Live integration test gap**: no responder role exists yet (F20.a.1 paused). Phase A is unit-test only for state machine; hardware validation is manual against engagement locks.
- **AES-128-ECB backend latency**: firmware backend adds ~5ms USB round-trip per `e()` call. For pairing, this is ~6 calls = 30ms — negligible vs total pairing time (~500ms-2s). For Crackle passkey brute-force (1M trials), MUST use `backend="pure_python"` to avoid 5000s firmware bottleneck.

## Out of scope (deferred)

- **Phase B**: LE Secure Connections (ECDH P-256, f1/f2/f3/f4/f5/f6, AES-CMAC, Numeric Comparison, OOB).
- **Phase C**: Persistent bond storage (file-backed bond database).
- **SMP Responder role**: requires F20.a.1.g (canonical TI CMD_BLE_SLAVE pivot) — separate spec.
- **MitM relay**: requires Phase B + F20.a.1.g. Future scope.
- **Reactive jamming + forced re-pair workflow**: separate phase.
