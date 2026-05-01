# F8b — GATT Investigation Toolkit

**Status:** design approved 2026-05-01.
**Author:** sabas + Claude.
**Parent roadmap:** `feature/ti-rtos-migration` post-F12 (HEAD `afb15f5`).
**Tag target:** `v2.0-f8b`.
**Successors:** F8c (MTU + descriptors + read-by-UUID + PHY update + DC reason),
F8d (RPA resolution + conn params + bond persistence + raw debug).

## Why

The 2026-05-01 Sony WH-CH720N investigation
(`docs/investigations/2026-05-01-sony-wh-ch720n.md`) hit three blockers
that are not WH-CH720N-specific — they would block any non-trivial BLE
peripheral exploration:

1. **No notification reception in firmware.** `att_client.c` handles
   READ + WRITE only. ATT opcodes `0x1B` (HANDLE_VALUE_NTF) and `0x1D`
   (HANDLE_VALUE_IND) are silently dropped. Subscribing to a CCC has no
   visible effect from the host. Bidirectional vendor protocols (Sony
   Headphones Connect, battery, button events) cannot be observed.

2. **No passive connection-follower mode.** When a non-FeralRF master
   (e.g., user's phone) is talking to a target peripheral, FeralRF
   cannot observe the conversation. Sniffle has this as its core
   feature (`fw/RadioTask.c`, `fw/PacketTask.c`). Porting it would let
   us capture full pairing exchanges and live app traffic.

3. **No pairing initiator.** Many peripherals require an encrypted
   link before allowing reads on protected characteristics (battery,
   custom protected services). FeralRF's BLE central can connect but
   cannot pair — chars that respond with "Insufficient Authentication"
   stay inaccessible.

F25 (commit `3b1100f`) just landed AES + ECDH + SHA-256 + AES-CMAC in
hardware. The crypto primitives needed for SMP are present; F8b
integrates them.

## Scope

F8b bundles three deliverables:

- **(1) GATT Notifications + Indications reception (central client).**
  Subscribe to a CCC, receive async push notifications and
  auto-acknowledged indications.
- **(2b) Passive connection follower (Sniffle-style observer).**
  Capture-only mode that follows a non-FeralRF connection and emits
  every LL data PDU as `RSP_LL_PACKET` with raw bytes.
- **(3) Pairing initiator (Just Works only).** SMP state machine that
  pairs the FeralRF central with peripherals using LE Secure
  Connections + Just Works association. Numeric Comparison and
  Passkey Entry deferred to F8c.

**Out of scope** (deferred to later phases):

- Bond persistence (NVM storage of LTK/IRK) → F8d.
- MTU exchange, Data Length Extension → F8c.
- Read by UUID, descriptor enumeration → F8c.
- PHY update post-connect → F8c.
- IRK-based RPA resolution → F8d.

## Decisions

| # | Decision | Resolution |
|---|----------|------------|
| 1 | Phase decomposition | F8b → F8c → F8d, sequential, each with its own spec/plan/impl cycle. |
| 2 | F8b feature bundle | (1) Notifications + (2b) Sniffle observer + (3) Pairing Just Works. |
| 3 | SMP modes in (3) | Just Works only. NC + PE deferred to F8c. |
| 4 | Notification delivery model | Async push via new `RSP_GATT_NOTIFY` frame, matching `gatt_discover` streaming pattern. |
| 5 | Connection follower scope | Capture-only firmware. LL parsing in host-side Python helper. |
| 6 | Sniffle code reuse | Vendor with provenance comments under `firmware/cc1352/sniffle/`. License compatible (both GPL-3.0). |
| 7 | Test peripheral | Sony WH-CH720N only. Tests assert structural properties (e.g., "≥1 notif on h564 after writing h563"), not exact payloads. |

## Architecture

F8b adds three independent subsystems to the CC1352 firmware plus three
parallel surfaces in `python/feralrf/`. The three share the RF handle
and COBS pipe but are otherwise orthogonal — they can be implemented
and tested in any order.

```
                      ┌──────────────────────────────────────────┐
HOST (Python)         │  feralrf.Radio (existing)                │
                      │   ├─ subscribe / read_gatt_notifications │ ← (1)
                      │   ├─ follow_connection / read_ll_packets │ ← (2b)
                      │   └─ pair / encrypted GATT context       │ ← (3)
                      └──────────────────┬───────────────────────┘
                                         │ COBS-framed binary protocol
                                         │ (existing transport)
                      ┌──────────────────┴───────────────────────┐
FIRMWARE (CC1352)     │  command_processor.c (existing)          │
                      │   dispatch new opcodes:                  │
                      │   ├─ CMD_GATT_SUBSCRIBE        → (1)     │
                      │   ├─ CMD_FOLLOW_START/STOP     → (2b)    │
                      │   └─ CMD_PAIR / CMD_PAIR_STATUS → (3)    │
                      └──┬────────────┬────────────┬─────────────┘
                         ▼            ▼            ▼
                 ┌─────────────┐  ┌─────────────┐  ┌────────────┐
                 │ att_client  │  │ ll_follower │  │   smp.c    │
                 │   .c        │  │    .c       │  │            │
                 │ (extend NTF │  │  (vendored  │  │ Just Works │
                 │  / IND)     │  │  Sniffle)   │  │ uses F25   │
                 │             │  │             │  │ crypto     │
                 └─────┬───────┘  └─────┬───────┘  └─────┬──────┘
                       │                │                │
                       ▼                ▼                ▼
                 ┌──────────────────────────────────────────┐
                 │  RF driver (Ble5_0_cmdBle5GenericRx,     │
                 │  CMD_BLE5_MASTER, etc.) — existing       │
                 └──────────────────────────────────────────┘
```

**Key architectural points:**

- **(1) Notifications** are an extension of `att_client.c` (no new
  file). Adds two opcode handlers (`0x1B`, `0x1D`) to the existing ATT
  incoming switch. On a handle-value packet the firmware emits
  `RSP_GATT_NOTIFY` directly. INDICATEs auto-respond with CFM (`0x1E`)
  so the peripheral does not stall.

- **(2b) Connection follower** lives in a new file `ll_follower.c`,
  vendored from Sniffle. It is a *radio mode* that is mutually
  exclusive with BLE central — you cannot follow a connection while
  acting as master. The state machine is: `IDLE → SCANNING_ADV →
  CONNECT_IND_CAPTURED → FOLLOWING → IDLE`. Terminates on
  `CMD_FOLLOW_STOP`, supervision timeout, or peer disconnect.

- **(3) Pairing** lives in a new file `smp.c`. After a normal BLE
  central connect, `r.pair()` runs the SMP exchange via SMP channel
  (CID `0x0006`) ATT opcodes `0x01-0x0F`. Uses F25
  `crypto_engine_ecdh()` (P-256) + `crypto_engine_aes_cmac()` for the
  handshake. Resulting LTK lives in RAM only, sent to the RF core via
  a new `RadioIF_setLlEncryption(rand, ediv, ltk)` helper to encrypt
  the LL link.

**What does not change:** the COBS pipe, the Radio base API, the
existing GATT commands (DISCOVER/READ/WRITE), the scan + connect
pattern, the F25 crypto module.

## Components

### Firmware (C, CC1352)

#### `firmware/cc1352/src/att_client.c` — EXTENDED (~80 LOC)

Add two opcode handlers to the incoming ATT dispatch:

```c
case 0x1B: /* ATT_HANDLE_VALUE_NOTIFICATION */
    AttClient_emitNotification(&data[1], len - 1);   /* [handle:2][value:N] */
    break;
case 0x1D: /* ATT_HANDLE_VALUE_INDICATION */
    AttClient_emitNotification(&data[1], len - 1);
    AttClient_sendCfm();                              /* required ack 0x1E */
    break;
```

```c
static void AttClient_emitNotification(const uint8_t *data, uint8_t len) {
    send_response(RSP_GATT_NOTIFY, s_active_seq, data, len);
}
```

No new state, no buffer, no subscription tracking — that is the host's
responsibility.

#### `firmware/cc1352/src/smp.c` — NEW (~600 LOC)

Just Works SMP state machine. States:

```
IDLE → PAIRING_REQ_SENT → PUBLIC_KEY_EXCHANGE → DHKEY_CHECK
     → LTK_DERIVED → LL_ENC_REQ_SENT → ENCRYPTED → IDLE
```

Public API:

```c
bool       Smp_init(void);
bool       Smp_pairJustWorks(void);
bool       Smp_isEncrypted(void);
SmpStatus  Smp_getStatus(void);
```

Dependencies:

- `att_client.c` for sending Pairing Request via opcode `0x01` over the
  SMP CID (`0x0006`).
- `crypto_engine.c` (F25) for `ecdh()` P-256, `aes_cmac()`,
  `random_bytes()`.
- `radio_if.c` for the new helper
  `RadioIF_setLlEncryption(rand, ediv, ltk)` that programs the RF core
  with the negotiated LTK.

#### `firmware/cc1352/sniffle/ll_follower.c` — NEW (~500 LOC vendored)

Adapted from Sniffle's `RadioTask.c` + `PacketTask.c`, integrated with
FeralRF's `ti_drivers_config` and `radio_if`.

```c
bool             LlFollower_init(void);
bool             LlFollower_start(const uint8_t *trigger_mac /* NULL = wildcard */);
bool             LlFollower_stop(void);
LlFollowerStats  LlFollower_getStats(void);
```

Vendored support files: `csa2.c` (Channel Selection Algorithm #2),
`adv_header_cache.c` (ADV deduplication), `AuxAdvScheduler.c`. Each
file gets a header noting upstream provenance, original author, SPDX,
and a "Modifications by Electronic Cats / FeralRF (2026-05)" line.

No internal buffer — each captured packet emits as `RSP_LL_PACKET`
immediately. No dependency on `smp.c` or `att_client.c`.

#### `firmware/cc1352/src/command_processor.c` — EXTENDED (~150 LOC)

New opcodes:

```c
#define CMD_GATT_SUBSCRIBE   0x46u  /* [handle:2][enable:1]    → ACK | ERROR */
#define CMD_FOLLOW_START     0x50u  /* [trigger_mac:6 or zero] → ACK */
#define CMD_FOLLOW_STOP      0x51u  /* []                      → ACK */
#define CMD_PAIR             0x52u  /* [method:1=just_works]   → ACK + RSP_PAIR_DONE */
#define CMD_PAIR_STATUS      0x53u  /* []                      → RSP_PAIR_STATUS */

#define RSP_GATT_NOTIFY      0x95u  /* [handle:2][value:N] (async) */
#define RSP_LL_PACKET        0x96u  /* [direction:1][channel:1][rssi:1][seqn:2][raw_pdu:N] */
#define RSP_PAIR_DONE        0x97u  /* [status:1][reason:1] */
#define RSP_PAIR_STATUS      0x98u  /* [encrypted:1][ltk_present:1][counter:2] */
```

Dispatch handlers ~30 LOC each.

### Python (`python/feralrf/`)

#### `radio.py` — EXTENDED (~250 LOC)

```python
def gatt_subscribe(self, handle: int, enable: bool = True,
                   indicate: bool = False, timeout: float = 3.0) -> None: ...

def read_gatt_notifications(self, timeout: float = 5.0
                            ) -> Iterator["GattNotification"]: ...

def follow_connection(self, trigger_mac: Optional[str] = None,
                      timeout: float = 5.0) -> None: ...
def stop_follow_connection(self) -> None: ...

def read_ll_packets(self, timeout: float = 30.0
                    ) -> Iterator["LLPacket"]: ...

def pair(self, method: str = "just_works",
         timeout: float = 30.0) -> "PairResult": ...
def is_encrypted(self) -> bool: ...
def pair_status(self) -> "PairStatus": ...
```

New dataclasses (in `radio.py` or split into `_gatt_events.py`):

```python
@dataclass
class GattNotification:
    handle: int
    value: bytes
    timestamp: float       # host monotonic

@dataclass
class LLPacket:
    direction: str         # "M->S" or "S->M"
    channel: int
    rssi_dbm: int
    seqn: int
    payload: bytes
    timestamp: float

@dataclass
class PairResult:
    status: int            # 0 = ok, nonzero = failure
    reason: Optional[str]
    ltk: Optional[bytes]   # debug mode only

@dataclass
class PairStatus:
    encrypted: bool
    ltk_present: bool
    counter: int
```

#### `python/feralrf/_ll_parser.py` — NEW (~200 LOC)

```python
def parse_ll_pdu(payload: bytes) -> "LLPdu": ...
def parse_att_pdu(payload: bytes) -> "AttPdu": ...
def export_pcap(packets: List["LLPacket"], filename: str) -> None: ...
```

Decodes BLE LL opcodes (`0x00-0x1F`) and ATT opcodes (`0x01-0x1E`),
maps to readable names. Produces a Wireshark-importable pcap-NG.

#### `commands.py` and `responses.py` — EXTENDED (~50 LOC)

Add the 5 new `CMD_` and 4 new `RSP_` opcodes to the enum and builder
methods.

### Total deltas

See **Effort estimate** below for the authoritative per-feature LOC
breakdown. Summary: 6 new firmware files (`smp.c`, `ll_follower.c`,
plus 4 vendored Sniffle helpers), 3 edited firmware files
(`att_client.c`, `command_processor.c`, `radio_if.c`), 1 new Python
file (`_ll_parser.py`), 3 edited Python files (`radio.py`,
`commands.py`, `responses.py`), 3+ smoke scripts, 3 unit-test files.

## Data flow

### (1) Notifications — end-to-end trace

```
Host                          Firmware (CC1352)              Sony WH-CH720N
────                          ─────────────────              ──────────────

r.gatt_subscribe(handle=212)
  ├─ CMD_GATT_SUBSCRIBE   ──→  command_processor:
  │  [212:le, enable=1]        dispatch CMD_GATT_SUBSCRIBE
  │                              ↓
  │                            AttClient_writeCcc(212, 0x0001)
  │                              ↓ (existing GATT_WRITE path)
  │                            ATT_WRITE_REQ op=0x12 to h213  ──→  CCC enabled
  │                              ↓                            ←──    ATT_WRITE_RSP
  │                            send ACK to host
  │ ←─ RSP_ACK
  │
  │  (user presses NC button on headphones)
  │                                                                    ↓
  │                            ATT incoming dispatch in    ←──   ATT op=0x1B
  │                            att_client.c hits new                [handle=212][value=...]
  │                            case 0x1B:
  │                              ↓
  │                            AttClient_emitNotification()
  │                              ↓
  │                            build RSP_GATT_NOTIFY frame
  │                              [handle:2][value:N]
  │                              ↓
  │ ←─ RSP_GATT_NOTIFY (async, no prior CMD)
  │
for n in r.read_gatt_notifications():
  yield GattNotification(handle=212, value=b'...', timestamp=...)
```

The `RSP_GATT_NOTIFY` is unsolicited — not a response to any pending
CMD. Python's `_read_response` already tolerates unsolicited frames
(F12 active scan also relies on this). `read_gatt_notifications()`
iterates the RX stream filtering for `RSP_GATT_NOTIFY`.

### (2b) Connection follower — end-to-end trace

```
Host                          Firmware (CC1352)              Sony ↔ Phone
────                          ─────────────────              ─────────────

r.follow_connection(trigger_mac="A8:E6:E8:8A:7D:F8")
  ├─ CMD_FOLLOW_START   ──→  LlFollower_start(mac):
  │  [mac:6]                   state = SCANNING_ADV
  │                            Ble5_0_cmdBle5GenericRx in
  │                            ch37/38/39 with addr filter
  │ ←─ RSP_ACK
  │                            (waits for ADV from Sony)
  │                              ↓
  │                            Sony advertises    ←──  ADV_IND
  │                            (filter match)
  │                              ↓
  │                            Phone connects     ←──  CONNECT_IND
  │                              ↓                       (captured!)
  │                            Parse CONNECT_IND:
  │                              AccessAddress, CRCInit,
  │                              WinSize, Interval,
  │                              ChannelMap, Hop, SCA
  │                              ↓
  │                            csa2_init(ChannelMap, Hop)
  │                              ↓
  │                            state = FOLLOWING
  │                            Ble5_0_cmdBle5GenericRx with
  │                            new AA, hopping per CSA #2
  │
  │ ←─ RSP_LL_PACKET    ←──  Each captured data PDU
  │   (async stream)         [direction:1][ch:1][rssi:1]
  │                          [seqn:2][payload:N]
  │
for pkt in r.read_ll_packets():        ↑
  yield LLPacket(...)                   │  (continues until disconnect,
                                        │   supervision timeout,
                                        │   or CMD_FOLLOW_STOP)
                                        │
r.stop_follow_connection()              │
  ├─ CMD_FOLLOW_STOP    ──→  LlFollower_stop()
  │                            state = IDLE
  │ ←─ RSP_ACK                 RF_cancelCmd, RF_flushCmd
```

Capturing the CONNECT_IND is timing-critical — it is sent once on one
of ch37/38/39. The follower scans the three channels rotating quickly.
After capture it switches to connection mode with CSA #2 and follows
the hopping. If the CONNECT_IND is missed, the host must wait for
phone to reconnect (typically seconds to minutes).

### (3) Pairing Just Works — end-to-end trace

```
Host                          Firmware (CC1352)              Sony WH-CH720N
────                          ─────────────────              ──────────────

(connection established + gatt_discover ran)

r.pair(method="just_works")
  ├─ CMD_PAIR          ──→  Smp_pairJustWorks():
  │  [method=0x00]            state = PAIRING_REQ_SENT
  │                             ↓
  │                           SMP op=0x01 PAIRING_REQUEST   ──→  Sony
  │                           (over CID 0x0006 SMP channel,
  │                            IO=NoInputNoOutput, OOB=No,
  │                            AuthReq=SC bit set)
  │ ←─ RSP_ACK
  │                                                          ←──    PAIRING_RESPONSE SMP op=0x02
  │                           state = PUBLIC_KEY_EXCHANGE
  │                             ↓
  │                           crypto_engine_random_bytes(32)
  │                             ↓
  │                           crypto_engine_ecdh(my_priv, …)
  │                             ↓
  │                           SMP op=0x0C PUBLIC_KEY_X+Y   ──→
  │                                                         ←──    SMP op=0x0C peer pub
  │                             ↓
  │                           crypto_engine_ecdh() → DHKey
  │                             ↓
  │                           state = DHKEY_CHECK
  │                           compute Ea, Eb (f5/f6 via AES-CMAC)
  │                             ↓
  │                           SMP op=0x0D Ea               ──→
  │                                                         ←──    SMP op=0x0D Eb
  │                           verify Eb == expected
  │                             ↓
  │                           state = LTK_DERIVED
  │                           derive LTK via f5
  │                             ↓
  │                           state = LL_ENC_REQ_SENT
  │                           RadioIF_setLlEncryption(rand, ediv, ltk)
  │                             ↓
  │                           RF core sends LL_ENC_REQ      ──→
  │                                                          ←──    LL_ENC_RSP
  │                                                          ←──    LL_START_ENC_REQ
  │                           RF core auto-encrypts subsequent LL packets
  │                             ↓
  │                           state = ENCRYPTED → IDLE
  │ ←─ RSP_PAIR_DONE [status=0][reason=0]

# subsequent reads/writes use the encrypted link transparently
data = r.gatt_read(0x60)   # battery, denied pre-pair, works now
```

F25 already provides `crypto_engine_ecdh()` (P-256),
`crypto_engine_aes_cmac()`, and `crypto_engine_random_bytes()`. New
work: the SMP state machine in `smp.c` and the `RadioIF_setLlEncryption()`
helper that programs the RF core with the LTK.

LTK is held in RAM only (`s_smp_state.ltk`); it is lost on disconnect.
NVM persistence is F8d.

## Error handling

### Cross-cutting: AttClient stale-state recovery

The bug filed in `memory/project_gatt_attclient_bug.md` says that after
a disconnect, the next connect succeeds at LL but no ATT data flows
(verified 2026-05-01). F8b is expected to **fix this at the root** in
`att_client.c` or the BLE central state machine.

If the fix is impractical within F8b (e.g., requires RF driver
changes), F8b **must** document the deferral explicitly in
`memory/project_gatt_attclient_bug.md` and keep `reset_device()` as
the sanctioned workaround. No silent debt.

**Acceptance gate for F8b:** the sequence `ble_connect → gatt_discover →
ble_disconnect → ble_connect → gatt_discover` must succeed 5/5 without
calling `reset_device()`.

### (1) Notifications — error cases

| Case | Detection | Response |
|------|-----------|----------|
| Subscribe write fails (invalid CCC handle, char without N/I prop) | ATT_ERROR_RSP from peer | Firmware forwards as `RSP_ERROR + ATT_error_code`. Python raises `CommandError`. |
| Subscribe OK but peripheral never sends notification | Host-side timeout | `read_gatt_notifications(timeout=N)` iterator ends quietly. No error raised. |
| Notification arrives malformed (len < 2) | `att_client.c` validates before emit | Drop silently, increment `s_metrics.bad_notif`. |
| Host buffer overflow (host slow) | RX ring buffer full → frames dropped at RX | Accepted for F8b. Documented in API docstring. Backpressure is F8c. |
| Disconnect while subscribed | Existing disconnect detection | Firmware emits existing `RSP_DISCONNECTED`. Python invalidates iterator. |

### (2b) Connection follower — error cases

| Case | Detection | Response |
|------|-----------|----------|
| Trigger MAC never advertises in window | `r.follow_connection(timeout=N)` host timeout | Firmware keeps scanning until `CMD_FOLLOW_STOP` or host timeout. Python raises `TimeoutError`. |
| CONNECT_IND captured but CSA #2 sync fails | 0 packets in >5 conn intervals post-capture | Firmware emits `RSP_FOLLOW_FAILED [reason=sync]`. Python raises `ProtocolError`. |
| Connection drops mid-follow | RF Core sees > supervision timeout | Firmware emits `RSP_FOLLOW_DONE [reason=peer/timeout]`. Iterator ends cleanly. |
| CPU saturation drops packets | RFQueue overflow counter | Periodically (every 100 packets) firmware emits `RSP_FOLLOW_STATS [captured:4][dropped:4]`. Host logs warning. |
| Channel hop drift accumulates | Not directly detectable | Mitigation: SDU drift estimation already in Sniffle's port. Trust the upstream impl. |
| START while already following | `s_follow_state != IDLE` | Reject with `RSP_ERROR + ERR_INVALID_STATE`. |

### (3) Pairing — error cases

| Case | Detection | Response |
|------|-----------|----------|
| Peripheral rejects (op `0x05` PAIRING_FAILED) | SMP layer receives op `0x05` | `RSP_PAIR_DONE [status=1][reason=ATT_REASON]`. Python raises `PairingError(reason)`. |
| ECDH key generation fails | `crypto_engine_random_bytes` returns error | Abort, `RSP_PAIR_DONE [status=2][reason=crypto_fail]`. |
| DHKey check mismatch (Ea/Eb) | f5/f6 verification fails | `RSP_PAIR_DONE [status=3][reason=mitm_suspected]`. **No retry** — pairing aborts hard. |
| LL_ENC_REQ rejected | RF Core reports encryption setup failed | `RSP_PAIR_DONE [status=4][reason=ll_enc_failed]`. Python raises `EncryptionError`. |
| Pairing takes >30s (user too slow) | Host timeout | Firmware keeps trying. Python raises `TimeoutError`. User can call `pair_status()` for current state. |
| Disconnect mid-pairing | Existing disconnect detection | State = IDLE, LTK discarded, `RSP_PAIR_DONE [status=5][reason=disconnected]`. |
| `pair()` called when already paired | `Smp_isEncrypted() == true` | Reject `ERR_INVALID_STATE`. Python raises `CommandError`. |

### Logging / observability

Per-feature counters in `s_metrics` (existing struct, extended):

- `notif_rx`, `notif_dropped`, `notif_bad`
- `follow_packets_captured`, `follow_packets_dropped`, `follow_sessions`
- `pair_attempts`, `pair_success`, `pair_failed_by_reason[8]`

Exposed via existing `r.get_stats()` (extended return dict).

## Testing

### Unit tests (Python, hardware-free, run in CI)

#### `python/tests/test_gatt_notifications.py` — NEW (~150 LOC)

- `test_subscribe_writes_correct_ccc_value`: mocks `gatt_write`,
  asserts `gatt_subscribe(handle=212)` writes `b"\x01\x00"` to handle
  `213` (CCC = `handle + 1`).
- `test_subscribe_indicate_writes_indicate_bit`: `enable_indicate=True`
  writes `b"\x02\x00"`.
- `test_read_gatt_notifications_yields_parsed`: feed mock
  `RSP_GATT_NOTIFY` to RX buffer, assert iterator yields
  `GattNotification(handle=212, value=..., timestamp=...)`.
- `test_iterator_stops_on_timeout`: empty RX, `timeout=0.1` returns in
  ≤0.2 s.
- `test_iterator_stops_on_disconnect`: feed `RSP_DISCONNECTED`
  mid-stream, iterator ends.

#### `python/tests/test_ll_packet_parser.py` — NEW (~200 LOC)

Vector-based, sourced from known Wireshark captures:

- `test_parse_ll_enc_req`: bytes `03 0a 24 00 ...` → `LLPdu(opcode=LL_ENC_REQ, ediv=0x0024, ...)`.
- `test_parse_att_write_req_handle_213`: bytes `04 ... 12 d5 00 01 00`
  → `AttPdu(opcode=ATT_WRITE_REQ, handle=0x00d5, value=b"\x01\x00")`.
- `test_parse_att_handle_value_notification`:
  bytes `04 ... 1b d4 00 ...` → `AttPdu(opcode=ATT_HANDLE_VALUE_NTF, handle=0x00d4, ...)`.
- `test_export_pcap_round_trip`: list of `LLPacket` → pcap file →
  parsed back with `dpkt`/`scapy` → bytes match.
- 12+ vectors covering: LL opcodes `0x03/0x05/0x07/0x0C`, ATT opcodes
  `0x01/0x02/0x08/0x09/0x12/0x13/0x1B/0x1D`, malformed inputs.

#### `python/tests/test_pairing.py` — NEW (~120 LOC)

Mocked, hardware-free:

- `test_pair_just_works_command_format`: `r.pair("just_works")` sends
  `CMD_PAIR [method=0x00]`.
- `test_pair_just_works_blocks_until_done`: feed
  `RSP_PAIR_DONE [status=0]` → returns `PairResult(status=0)`.
- `test_pair_failure_raises_pairing_error`: feed
  `RSP_PAIR_DONE [status=1, reason=0x05]` → raises
  `PairingError("Pairing Not Supported")`.
- `test_pair_when_already_encrypted`: mocked `is_encrypted=True` →
  `pair()` raises `CommandError(INVALID_STATE)`.
- `test_pair_invalid_method_raises`: `r.pair("passkey")` raises
  `ValueError` (Just Works only in F8b).

**Total unit:** ~470 LOC, ~30+ test cases. All hardware-free, run in
CI via `pytest -m "not hardware"`.

### Hardware smoke tests (manual, against WH-CH720N)

#### `python/examples/lab/smoke_f8b_notifications.py` — NEW

1. Reset firmware (workaround AttClient bug if not yet fixed).
2. Connect to WH-CH720N.
3. `gatt_discover()`.
4. Subscribe to handles 170, 186, 194, 212 (Sony custom services).
5. Wait 30 s while user presses headphones buttons.
6. Verify ≥1 notification received on any subscribed handle.

**Closure bar:** PASS if ≥1 notification captured in 30 s. Print bytes
for manual Sony protocol inspection.

#### `python/examples/lab/smoke_f8b_follower.py` — NEW

1. Reset firmware.
2. `r.follow_connection(trigger_mac="A8:E6:E8:8A:7D:F8", timeout=60)`.
3. User puts Sony in pair mode + opens Bluetooth on phone, pairs.
4. Capture packets for 30 s post-connection.
5. Verify ≥10 LL packets captured, ≥1 LL_ENC_REQ seen, ≥1 ATT_WRITE_REQ seen.
6. Export pcap to `/tmp/sony_pair.pcapng` for Wireshark.

**Closure bar:** PASS if ≥10 bidirectional packets captured and pcap
opens cleanly in Wireshark.

#### `python/examples/lab/smoke_f8b_pairing.py` — NEW

1. Reset firmware.
2. WH-CH720N in pair mode (factory reset, not bonded to anything).
3. Connect as BLE central.
4. `r.pair(method="just_works")`.
5. Verify `r.is_encrypted() == True`.
6. Read a previously-protected char (e.g., battery) — should return a
   value instead of "Insufficient Authentication".

**Closure bar:** PASS if pairing succeeds and the protected char read
returns ≥1 byte (vs error pre-pair).

#### `python/examples/lab/smoke_f8b_full.py` — NEW

End-to-end:

1. Reset.
2. Connect → discover → subscribe → pair → read protected → trigger
   button events → read notifications → disconnect.
3. Reconnect → repeat the same flow (validates the AttClient bug fix /
   reset_device avoidance).

**Closure bar:** PASS 3/3 consecutive runs without `reset_device()`
between them.

### Pre-commit gate

All new files pass:

- `clang-format` (firmware)
- `cppcheck` (firmware)
- `black`, `isort`, `flake8`, `mypy` (Python)
- CMake CC1352 build check (zero new warnings)

### CI

- Build firmware in CI (validates compilation; no flash).
- Run unit tests `pytest -m "not hardware"`.
- Hardware smokes are **manual** (require WH-CH720N + 1 board), run
  locally before tagging.

### Acceptance gates for `v2.0-f8b`

1. Unit tests: 30+ passing 100%.
2. Smoke notifications: ≥1 button event captured.
3. Smoke follower: valid pcap + ≥10 bidirectional packets.
4. Smoke pairing: encrypted link + protected char readable.
5. Smoke full integration: 3/3 consecutive runs.
6. Pre-commit clean across all files.
7. Firmware build OK.
8. AttClient stale-state bug status documented (fixed or explicitly
   deferred to F8c).

## Risks

- **SMP timing under TI-RTOS.** The Just Works flow has multiple
  request/response round-trips (PAIRING_REQ, PUB_KEY, DHKEY_CHECK,
  LL_ENC_REQ). Each leg has a default 30 s SMP timer. `project_f8a_session5`
  documented timing-class bugs (transmitWindowDelay double-applied).
  Mitigation: reuse the timing wrappers fixed in F8a; verify SMP
  responses arrive within their windows on first smoke run.
- **CSA #2 port from Sniffle.** Sniffle and FeralRF both target
  CC1352P7 with TI-RTOS + multi_protocol patch, but they use different
  override patterns and different AT-RTOS layouts. Risk: vendored code
  doesn't compile cleanly first try. Mitigation: small commits, file
  by file.
- **AttClient stale-state cross-cut.** Fixing the bug may surface
  deeper RF state-management issues. Mitigation: the acceptance gate
  for F8b allows explicit deferral to F8c with documented status, so
  F8b is not blocked.
- **WH-CH720N peripheral cooperativeness.** Tests depend on the user's
  Sony being in a known state. The flaky behavior observed on
  2026-05-01 (peripheral going non-responsive) might recur. Mitigation:
  smoke tests include explicit power-cycle steps for the peripheral.

## Effort estimate

| Feature | Sessions | Firmware LOC | Python LOC | Tests LOC |
|---------|----------|--------------|------------|-----------|
| (1) Notifications | 1 | ~80 | ~100 | ~150 |
| (2b) Connection follower | 1-2 | ~500 vendored + ~50 integ. | ~150 | ~200 |
| (3) Pairing Just Works | 2-3 | ~600 + ~80 RadioIF helper | ~200 | ~120 |
| Cross-cutting (AttClient bug) | 0.5-1 | ~50 | — | — |
| Build / pre-commit / smoke harness | 0.5 | — | — | — |
| **Total F8b** | **5-7** | **~1360** | **~450** | **~470** |

## Reference material

- Investigation `docs/investigations/2026-05-01-sony-wh-ch720n.md`
- Bug `memory/project_gatt_attclient_bug.md`
- Sniffle source: `/home/sabas/Documents/electroniccats/Sniffle/fw/`
- F25 crypto API in `firmware/cc1352/include/crypto_engine.h`
- Existing GATT stubs in `firmware/cc1352/src/att_client.c` and
  `firmware/cc1352/src/command_processor.c` (CMD_GATT_DISCOVER /
  GATT_READ / GATT_WRITE handlers).
- Skill `ti-rtos-rf-cc1352` (RF driver rules; F8b must not violate).
- Bluetooth Core Specification 5.4: SMP (Vol 3 Part H), ATT
  (Vol 3 Part F), LL (Vol 6 Part B).
