# F12 — BLE Scanner Activo (TI-RTOS) — Design Spec

> **Date:** 2026-04-29
> **Branch:** `feature/f12-ble-scanner-active`
> **Phase:** F12 (Bloque C) of plan-v2
> **Author:** Sabas + Claude (brainstorming session)

---

## 1. Goal & scope

Cierre F12: exponer en Python un `radio.scan_ble_active(duration, channels)` que envíe `SCAN_REQ` automáticos y capture `SCAN_RSP`, mergeando ambos por MAC en un `BleScanResult` decoded.

**Audit 2026-04-29:** firmware ya está al ~95% para esto. La feature core ya funciona en TI-RTOS:

- `Ble5_0_cmdBle5Scanner` configurado en `firmware/cc1352/src/smartrf_ble5_0.c`
- Flag `s_ble_active_scan` + `RadioIF_setActiveScan()` en `radio_if.c`
- `runFsAndPostRx()` selecciona Scanner vs GenericRx según flag (línea 945)
- `RadioIF_getScannerStats()` expone `nTxReq/nRxAdvOk/nRxRspOk`
- `CMD_SET_BLE_SCAN_MODE (0x0B)` handler en `command_processor.c`
- Python `radio.set_ble_scan_mode(active=True)` ya existente
- `LLManager_processRxPacket` ya clasifica PDU type 0x04 → `ll_pdu_kind=LL_PDU_KIND_SCAN`, `ll_pdu_type=0x04` (líneas 27, 75-78 de `ll_manager.c`)
- Python parser ya extrae `pkt.ll_pdu_type` en `read_packets()`

**Por lo tanto F12 es Python-only** — sin cambios firmware. Reusamos `pkt.ll_pdu_type == 0x04` como discriminador SCAN_RSP en vez de añadir un flag dedicado a la wire format.

### Out of scope (F12)

- Soporte BLE5 extended advertising (AUX_PTR following) en active scan — feature válida del chip pero compleja, candidate para v2.1.
- Active scan en Coded PHY S2/S8 — el firmware lo soporta a nivel `phyMode` pero no se valida en F12 (default `PHY.BLE_1M`; el parámetro `phy` está expuesto para que avanzados lo prueben).
- BLE Peripheral / GATT server — F20.
- AD types 0x20 (Service Data 32-bit UUID) y 0x21 (Service Data 128-bit UUID) — raros en práctica; añadir cuando un device los use. 0x16 (16-bit) cubre >95% de casos.

## 2. API

### 2.1 `BleScanResult` dataclass

Archivo nuevo: `python/feralrf/_ble_scan.py`

```python
from dataclasses import dataclass, field, asdict
from typing import Optional

@dataclass
class BleScanResult:
    mac: str                                      # "AA:BB:CC:DD:EE:FF" display order
    addr_type: str                                # "public" | "random_static" | "random_resolvable" | "random_non_resolvable"
    name: Optional[str] = None                    # AD 0x09 (Complete) preferred over 0x08 (Shortened)
    rssi_max: int = -128
    rssi_min: int = 0
    rssi_avg: float = 0.0
    adv_count: int = 0                            # ADV_IND / ADV_NONCONN_IND / ADV_SCAN_IND / ADV_DIRECT_IND
    scan_rsp_count: int = 0                       # SCAN_RSP (PDU type 0x04)
    flags: Optional[int] = None                   # AD 0x01
    uuids_16bit: list[str] = field(default_factory=list)         # AD 0x02 + 0x03 — uppercase hex "FE2C"
    uuids_128bit: list[str] = field(default_factory=list)        # AD 0x06 + 0x07 — full UUID "0000180a-0000-1000-8000-00805f9b34fb"
    services_uuid16_data: dict[str, bytes] = field(default_factory=dict)   # AD 0x16
    manufacturer_data: dict[int, bytes] = field(default_factory=dict)      # AD 0xFF — company_id → bytes
    tx_power: Optional[int] = None                # AD 0x0A (signed int8)
    appearance: Optional[int] = None              # AD 0x19 (uint16)
    raw_advs: list[bytes] = field(default_factory=list)
    raw_scan_rsps: list[bytes] = field(default_factory=list)

    def to_dict(self) -> dict:
        """Return JSON-serializable dict (bytes → hex strings)."""
        d = asdict(self)
        d["raw_advs"] = [b.hex() for b in self.raw_advs]
        d["raw_scan_rsps"] = [b.hex() for b in self.raw_scan_rsps]
        d["services_uuid16_data"] = {k: v.hex() for k, v in self.services_uuid16_data.items()}
        d["manufacturer_data"] = {k: v.hex() for k, v in self.manufacturer_data.items()}
        return d
```

