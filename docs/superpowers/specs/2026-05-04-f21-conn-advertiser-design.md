# F21 — BLE Connectable Advertiser (legacy ADV_IND / ADV_DIRECT_IND / ADV_SCAN_IND)

**Date:** 2026-05-04
**Branch (target):** `feature/f21-conn-advertiser` cut from `main` HEAD=`f831664`
**Tag (target):** `v2.0-f21` (full closure — criterio 1 PDU types + criterio 2 SCAN_RSP)
**Source:** Master plan v2.0 §F21 (`docs/superpowers/specs/2026-04-24-feralrf-plan-v2-design.md`).

## Goal

Agregar capacidad de emitir 3 PDU types BLE legacy connectable/scannable que el firmware actual no soporta — el path actual (`RadioIF_transmitBleAdvRaw`) hardcodea ADV_NONCONN_IND. Implementación R1: **1 nuevo comando firmware** dispatch interno a 3 TI BLE legacy commands; **3 métodos Python** en `Radio` class. TI CPE maneja SCAN_REQ→SCAN_RSP en hardware automáticamente.

## Scope decisions (brainstorm 2026-05-04)

- **A3 → R1:** Original A3 (Python-only first) NO viable porque firmware hardcodea PDU type a 0x2 vía `Ble5_0_cmdBleAdvNc`. Pivot a R1 — firmware mínimo (1 comando + 3 TI dispatch) en una vuelta. Cierra criterio 1 + 2 del spec sin requerir F20.
- **L1:** Métodos públicos en `Radio` class (spec-literal: `radio.advertise_ind`, etc).
- **V1.b:** Validación raw RX + `_ll_parser.parse_ll_pdu` con asserción `LLPduKind` per type. Bonus: F12 active scanner valida criterio 2 (SCAN_RSP).
- **CONNECT_IND sin F20:** TI command termina con `BLE_DONE_CONNECT`; firmware break-loop + retorna. Phone scanner sufre timeout, no afecta estado interno.

## Bundle layout

| Bundle | Cambios | Commits |
|--------|---------|---------|
| 1 — Firmware: protocol + handler + RadioIF wrapper | `protocol.h`, `command_processor.c`, `radio_if.c`, `radio_if.h` | 1 |
| 2 — Python: enums + CommandBuilder + Radio methods | `enums.py`, `commands.py`, `radio.py` | 1 |
| 3 — Tests unitarios | `test_radio_advertise.py` (new) | 1 |
| 4 — Smoke V1.b + demo lab | `smoke_f21_advertise.py`, `demo_advertise_connectable.py` | 1 |
| Final | Tag + memory + FF | — |

Total: 4 commits + 1 tag.

## Wire format `CMD_BLE_ADV_LEGACY` (0x52)

Common header (todos los pdu_types, 14 bytes):

```
byte 0:    pdu_type           (0x0 ADV_IND | 0x1 ADV_DIRECT_IND | 0x6 ADV_SCAN_IND)
byte 1:    adv_addr_type      (0=public, 1=random)
bytes 2-7: adv_addr (6 LE)
byte 8:    channel            (37 / 38 / 39)
byte 9:    power_dbm          (int8, signed)
bytes 10-11: count             (uint16 LE — 0 reservado/inválido)
bytes 12-13: interval_units    (uint16 LE; 0.625ms units. 16 = 10ms.)
```

Body por pdu_type:

**ADV_IND (0x0) y ADV_SCAN_IND (0x6):**
```
byte 14:   adv_data_len       (0-31)
bytes 15+: adv_data            (adv_data_len bytes)
byte X:    scan_rsp_len       (0-31, X = 15 + adv_data_len)
bytes X+1+: scan_rsp_data     (scan_rsp_len bytes)
```
Total payload: `16 + adv_data_len + scan_rsp_len`.

**ADV_DIRECT_IND (0x1):**
```
byte 14:   init_addr_type     (0=public, 1=random)
bytes 15-20: init_addr (6 LE)
```
NO adv_data, NO scan_rsp_data per BT Core Spec Vol 6 Part B §2.3.1.2. Total payload: 21 bytes.

## Firmware implementation

### `firmware/cc1352/include/protocol.h`

```c
#define CMD_BLE_ADV_LEGACY 0x52u
```

### `firmware/cc1352/src/command_processor.c` — `handle_ble_adv_legacy`

