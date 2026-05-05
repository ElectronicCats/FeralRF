# F20.a.1 — BLE Peripheral + GATT server (Read only)

**Date:** 2026-05-04
**Branch (target):** `feature/f20a1-peripheral-read` cut from `main` HEAD=`ceeb5c7`
**Tag (target):** `v2.0-f20.a.1-partial` (la `.a.2` agrega Write+Notify, `.a` consolida; `.b` cubre operations avanzadas — Read Blob/Multiple, Indicate, dynamic GATT table)
**Source:** Master plan v2.0 §F20.

## Goal

Primer paso del peripheral role: aceptar CONNECT_IND post-F21 advertise, transicionar a CMD_BLE5_SLAVE, exponer una GATT table estática (GAP + custom service) y servir ATT_READ_REQ con respuestas correctas. NO Write, NO Notify, NO operations avanzadas — esos son A3.2 / A3.b.

Cierra parcialmente el criterio del spec §F20: "phone descubre services + Read funciona". Write y Notify quedan para sesión siguiente.

## Scope decisions (brainstorm 2026-05-04)

- **A3 → split en A3.1 (Read) + A3.2 (Write+Notify):** spec dice ~1500 LOC firmware, F8A central tomó 5 sesiones, conviene incrementar coverage por sub-vuelta.
- **H2 — handoff dos comandos:** `CMD_GATT_SERVE_TABLE` (0x53) toggle peripheral_active flag + reuse F21 `CMD_BLE_ADV_LEGACY` que ahora detecta CONNECT_IND y transitions a SLAVE si flag activo.
- **T2 — tabla estática:** GAP service (0x1800) + Device Name (0x2A00 = "FERAL_GATT") + Custom service (0xFFE0) + Test Read char (0xFFE1 = "HELLO_FERAL"). 6 handles total.
- **V3 — V1 smoke automatable + V2 opcional:** V1 = 2-board own GATT client smoke (required gate); V2 = nRF Connect manual (opcional, retroactive close pattern).

## Bundle layout

| Bundle | Cambios | Commits |
|--------|---------|---------|
| 1 — Firmware skeleton: protocol + cmdBle5Slave struct + RadioIF wrapper + handler + ATT server stub | `protocol.h`, `smartrf_ble5_0.c`, `radio_if.h`, `radio_if.c`, `command_processor.c`, `att_server.{c,h}` (new), `gatt_table.{c,h}` (new) | 1 |
| 2 — Firmware ATT Read paths + L2CAP RX dispatch | `att_server.c` (Read handlers), `ble_conn_pdu.c` (or new dispatch in att_server) | 1 |
| 3 — Python API + unit tests | `enums.py`, `commands.py`, `radio.py`, `test_radio_serve_gatt.py` (new) | 1-2 |
| 4 — Smoke V1 + demo lab | `smoke_f20a1_peripheral.py`, `demo_gatt_server.py` | 1 |
| Final | Tag + memory + FF | — |

Total: 4-6 commits + 1 tag.

## Architecture detalle

### Static GATT table T2 — `gatt_table.{c,h}`

```c
typedef enum {
    ATTR_PRIMARY_SERVICE = 0x2800,
    ATTR_CHARACTERISTIC  = 0x2803,
} GattAttrType;

typedef struct {
    uint16_t handle;
    uint16_t type;       /* ATTR_* enum or specific UUID16 */
    uint8_t  perms;      /* 0x01=Read (only A3.1; 0x02 Write, 0x10 Notify in A3.2) */
    uint8_t  value_len;
    const uint8_t *value;
} Attribute;

extern const Attribute g_gatt_table[6];
extern const size_t g_gatt_table_size;

const Attribute *GattTable_findByHandle(uint16_t handle);
```

T2 layout (bytes per attribute):

| Handle | Type | Value | Description |
|--------|------|-------|-------------|
| 0x0001 | 0x2800 (PRIMARY_SERVICE) | `00 18` (0x1800 LE) | GAP Primary Service |
| 0x0002 | 0x2803 (CHARACTERISTIC) | `02 03 00 00 2A` (Read prop, val_handle=3, UUID 0x2A00) | Device Name char declaration |
| 0x0003 | 0x2A00 (Device Name UUID) | `46 45 52 41 4C 5F 47 41 54 54` ("FERAL_GATT") | Device Name value |
| 0x0004 | 0x2800 (PRIMARY_SERVICE) | `E0 FF` (0xFFE0 LE) | Custom Primary Service |
| 0x0005 | 0x2803 (CHARACTERISTIC) | `02 06 00 E1 FF` (Read prop, val_handle=6, UUID 0xFFE1) | Test Read char declaration |
| 0x0006 | 0xFFE1 (Test Read UUID) | `48 45 4C 4C 4F 5F 46 45 52 41 4C` ("HELLO_FERAL") | Test Read value |

