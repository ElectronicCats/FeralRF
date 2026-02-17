# FeralRF Protocol (Host <-> Firmware)

Estado del contrato implementado en firmware `1.0.0` (CC1352), actualizado a `2026-02-17`.

## 1) Framing y transporte

- Transporte físico actual: UART (`921600`, 8N1).
- Delimitador de frame: `0x00` (cada frame COBS termina en `0x00`).
- Formato antes de COBS:

```text
[CMD_ID:1][SEQ:1][LEN:2 LE][PAYLOAD:0..255][CRC16:2 LE]
```

- `CRC16`: CRC-16-CCITT (polinomio `0x1021`, inicial `0xFFFF`) sobre `CMD_ID + SEQ + LEN + PAYLOAD`.
- Endianness de campos multi-byte: little-endian.

Límites:
- `PROTOCOL_MAX_PAYLOAD = 255`.
- `PROTOCOL_MAX_FRAME = 261` bytes (sin COBS, incluyendo header+crc).

## 2) IDs y capacidades

### PHY IDs

- `0`: `BLE_1M`
- `1`: `BLE_2M`
- `2`: `BLE_CODED_S8`
- `3`: `BLE_CODED_S2`
- `4`: `IEEE_802_15_4`
- `5`: `SUB_1GHZ_868`
- `6`: `SUB_1GHZ_915`
- `7`: `PROPRIETARY_GFSK`

### Capabilities (`GET_INFO.payload[3]`)

- `0x01` (`FW_CAPABILITY_RX_STATS`): habilita métricas RX (`GET_STATS` base).
- `0x02` (`FW_CAPABILITY_LL_PDU_META`): habilita metadata LL en `RSP_RX_PACKET`.
- `0x04` (`FW_CAPABILITY_LL_STATS_EXT`): habilita contadores LL extendidos en `RSP_STATS`.

En `fw 1.0.0` actual: `capabilities = 0x07`.

## 3) Comandos implementados

Comandos actualmente soportados por firmware:

- `0x01` `RADIO_INIT`
- `0x02` `SET_CHANNEL`
- `0x03` `SET_POWER`
- `0x04` `SET_PHY`
- `0x05` `GET_INFO`
- `0x06` `GET_STATS`
- `0x10` `RX_START`
- `0x11` `RX_STOP`

Comandos definidos en `python/feralrf/enums.py` pero no implementados en firmware responden `RSP_ERROR(0x81)` con `ERR_INVALID_CMD(0x01)`.

## 4) Payloads de comandos

### `RADIO_INIT (0x01)`
- Request payload: vacío.
- Response: `ACK` o `ERROR`.
- Efecto: reinicia estado de sesión RX y resetea métricas/estadísticas LL.

### `SET_CHANNEL (0x02)`
- Request payload: `channel_u8`.
- Response: `ACK` o `ERROR`.

### `SET_POWER (0x03)`
- Request payload: `power_i8` (enviado como byte).
- Response: `ACK` o `ERROR`.

### `SET_PHY (0x04)`
- Request payload soportado por firmware:
  - corto: `phy_u8` (1 byte)
  - extendido: `phy_u8 + channel_u16 + frequency_hz_u32` (7 bytes)
- `python/feralrf` usa formato extendido por defecto.
- Response: `ACK` o `ERROR`.

### `GET_INFO (0x05)`
- Request payload: vacío.
- Response: `RSP_INFO (0x94)`.

### `GET_STATS (0x06)`
- Request payload: vacío.
- Response: `RSP_STATS (0x93)`.

### `RX_START (0x10)` / `RX_STOP (0x11)`
- Request payload: vacío.
- Response: `ACK` o `ERROR`.

## 5) Respuestas

### `RSP_ACK (0x80)`
- Payload: vacío.

### `RSP_ERROR (0x81)`
- Payload:
  - `error_code_u8`
- Códigos actuales:
  - `0x01`: comando inválido (`ERR_INVALID_CMD`)
  - `0x02`: payload inválido (`ERR_INVALID_PAYLOAD`)
  - `0x03`: frame inválido (`ERR_INVALID_FRAME`, COBS/LEN/CRC)
  - `0x04`: frame demasiado largo en RX UART (`ERR_FRAME_TOO_LONG`)

Nota: en errores de parseo temprano, `SEQ` puede llegar como `0`.

### `RSP_INFO (0x94)`
- Payload (12 bytes):
  - `[0]` `fw_major_u8`
  - `[1]` `fw_minor_u8`
  - `[2]` `fw_patch_u8`
  - `[3]` `capabilities_u8`
  - `[4..11]` `serial_ascii_8bytes`

### `RSP_STATS (0x93)`
- Payload base (16 bytes):
  - `[0..3]` `rx_ok_u32`
  - `[4..7]` `rx_crc_err_u32`
  - `[8..11]` `rx_drop_u32`
  - `[12..15]` `rx_overflow_u32`
- Extensión LL (20 bytes adicionales, total 36):
  - `[16..19]` `ll_kind_unknown_u32`
  - `[20..23]` `ll_kind_adv_u32`
  - `[24..27]` `ll_kind_scan_u32`
  - `[28..31]` `ll_kind_connect_u32`
  - `[32..35]` `ll_kind_data_u32`

Compatibilidad:
- Clientes deben tolerar `16` o `36` bytes.
- Con `capability 0x04`, el host puede interpretar extensión LL.

### `RSP_RX_PACKET (0x90)` (stream asíncrono)

Payload:
- `[0..7]` `timestamp_us_u64`
- `[8]` `channel_u8`
- `[9]` `rssi_i8`
- `[10]` `lqi_u8`
- `[11]` `crc_ok_u8` (`0`/`1`)
- `[12]` `data_len_u8`
- `[13 .. 13+data_len-1]` `data`
- `+ [ll_pdu_kind_u8, ll_pdu_type_u8, ll_pdu_flags_u8]` (metadata LL)

`data_len` emitido máximo actual: `239` bytes (clamp por límite de payload de protocolo).

`ll_pdu_kind`:
- `0` `UNKNOWN`
- `1` `ADV`
- `2` `SCAN`
- `3` `CONNECT`
- `4` `DATA`

`ll_pdu_flags` bits:
- `0x01` `PRIMARY_ADV_CH`
- `0x02` `DATA_CH`
- `0x04` `EXT_ADV`
- `0x08` `RESERVED`

## 6) Semántica de `SEQ`

- En request/response de control (`ACK`, `ERROR`, `INFO`, `STATS`), firmware responde con el mismo `SEQ` del comando.
- `RSP_RX_PACKET` usa un contador interno independiente de `SEQ` (no correlacionado con requests).

## 7) Reglas de compatibilidad para clientes

- No asumir que todos los `Command` de `python/feralrf/enums.py` están implementados por firmware.
- Al parsear `RSP_STATS`, aceptar formato base (16) y extendido (36).
- Al parsear `RSP_RX_PACKET`, validar primero longitud base y luego LL metadata según capabilities y longitud real.
- Tratar `RSP_ERROR(0x81)` como respuesta válida de protocolo (no como frame corrupto).