Pseudocode (full impl in plan):
1. Validate `payload_len >= 14`
2. Parse common header (14 bytes)
3. Validate `pdu_type ∈ {0x0, 0x1, 0x6}`, `channel ∈ {37,38,39}`, `power_dbm ∈ [-20, 20]`, `count >= 1`
4. Branch on `pdu_type`:
   - `0x0` / `0x6`: parse `adv_data_len`, `adv_data`, `scan_rsp_len`, `scan_rsp_data`. Validate offsets.
   - `0x1`: validate `payload_len == 21`. Parse `init_addr_type` + `init_addr` (6 bytes).
5. Call `RadioIF_transmitBleAdvLegacy(...)` with parsed args
6. `send_ack(seq)` (immediate ACK; RF_runCmd loop runs blocking after — see RadioIF section)

### `firmware/cc1352/src/radio_if.c` — `RadioIF_transmitBleAdvLegacy`

New function paralela a `RadioIF_transmitBleAdvRaw`. Signature:

```c
bool RadioIF_transmitBleAdvLegacy(
    uint8_t pdu_type,
    uint8_t addr_type, const uint8_t *addr,
    uint8_t channel, int8_t power_dbm,
    uint16_t count, uint16_t interval_units,
    const uint8_t *adv_data, uint8_t adv_data_len,
    const uint8_t *scan_rsp, uint8_t scan_rsp_len,
    uint8_t init_addr_type, const uint8_t *init_addr
);
```

Internal dispatch:
- `pdu_type == 0x0` → `Ble5_0_cmdBleAdv` (TI opcode 0x1805)
- `pdu_type == 0x1` → `Ble5_0_cmdBleAdvDir` (TI opcode 0x1806)
- `pdu_type == 0x6` → `Ble5_0_cmdBleAdvScan` (TI opcode 0x1808)

Setup steps (per TI command):
1. `RadioIF_applyBleChannelConfig(channel)` + `RadioIF_applyBlePhyMode(BLE_1M)` (legacy is 1M only)
2. Copy `adv_data` to `s_ble_adv_tx_payload`, `addr` to `s_ble_adv_tx_device_addr`, `scan_rsp` to new buffer `s_ble_scan_rsp_payload[31]`
3. Setup `cmd->pParams` per TI command type (advLen, scanRspLen, pAdvData, pScanRspData, pDeviceAddress, pPeerAddress for DIRECT, etc)
4. Setup `cmd->channel`, `whitening.bOverride=0`, `startTrigger.triggerType=TRIG_NOW`, `condition.rule=COND_NEVER`
5. Loop `count` times:
   - `events = RF_runCmd(s_rf_handle, cmd, RF_PriorityNormal, NULL, 0)`
   - If `cmd->status == BLE_DONE_CONNECT`: break (CONNECT_IND received, F20 not implemented)
   - `Task_sleep(MS_TO_TASK_TICKS(interval_units * 625 / 1000))` between iterations
6. Return true

### Prerequisites verification at Bundle 1 start

Verify TI symbols exposed in `firmware/cc1352/src/smartrf_ble5_0.c` or equivalent:

```bash
grep "Ble5_0_cmdBleAdv\b\|Ble5_0_cmdBleAdvDir\|Ble5_0_cmdBleAdvScan" firmware/cc1352/src/smartrf_*.c
```

If missing, add via SysConfig regeneration OR copy struct definitions from
`firmware/cc1352/sdk/.../source/ti/ble5stack/rf_patches/` per
`project_syscfg_handedited` memory pattern (similar to F25 hand-edit).

Verify `_ll_parser.LLPduKind` enum includes ADV_IND, ADV_DIRECT_IND, ADV_SCAN_IND:

```bash
grep "ADV_IND\|ADV_DIRECT\|ADV_SCAN_IND\|LLPduKind" python/feralrf/_ll_parser.py
```

If missing, add to enum + parser logic as part of Bundle 4.

## Python implementation

### `python/feralrf/enums.py`

```python
class Command(IntEnum):
    ...
    BLE_ADV_LEGACY = 0x52
```

### `python/feralrf/commands.py CommandBuilder`