### 2.2 AD parser

Función pura `parse_ad_structures(payload: bytes) -> dict[str, object]` que recorre `[len][type][value(len-1)]...` y rellena los campos de `BleScanResult`.

| AD type | Significado | Acción |
|---|---|---|
| 0x01 | Flags | `flags = value[0]` |
| 0x02, 0x03 | Incomplete/Complete 16-bit UUIDs | extend `uuids_16bit` con UUIDs little-endian |
| 0x06, 0x07 | Incomplete/Complete 128-bit UUIDs | extend `uuids_128bit` con UUIDs little-endian, formato canónico |
| 0x08 | Shortened Local Name | set `name = value.decode('utf-8', errors='replace')` si `name is None` |
| 0x09 | Complete Local Name | set `name = ...` (preferred sobre 0x08) |
| 0x0A | TX Power Level | `tx_power = signed int8(value[0])` |
| 0x16 | Service Data 16-bit UUID | `services_uuid16_data[uuid_str] = value[2:]` |
| 0x19 | Appearance | `appearance = uint16_le(value[:2])` |
| 0xFF | Manufacturer Specific Data | `manufacturer_data[company_id] = value[2:]` |

**Manejo de errores en parser:**
- `len == 0` → skip avanzando 1 byte.
- `len > remaining_bytes` → break loop (payload truncado).
- AD type desconocido → skip silently, avanzar `len+1`.
- UTF-8 decode error en name → usa `errors='replace'`.
- Nunca raise; siempre retorna dict (puede estar vacío).

### 2.3 PDU layout extraction

`pkt.data` para ADV_IND/SCAN_RSP/etc:

```
[PDU header (2B)] [AdvA (6B little-endian)] [AdvData (variable)]
       │
       └─ byte 0: PDU type (low 4 bits) | RFU (4 bits)
          byte 1: length (low 6 bits) | RxAdd | TxAdd

PDU header byte 1 bit 6 (TxAdd):
  - 0 → public address
  - 1 → random address (sub-classify by AdvA byte 5 high 2 bits):
        0b00 → random_non_resolvable
        0b01 → random_resolvable
        0b11 → random_static
```

`mac` se construye reversed-display: AdvA `[FE CA EF BE AD DE]` little-endian → display `"DE:AD:BE:EF:CA:FE"`.

### 2.4 `scan_ble_active()` method

Archivo: `python/feralrf/radio.py` — agregar método y export en `__all__` / class API list.

```python
from typing import Sequence, Union
from feralrf._ble_scan import BleScanResult, parse_ad_structures

def scan_ble_active(
    self,
    duration: float,
    channels: Union[int, Sequence[int]] = (37, 38, 39),
    phy: PHY = PHY.BLE_1M,
) -> dict[str, BleScanResult]:
    """Active BLE scan: send SCAN_REQ, capture SCAN_RSP, merge per MAC.

    Args:
        duration: seconds to listen.
        channels: int or sequence of advertising channels (37/38/39).
                  If single channel → adv_hop disabled. If multiple → adv_hop enabled.
        phy: BLE PHY for scanning (default BLE_1M).

    Returns:
        dict keyed by MAC ("AA:BB:CC:DD:EE:FF" display order) of BleScanResult.

    Side effects (saved/restored on exit, even on exception):
        - set_ble_scan_mode (active/passive flag)
        - set_adv_hop (channel hopping flag)
        - PHY/channel state via set_phy
    """
```

**Implementation outline:**

1. Normalize `channels`: int → (int,); sequence → tuple.
2. Snapshot prior state — there are no public getters; we use the cached attrs `self._phy`, `self._channel` already maintained by `set_phy`. For `set_ble_scan_mode` and `set_adv_hop` no getter exists; we always restore to defaults after (passive, hop=False) — documented as such.
3. `set_ble_scan_mode(True)`, `set_adv_hop(len(channels) > 1)`, `set_phy(phy, channel=channels[0])`, `start_rx()`.
4. Iterate `read_packets(timeout=duration)`:
   - Skip `not pkt.crc_ok`.
   - Skip if no LL meta (defensive — Scanner always emits LL meta in current FW).
   - Skip non-BLE-adv packets (`ll_pdu_kind != LL_PDU_KIND_SCAN` and `ll_pdu_kind != LL_PDU_KIND_ADV` — see capabilities map).
   - Extract MAC + addr_type from `pkt.data[2:8]` and PDU header.
   - Lookup or create `BleScanResult` keyed by MAC.
   - If `pkt.ll_pdu_type == 0x04` (SCAN_RSP) → append to `raw_scan_rsps`, `scan_rsp_count += 1`.
   - Else (ADV_*) → append to `raw_advs`, `adv_count += 1`.
   - Update `rssi_min/max/avg` (rolling average).
   - Run `parse_ad_structures(pkt.data[8:])` and merge fields into the result (overlay name preferring 0x09 over 0x08; extend lists; merge dicts).