### ATT server skeleton — `att_server.{c,h}`

Dispatch por opcode incoming:

```c
void AttServer_handleRequest(const uint8_t *pdu, uint8_t pdu_len);
```

Opcodes A3.1:
- `0x06 ATT_FIND_BY_TYPE_VALUE_REQ` → discover primary services by UUID
- `0x08 ATT_READ_BY_TYPE_REQ` → discover chars (type=0x2803)
- `0x0A ATT_READ_REQ` → leer valor por handle
- `0x10 ATT_READ_BY_GROUP_TYPE_REQ` → discover primary services (group=0x2800)

Otros opcodes incoming:
- `0x12 ATT_WRITE_REQ`, `0x52 ATT_WRITE_CMD`, `0x0C ATT_READ_BLOB_REQ`, `0x0E ATT_READ_MULTIPLE_REQ`, `0x04 ATT_FIND_INFORMATION_REQ`, `0x02 ATT_EXCHANGE_MTU_REQ` → `ATT_ERROR_RSP (0x01)` con error code `0x06 Request Not Supported`

Cada handler construye RSP en buffer ≤ 23 bytes (ATT_DEFAULT_MTU), llama `AttServer_txEnqueue(rsp_pdu, rsp_len)` que envuelve en L2CAP frame (CID 0x0004, len, cid, payload) y enqueue en RF tx queue para próximo connection event.

### L2CAP RX path

Reuse de `ble_conn_pdu.c` parser (existing F8A). Para slave:
- LL data PDU header: LLID = `1` (DATA_CONT) o `2` (DATA_START)
- A3.1 asume LLID=2 con frame completo en single PDU (MTU=23, no reassembly)
- L2CAP header: 4 bytes = len(2 LE) + cid(2 LE)
- Si `cid == 0x0004` (ATT): pasar payload to `AttServer_handleRequest()`
- Otros CIDs (0x0005 LE Signaling, 0x0006 Security): A3.1 ignora silenciosamente

### Connection handoff F21 → F20

Modificación a `CMD_BLE_ADV_LEGACY` handler post-F21:

```c
case CMD_BLE_ADV_LEGACY: {
    /* ... existing F21 parsing ... */
    send_ack(seq);

    bool ok = RadioIF_transmitBleAdvLegacy(...);

    /* F20.a.1: if peripheral mode active and CONNECT_IND received,
     * extract conn params from RX queue (last PDU type=0x5) and transition
     * to slave state. Slave loop blocks until disconnect. */
    if (ok && s_peripheral_active && /* CMD_BLE_ADV exited with BLE_DONE_CONNECT */) {
        BlePeripheralConnParams params;
        if (extract_connect_ind_from_rx_queue(&params)) {
            (void)RadioIF_runBlePeripheral(&params);
        }
        /* Reset peripheral_active so next advertise_ind doesn't auto-handoff */
        s_peripheral_active = false;
    }
    return;
}
```

`extract_connect_ind_from_rx_queue()`: scan data queue para PDU type=0x5 (CONNECT_IND), extract:
- access_addr (4 LE) at offset 12 of CONNECT_IND PDU body
- crc_init (3 LE)
- win_size (1)
- win_offset (2 LE)
- interval (2 LE) — 1.25ms units
- latency (2 LE)
- timeout (2 LE) — 10ms units
- ch_map (5)
- hop_chsel (1) — bits 4:0 = hop_increment

### CMD_BLE5_SLAVE — `RadioIF_runBlePeripheral`

Espejo de F8A central `RadioIF_runBleCentral`:

```c
typedef struct {
    uint32_t access_addr;
    uint32_t crc_init;       /* 24-bit, low 3 bytes */
    uint16_t win_size_us;
    uint16_t win_offset_us;
    uint16_t interval_us;    /* 1.25ms units expanded */
    uint16_t latency;
    uint16_t timeout_ms;
    uint8_t ch_map[5];
    uint8_t hop_increment;
} BlePeripheralConnParams;

bool RadioIF_runBlePeripheral(const BlePeripheralConnParams *params);
```