```python
@staticmethod
def ble_adv_legacy(
    pdu_type: int,
    adv_addr_le: bytes,
    adv_addr_type: int = 1,
    channel: int = 37,
    power_dbm: int = 0,
    count: int = 50,
    interval_units: int = 16,  # 16 * 0.625ms = 10ms
    adv_data: bytes = b"",
    scan_rsp_data: bytes = b"",
    init_addr_le: bytes = b"",
    init_addr_type: int = 1,
) -> bytes:
    """Build CMD_BLE_ADV_LEGACY payload per F21 spec."""
    if pdu_type not in (0x0, 0x1, 0x6):
        raise ValueError(f"pdu_type must be 0x0/0x1/0x6, got 0x{pdu_type:X}")
    if len(adv_addr_le) != 6:
        raise ValueError("adv_addr_le must be 6 bytes")
    if power_dbm < -20 or power_dbm > 20:
        raise ValueError(f"power_dbm out of range: {power_dbm}")
    if channel not in (37, 38, 39):
        raise ValueError(f"channel must be 37/38/39, got {channel}")
    if count < 1 or count > 0xFFFF:
        raise ValueError(f"count must be in [1, 65535], got {count}")
    if interval_units < 1 or interval_units > 0xFFFF:
        raise ValueError(f"interval_units must be in [1, 65535], got {interval_units}")

    head = bytes([pdu_type, adv_addr_type]) + adv_addr_le + bytes([channel, power_dbm & 0xFF])
    head += struct.pack("<HH", count, interval_units)

    if pdu_type == 0x1:  # ADV_DIRECT_IND
        if len(init_addr_le) != 6:
            raise ValueError("ADV_DIRECT_IND requires init_addr_le (6 bytes)")
        return head + bytes([init_addr_type]) + init_addr_le

    # ADV_IND or ADV_SCAN_IND
    if len(adv_data) > 31:
        raise ValueError(f"adv_data > 31 bytes ({len(adv_data)})")
    if len(scan_rsp_data) > 31:
        raise ValueError(f"scan_rsp_data > 31 bytes ({len(scan_rsp_data)})")
    return (
        head
        + bytes([len(adv_data)]) + adv_data
        + bytes([len(scan_rsp_data)]) + scan_rsp_data
    )
```

### `python/feralrf/radio.py` — 3 nuevos métodos en `Radio`

```python
def advertise_ind(self, payload: bytes, scan_resp_data: bytes = b"",
                  target_addr: Optional[str] = None, count: int = 50,
                  channel: int = 37, power_dbm: int = 0,
                  interval_us: int = 10_000) -> None:
    """Emit ADV_IND (general connectable + scannable). TI handles SCAN_RSP automatically."""
    addr_le = self._resolve_target_addr(target_addr)
    interval_units = max(1, interval_us // 625)
    cmd_payload = CommandBuilder.ble_adv_legacy(
        pdu_type=0x0, adv_addr_le=addr_le, channel=channel, power_dbm=power_dbm,
        count=count, interval_units=interval_units,
        adv_data=payload, scan_rsp_data=scan_resp_data,
    )
    self._send_command(Command.BLE_ADV_LEGACY, cmd_payload)
    self._read_response(timeout=5.0, expected={Response.ACK, Response.ERROR})

def advertise_direct(self, target_addr: str, init_addr: str,
                     mode: str = 'low', count: int = 50,
                     channel: int = 37, power_dbm: int = 0) -> None:
    """Emit ADV_DIRECT_IND. mode='low' (10ms) | 'high' (3.75ms)."""
    addr_le = _mac_str_to_le_bytes(target_addr)
    init_le = _mac_str_to_le_bytes(init_addr)
    interval_us = 3_750 if mode == 'high' else 10_000
    interval_units = max(1, interval_us // 625)
    cmd_payload = CommandBuilder.ble_adv_legacy(
        pdu_type=0x1, adv_addr_le=addr_le, channel=channel, power_dbm=power_dbm,
        count=count, interval_units=interval_units,
        init_addr_le=init_le,
    )
    self._send_command(Command.BLE_ADV_LEGACY, cmd_payload)
    self._read_response(timeout=5.0, expected={Response.ACK, Response.ERROR})

def advertise_scan_ind(self, payload: bytes, scan_resp_data: bytes = b"",
                       target_addr: Optional[str] = None, count: int = 50,
                       channel: int = 37, power_dbm: int = 0,
                       interval_us: int = 10_000) -> None:
    """Emit ADV_SCAN_IND (scannable non-connectable)."""
    # Same as advertise_ind with pdu_type=0x6 — see _build_pdu_type wrapper or inline
    addr_le = self._resolve_target_addr(target_addr)
    interval_units = max(1, interval_us // 625)
    cmd_payload = CommandBuilder.ble_adv_legacy(
        pdu_type=0x6, adv_addr_le=addr_le, channel=channel, power_dbm=power_dbm,
        count=count, interval_units=interval_units,
        adv_data=payload, scan_rsp_data=scan_resp_data,
    )
    self._send_command(Command.BLE_ADV_LEGACY, cmd_payload)
    self._read_response(timeout=5.0, expected={Response.ACK, Response.ERROR})
```

`_resolve_target_addr` y `_mac_str_to_le_bytes` son helpers internos
(reusables de attacks/ble.py o nuevos en radio.py).