5. `try/finally`: `stop_rx()`, restore (`set_ble_scan_mode(False)`, `set_adv_hop(False)`, restore prior PHY/channel via `set_phy(prior_phy, prior_channel)`).
6. Return dict.

### 2.5 Cleanup semantics

- Restoration runs in `finally`; raises in cleanup are logged but don't mask the original exception.
- If user had `set_ble_scan_mode(True)` previously, after `scan_ble_active` it's left as `False` — documented limitation (no public getter for prior state). Users who need to keep active scan after the call can re-call `set_ble_scan_mode(True)`.

## 3. Demo — `python/examples/lab/demo_ble_scan_active.py`

Compares passive vs active scan to highlight what active adds. Single positional arg = port (default `/dev/ttyACM0`). Optional `--json out.json` to dump full results.

Flow:

```
[1/3] Passive scan 5 s on ch 37/38/39:
        radio.set_ble_scan_mode(False)
        scan_dict = scan-loop equivalent without SCAN_REQ
        Print table: MAC | name | RSSI_max | UUIDs (16+128 count) | mfg_companies | adv_count

[2/3] Active scan 5 s on ch 37/38/39:
        radio.scan_ble_active(5.0, channels=(37,38,39))
        Print same table + scan_rsp_count column

[3/3] Diff:
        For each MAC seen in BOTH passive and active:
            - name passive vs active → highlight if active completed/changed
            - UUIDs only-in-active → list with ★
            - manufacturer_data only-in-active → list with ★
            - services_uuid16_data only-in-active → list with ★
        Print summary:
            "passive: N devices, K UUIDs total"
            "active:  N' devices, K' UUIDs total (delta: +X UUIDs, +Y mfg, +Z names completed)"
            "F12 closure: PASS — at least 1 device contributed scan_rsp content not in adv"
        OR if no device satisfied criterion:
            "F12 closure: SKIP — no scannable peripheral in range. Bring an ESP32/phone/smart-bulb closer and re-run."

If --json: save full results (passive + active + diff) to file.
```

Target runtime: ~10 s + setup overhead.

## 4. Tests

### 4.1 Unit tests — `python/tests/test_ble_scan.py`

Hardware-free, run in CI via existing `build.yml`.

| # | Test | What it verifies |
|---|---|---|
| 1 | `test_parse_ad_complete_name` | AD 0x09 decodes UTF-8 |
| 2 | `test_parse_ad_shortened_name_replaced_by_complete` | 0x08 then 0x09 → name = complete |
| 3 | `test_parse_ad_uuids_16bit_complete_and_incomplete` | 0x02 + 0x03 both extend `uuids_16bit` |
| 4 | `test_parse_ad_uuids_128bit_canonical_format` | 0x06/0x07 produce `"xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"` |
| 5 | `test_parse_ad_manufacturer_data_company_id` | 0xFF with Apple `0x004C` extracts company + payload |
| 6 | `test_parse_ad_service_data_uuid16` | 0x16 with UUID + variable-length data |
| 7 | `test_parse_ad_appearance_tx_power_flags` | 0x19 / 0x0A / 0x01 single-byte fields |
| 8 | `test_parse_ad_malformed_zero_length_skips` | `len=0` doesn't infinite-loop |
| 9 | `test_parse_ad_malformed_overflow_breaks` | `len > remaining` stops parsing without raise |
| 10 | `test_parse_ad_unknown_type_skipped` | unknown AD type advances correctly |
| 11 | `test_blescanresult_merge_adv_then_scan_rsp` | both raw lists populated, fields merged (name from adv, UUIDs from scan_rsp) |
| 12 | `test_blescanresult_to_dict_json_round_trip` | `asdict()` → `json.dumps()` → no error |
| 13 | `test_addr_type_classification_public` | TxAdd=0 → public |
| 14 | `test_addr_type_classification_random_static` | TxAdd=1 + AdvA[5]&0xC0=0xC0 → random_static |
| 15 | `test_addr_type_classification_random_resolvable` | TxAdd=1 + 0x40 → random_resolvable |
| 16 | `test_addr_type_classification_random_non_resolvable` | TxAdd=1 + 0x00 → random_non_resolvable |
| 17 | `test_rssi_avg_rolling` | 3 packets RSSIs → avg correct |