Internamente: setup `Ble5_0_cmdBle5Slave` per-event, anchor calc, hop sequence per BT spec §4.5.8 channel selection algorithm #1 (same as F8A central). Loop hasta:
- LL_TERMINATE_IND received (parse from RX queue, reason byte → emit RSP_DISCONNECTED)
- Supervision timeout (no RX for `timeout_ms` ms — emit RSP_DISCONNECTED reason 0x08)

### `Ble5_0_cmdBle5Slave` struct (smartrf_ble5_0.c)

Espejo de `Ble5_0_cmdBle5Master` ya existente (línea 640). Mismo pattern, distinto commandNo (CMD_BLE5_SLAVE = 0x1823).

## Wire protocol

### `CMD_GATT_SERVE_TABLE` (0x53)

A3.1: empty payload (flag toggle).
A3.b: payload = `[entry_count:1][entries: variable]` para dinámico.

Response: `RSP_ACK` simple. Sin echo de state.

Estado firmware: `static bool s_peripheral_active = false;` toggle on receipt.

## Python API

### `enums.py`

```python
class Command(IntEnum):
    ...
    GATT_SERVE_TABLE = 0x53
```

### `commands.py CommandBuilder`

```python
@staticmethod
def gatt_serve_table() -> bytes:
    """Build CMD_GATT_SERVE_TABLE payload (F20.a.1: empty/flag toggle)."""
    return b""
```

### `radio.py Radio.serve_gatt`

```python
def serve_gatt(self, table: Optional[object] = None) -> None:
    """F20.a.1 — toggle peripheral mode on. Subsequent advertise_ind() will
    auto-handoff to GATT server slave on CONNECT_IND.

    A3.1: `table` arg ignored (warns) — firmware uses hardcoded T2 table
    (GAP service "FERAL_GATT" + custom service "HELLO_FERAL").
    A3.b will accept dynamic table here.
    """
    if table is not None:
        import warnings
        warnings.warn("table arg ignored in F20.a.1 — firmware uses hardcoded T2 table; "
                      "dynamic table coming in F20.b", stacklevel=2)
    cmd_payload = CommandBuilder.gatt_serve_table()
    self._send_command(Command.GATT_SERVE_TABLE, cmd_payload)
    self._read_response(timeout=2.0, expected={Response.ACK, Response.ERROR})
```

## Tests

### Unit — `python/tests/test_radio_serve_gatt.py`

```python
class TestServeGattPayload:
    def test_empty_payload(self):
        assert CommandBuilder.gatt_serve_table() == b""

class TestRadioServeGatt:
    def test_dispatch_correct_command(self):
        radio, fake = _radio_with_fake_serial()
        fake.queue_response(Response.ACK, seq=radio._seq)
        radio.serve_gatt()
        frames = fake.written_frames()
        assert len(frames) == 1
        cmd_id, _, payload = frames[0]
        assert cmd_id == Command.GATT_SERVE_TABLE
        assert payload == b""

    def test_warns_when_table_arg_passed(self):
        radio, fake = _radio_with_fake_serial()
        fake.queue_response(Response.ACK, seq=radio._seq)
        with pytest.warns(UserWarning, match="table arg ignored"):
            radio.serve_gatt(table=[("dummy",)])
```

Total: ~6-8 tests.

### Smoke V1 — `python/examples/smoke_f20a1_peripheral.py`

2-board: peripheral on board #1, central on board #2.

```
Board #1 (peripheral, run first):
    radio.init()
    radio.serve_gatt()
    radio.advertise_ind(target_addr="DE:AD:BE:EF:CA:FE", count=200, interval_us=10000)
    # Firmware blocks; CONNECT_IND triggers handoff to slave loop.

Board #2 (central, started after 0.5s):
    radio.init()
    radio.reset_device()  # F8 GATT client bug workaround
    radio.init()
    addr_le = bytes.fromhex("FECAEFBEADDE")
    result = radio.ble_connect(addr_le, addr_type=1, timeout=10.0)
    assert result.is_ok
    services = radio.gatt_discover(timeout=10.0)
    name_value = radio.gatt_read(handle=3, timeout=5.0)
    test_value = radio.gatt_read(handle=6, timeout=5.0)
    radio.ble_disconnect(timeout=5.0)

Asserciones:
    len(services.services) >= 2
    len(services.characteristics) >= 2
    name_value == b"FERAL_GATT"
    test_value == b"HELLO_FERAL"
```