## Tests

### `python/tests/test_radio_advertise.py` — unit tests (~12-15)

```python
class TestBleAdvLegacyPayload:
    def test_payload_layout_adv_ind(self):
        p = CommandBuilder.ble_adv_legacy(
            pdu_type=0x0, adv_addr_le=b"\x06\x05\x04\x03\x02\x01",
            adv_data=b"HELLO", scan_rsp_data=b"WORLD",
        )
        # head 14 bytes + adv_len(1) + adv(5) + scan_len(1) + scan(5) = 26
        assert len(p) == 26
        assert p[0] == 0x0  # pdu_type
        assert p[1] == 0x1  # addr_type random
        assert p[2:8] == b"\x06\x05\x04\x03\x02\x01"
        assert p[8] == 37
        assert p[14] == 5  # adv_len
        assert p[15:20] == b"HELLO"
        assert p[20] == 5  # scan_rsp_len
        assert p[21:26] == b"WORLD"

    def test_payload_layout_adv_direct(self):
        p = CommandBuilder.ble_adv_legacy(
            pdu_type=0x1, adv_addr_le=b"\x06\x05\x04\x03\x02\x01",
            init_addr_le=b"\xfe\xee\xdd\xcc\xbb\xaa",
        )
        assert len(p) == 21
        assert p[0] == 0x1
        assert p[14] == 0x1  # init_addr_type random
        assert p[15:21] == b"\xfe\xee\xdd\xcc\xbb\xaa"

    def test_payload_layout_adv_scan_ind(self):
        p = CommandBuilder.ble_adv_legacy(
            pdu_type=0x6, adv_addr_le=b"\x06\x05\x04\x03\x02\x01",
            adv_data=b"X", scan_rsp_data=b"Y",
        )
        assert p[0] == 0x6

    def test_rejects_invalid_pdu_type(self):
        with pytest.raises(ValueError, match="pdu_type"):
            CommandBuilder.ble_adv_legacy(
                pdu_type=0x2, adv_addr_le=b"\x06\x05\x04\x03\x02\x01"
            )

    def test_rejects_invalid_channel(self):
        with pytest.raises(ValueError, match="channel"):
            CommandBuilder.ble_adv_legacy(
                pdu_type=0x0, adv_addr_le=b"\x06\x05\x04\x03\x02\x01",
                channel=36,
            )

    def test_rejects_oversized_adv_data(self):
        with pytest.raises(ValueError, match="adv_data"):
            CommandBuilder.ble_adv_legacy(
                pdu_type=0x0, adv_addr_le=b"\x06\x05\x04\x03\x02\x01",
                adv_data=b"\x00" * 32,
            )

    def test_direct_requires_init_addr(self):
        with pytest.raises(ValueError, match="init_addr_le"):
            CommandBuilder.ble_adv_legacy(
                pdu_type=0x1, adv_addr_le=b"\x06\x05\x04\x03\x02\x01",
            )


class TestRadioAdvertiseMethodsViaFakeSerial:
    def test_advertise_ind_sends_correct_command(self):
        radio, fake = _radio_with_fake_serial()
        fake.queue_response(Response.ACK, seq=0x10, payload=b"")
        radio._last_seq = 0x0F  # next will be 0x10
        radio.advertise_ind(payload=b"\x02\x01\x06", target_addr="DE:AD:BE:EF:CA:FE", count=5)
        frames = fake.written_frames()
        assert len(frames) >= 1
        cmd_id, _, p = frames[0]
        assert cmd_id == Command.BLE_ADV_LEGACY
        assert p[0] == 0x0  # ADV_IND

    # similar for advertise_direct, advertise_scan_ind
```

Total: ~12-15 tests; suite ≥ 600 pass.

### Smoke V1.b — `python/examples/smoke_f21_advertise.py`

Patrón derivado de `smoke_f17_emulation.py`:

```
Per personality (3 PDU types):
    Board #1: set_phy(BLE_1M, 37) + start_rx
    Board #2: getattr(tx, method)(...) count=20
    Board #1: read_packets(timeout=3.0) → for each pkt, parse_ll_pdu
    matched = sum(1 for parsed if parsed.kind == expected and adv_addr_le in pkt.data)
    assert matched >= 10

Bonus criterio 2 (SCAN_RSP):
    Para advertise_ind y advertise_scan_ind:
        Board #1: scan_ble_active(duration=5)
        Board #2: advertise_ind(payload=..., scan_resp_data=b"FERAL_SCAN_RSP", count=50)
        results = scan_ble_active output
        matched_scan_rsp = any(r.scan_response_data and b"FERAL_SCAN_RSP" in r.scan_response_data for r in results)
        assert matched_scan_rsp
```