### 4.2 Hardware smoke — `python/examples/lab/smoke_f12_scan_active.py`

Single-board on `/dev/ttyACM8` (or `--port` arg). No second board needed — uses ambient lab traffic.

```python
result = radio.scan_ble_active(duration=10.0, channels=(37, 38, 39))

assert len(result) >= 3, f"expected ≥3 BLE devices in lab ambient; got {len(result)}"

scan_rsp_devices = [r for r in result.values() if r.scan_rsp_count > 0]
assert len(scan_rsp_devices) >= 1, "expected ≥1 device responding to SCAN_REQ"

closure_devices = [
    r for r in result.values()
    if r.scan_rsp_count > 0
    and r.name
    and (r.uuids_16bit or r.uuids_128bit or r.manufacturer_data)
]
assert len(closure_devices) >= 1, (
    "F12 closure criterion not met: no device with name + UUIDs/mfg + scan_rsp"
)

print(f"[ OK ] F12 wire smoke PASS — {len(result)} devs, "
      f"{len(scan_rsp_devices)} scan_rsps, {len(closure_devices)} closure-eligible")
```

Exit 0 if all asserts pass; 1 otherwise (with stdout reporting which assert failed).

### 4.3 Manual checkpoint humano (deferred)

Scan 5 s active contra:
1. **Móvil** (Android/iPhone) en pairing mode visible.
2. **ESP32** corriendo NimBLE peripheral example (advertise + scan_rsp con services).
3. **Comercial** — earbuds, smart bulb, fitness tracker.

Para cada uno: verificar que `result[mac]` contiene `name` completo y al menos 1 UUID que no estaba en `raw_advs[*]` (sí en `raw_scan_rsps[*]`).

Tag `v2.0-f12` solo después del checkpoint manual completo, igual al patrón F11.

## 5. File layout

```
python/feralrf/_ble_scan.py            (new ~280 LOC — dataclass + parser + PDU layout)
python/feralrf/radio.py                (modify ~+50 LOC — scan_ble_active method)
python/tests/test_ble_scan.py          (new ~250 LOC — 17 unit tests)
python/examples/lab/demo_ble_scan_active.py  (new ~150 LOC — passive/active diff)
python/examples/lab/smoke_f12_scan_active.py (new ~70 LOC — wire smoke for closure)
docs/superpowers/specs/2026-04-29-f12-ble-scanner-active-design.md  (this file)
docs/superpowers/plans/2026-04-29-f12-ble-scanner-active-plan.md    (next: writing-plans output)
```

Estimación: ~800 LOC new + 50 LOC modify, 0 LOC firmware.

## 6. Risks

| # | Riesgo | Mitigación |
|---|--------|------------|
| f12-r1 | Lab ambiente sin scannable peripherals → smoke falla con criterio no cumplido | Reportar claro qué falta; user debe traer peripheral. Documentado en demo y smoke. |
| f12-r2 | UUIDs del scan_rsp idénticos a adv en algunos devices (no aporta info nueva) | Aceptado — el chip funciona; no es bug. Test smoke verifica scan_rsp_count>0, no que aporte UUIDs distintos. |
| f12-r3 | RF Core sigue con bAutoFlushIgnored=1 (cambio reciente F11a colateral) — verificar que Scanner no pierde packets útiles | Ya validado en smoke F11 (5/5 attacks). Default es 1 = filtra paquetes que no decodificó (no útiles). Si algún device usa AccessAddress no estándar se perderían — fuera de scope F12. |
| f12-r4 | seq=0xFF bug — ya fixed en `9e2f5f3`, base branch contiene fix | No-op risk |

## 7. Closure criteria (gate al merge a `feature/ti-rtos-migration`)

- [ ] Unit tests `test_ble_scan.py` 17/17 PASS
- [ ] Full Python suite no regression (excluyendo el pre-existing fail no relacionado)
- [ ] Hardware smoke `smoke_f12_scan_active.py` PASS en board #1 (ACM8)
- [ ] Demo `demo_ble_scan_active.py` runs sin error y reporta diff coherente
- [ ] Pre-commit clean
- [ ] Plan en `docs/superpowers/plans/...` cubierto checkbox por checkbox

Tag `v2.0-f12` solo después de checkpoint manual con 3 peripherals.

## 8. Open questions

Ninguna — todo cerrado en brainstorming 2026-04-29.

---

**Next step:** writing-plans skill → genera plan de implementación step-by-step en `docs/superpowers/plans/2026-04-29-f12-ble-scanner-active-plan.md`.