Pass = 4/4. Total runtime ~30s.

Demo lab `demo_gatt_server.py`: single-board peripheral hasta Ctrl-C, útil para nRF Connect V2 manual.

## Risks and mitigations

| Risk | Impact | Mitigation |
|------|--------|-----------|
| CONNECT_IND extraction de RX queue post-cmdBleAdv — TI undocumented path | Sin conn params, slave no inicia | Bundle 1: dump RX queue post-CONNECT_IND, validar PDU type=0x5 + parse. Fallback `CMD_BLE5_ADV_AUX` (5.0) que sí expone params en pOutput |
| `Ble5_0_cmdBle5Slave` no en SmartRF config | Build fail | Verify Bundle 1; agregar struct definition siguiendo pattern F21 (cmdBle5Master template) |
| Slave timing — anchor/hop/event_count drift | NOSYNC equivalente F8A central | F8A tomó 5 sesiones. Reusar **exact pattern validado** en F8A central (anchor calc, hop seq, event_count++). NO inventar — copiar de RadioIF_runBleCentral mirror |
| ATT response timing dentro de connection event | RSP llega tarde, peer timeout | TI cmdBle5Slave maneja TX queue automático — encolar ATT_RSP en s_rf_tx_queue antes del event y CPE TXea cuando timing permite |
| L2CAP RX path no extrae ATT correctamente | Server no recibe requests | Reuse ble_conn_pdu.c parser; extraer ATT bytes después del L2CAP header (4 bytes). Solo CID 0x0004 en A3.1 |
| LL_TERMINATE_IND RX no detecta disconnect | Slave loop cuelga | Detect PDU type=0x06 LL Control + opcode 0x02 LL_TERMINATE_IND. Supervision timeout fallback si peer no envía terminate |
| project_gatt_attclient_bug afecta V1 smoke | Smoke falla 2nd run | reset_device() ambos boards al inicio |
| Pairing/encryption requested por phone (V2) | Phone se desconecta | A3.1 perms=Read sin auth — phone no debería pedir bonding |
| Single connection — no resync si peer reconecta | A3.1 acepta 1 conn, después pasivo | Documentar; reconnect requiere re-llamar serve_gatt + advertise_ind |
| MTU=23 default — Read responses limitadas | Char values >20 bytes truncados | T2 values ≤11 bytes (cabe en 1 ATT PDU). MTU exchange (>23) es F20.b |
| `s_peripheral_active` reset post-disconnect | User confused (next advertise_ind no auto-handoff) | Documented: must re-call serve_gatt() después de cada disconnect |

## Acceptance criteria

- ✅ Firmware: `CMD_GATT_SERVE_TABLE` (0x53) + struct `cmdBle5Slave` + ATT server skeleton + Read path + L2CAP RX dispatch
- ✅ Static GATT table T2 hardcoded (6 handles)
- ✅ F21 `CMD_BLE_ADV_LEGACY` modificado para handoff a SLAVE on CONNECT_IND si `s_peripheral_active`
- ✅ Python: `Command.GATT_SERVE_TABLE`, `CommandBuilder.gatt_serve_table()`, `Radio.serve_gatt()`
- ✅ Tests unitarios ≥ 6 nuevos
- ✅ Smoke V1 4/4 asserciones (2 boards FeralRF cross-validation)
- ✅ Pre-commit clean en cada commit
- ✅ `cmake --build firmware/cc1352/build -j2` clean
- ✅ Smokes F11/F21/F8 pre-existentes siguen pasando (no regression)
- ✅ Tag `v2.0-f20.a.1-partial`
- ✅ Memory entry `project_f20a1_done.md`
- ✅ FF merge a `main`

**Bonus opcional (V2):** nRF Connect en phone descubre "FERAL_GATT" + Read works.

## Out of scope

- F20.a.2: Write Req/Rsp + HVN (Notify) — sesión siguiente
- F20.b: Read Blob, Read Multiple, Indicate, dynamic GATT table desde host, MTU exchange server-side
- F20.c: pairing/encryption/bonding (out of scope all v2.0 — security features deferred)
- Multiple concurrent connections
- Reconnect lifecycle automático
- L2CAP reassembly server-side
- ATT_FIND_INFORMATION_REQ (descriptor discovery — no descriptors en T2)
- nRF Connect manual checkpoint requerido (es V2 opcional)
- Continuous advertising mientras conectado
- Advertising filter / RPA