Total runtime: ~3-4 min.

### Demo lab — `python/examples/lab/demo_advertise_connectable.py`

argparse + loop emisión hasta Ctrl-C. Útil para nRF Connect manual checkpoint
(opcional — no parte de smoke automatizado).

## Risks and mitigations

| Risk | Impact | Mitigation |
|------|--------|-----------|
| TI symbols (`Ble5_0_cmdBleAdv` etc) no expuestos en SmartRF config | FW build fail | Verify at Bundle 1 start; agregar SysConfig regeneration o hand-edit struct definitions per `project_syscfg_handedited` |
| TI cmd param structs distintos per command (cmdBleAdvDir tiene pPeerAddress, no pScanRspData) | Crash si polymorfismo erróneo | Tres branches separadas en `RadioIF_transmitBleAdvLegacy` — cada una usa el struct correcto. Sin polymorfismo |
| CONNECT_IND sin F20 → cuelga RF | Próximo TX falla | TI command status `BLE_DONE_CONNECT` → loop break + return. Verified pattern (similar a F8a INITIATOR break-on-state) |
| Firmware bloquea en RF_runCmd loop por count*interval | Python timeout | Firmware ACK INMEDIATO antes del loop; loop async hace TX. Total RF time = count * interval (max ~60s para count=20 + interval=10ms = 200ms — chico). Python timeout 5s suficiente |
| `_ll_parser.LLPduKind` no incluye ADV_IND/DIRECT/SCAN_IND | Smoke V1.b no clasifica | Verify at Bundle 4 start; agregar al enum + parser switch como fix paralelo |
| Single channel per call (no hopping) | User confused | Documented in docstring; user llama 3× para hop 37/38/39 |
| Address random bit ambigüedad (random static vs random non-resolvable) | nRF Connect categoría incorrecta | TI command setea TxAdd según param; default = 1 (random). Documentado |
| Pre-existing CMD_TX_RAW path para ADV_NONCONN_IND | Regression risk si funciones se cruzan | NO se modifica `RadioIF_transmitBleAdvRaw`. Funciones paralelas. Smoke F11 attacks regression check |
| TI BLE legacy adv = 1M only | F21 no soporta 2M / Coded | Documentado out-of-scope. Legacy adv siempre es 1M per BT spec |
| ADV_DIRECT_IND high-duty 3.75ms satura RX | RX pierde packets | Default 'low' (10ms); 'high' opcional con threshold ≥5/20 |
| Pre-commit black auto-format de wire-format helpers | Reformat | Aceptar reformat |

## Acceptance criteria

- ✅ Firmware: `CMD_BLE_ADV_LEGACY` (0x52) en protocol.h
- ✅ Firmware: `RadioIF_transmitBleAdvLegacy` dispatch a 3 TI BLE legacy commands
- ✅ Python: `Command.BLE_ADV_LEGACY` en enums.py
- ✅ Python: `CommandBuilder.ble_adv_legacy` con validation
- ✅ Python: `Radio.advertise_ind / advertise_direct / advertise_scan_ind` métodos
- ✅ Tests unitarios ≥ 12 nuevos; suite total ≥ 600 pass
- ✅ Smoke V1.b 3/3 PDU types clasificados correctamente (≥10/20 cada uno)
- ✅ Bonus criterio 2: SCAN_RSP detectado por F12 active scanner (≥1/5 results match)
- ✅ Pre-commit clean en cada commit
- ✅ `cmake --build firmware/cc1352/build -j2` clean (no warnings nuevos)
- ✅ Smoke F11 attacks pre-existente sigue pasando (no regression)
- ✅ Tag `v2.0-f21` en HEAD final
- ✅ Memory entry `project_f21_done.md`
- ✅ FF merge a `main`

## Out of scope

- F20 — peripheral role + GATT server (CONNECT_IND completo). Fase separada
- nRF Connect manual checkpoint (V2). Disponible vía demo_advertise_connectable.py pero opcional
- BLE 2M / Coded extended advertising (TI command no es legacy)
- Channel hopping interno per call (user envuelve)
- Filtro `advFilterPolicy` configurable (default = allow any scanner/initiator)
- ADV_NONCONN_IND vía este path (ya existe vía CMD_TX_RAW + adv_spoof — no se cambia)
- F17 personalities migration al nuevo path (sigue usando ADV_NONCONN_IND, opcional F17.b)
- Continuous mode (count=0 = permanente). Fuera de scope vuelta 1; si user necesita, envolver count=N en Python loop
