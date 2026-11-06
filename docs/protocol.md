# FeralRF Protocol (Host <-> Firmware)

> **Contrato canónico del protocolo wire FeralRF entre el host y el firmware del CC1352.**

La ruta de transporte es: biblioteca/aplicación host → USB CDC Cat-Bridge/RP2040 → UART → firmware FeralRF del CC1352. Cat-Bridge transporta bytes; este documento no presupone que el RP2040 interprete los comandos FeralRF.

Estado del contrato reportado como firmware `1.0.0` sobre CC1352, actualizado inicialmente a `2026-07-20`. `1.0.0` es el valor devuelto por el protocolo en `RSP_INFO`; no identifica de forma única un commit Git, hash binario, artefacto flasheado ni placa física.

Este documento describe el contrato real expuesto hoy por firmware y la API Python actual. Reemplaza el estado MVP antiguo que solo cubria `GET_INFO/SET_PHY/RX_START/RX_STOP`.

Implementaciones principales:

- Host: [`python/feralrf/protocol.py`](../python/feralrf/protocol.py), con IDs y builders en [`python/feralrf/enums.py`](../python/feralrf/enums.py) y [`python/feralrf/commands.py`](../python/feralrf/commands.py).
- Firmware: [`firmware/cc1352/include/protocol.h`](../firmware/cc1352/include/protocol.h), [`firmware/cc1352/src/protocol.c`](../firmware/cc1352/src/protocol.c) y [`firmware/cc1352/src/command_processor.c`](../firmware/cc1352/src/command_processor.c).

Para el flujo por capas consulte [`ARCHITECTURE.md`](ARCHITECTURE.md); para la interfaz pública del host consulte [`PYTHON_API.md`](PYTHON_API.md).

## 1) Framing y transporte

- Transporte fisico actual: UART a `921600`, `8N1`.
- Delimitador de frame: `0x00`.
- Cada frame binario se serializa como:

```text
[CMD_ID:1][SEQ:1][LEN:2 LE][PAYLOAD:0..255][CRC16:2 LE]
```

- Ese bloque se codifica con COBS y se termina con `0x00`.
- `CRC16`: CRC-16-CCITT, polinomio `0x1021`, inicial `0xFFFF`.
- El CRC se calcula sobre `CMD_ID + SEQ + LEN + PAYLOAD`.
- Todos los campos multibyte usan little-endian.

Limites:

- `PROTOCOL_MAX_PAYLOAD = 255`.
- `PROTOCOL_MAX_FRAME = 261` bytes antes de COBS.

## 2) PHY IDs y capabilities

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

- `0x01`: `FW_CAPABILITY_RX_STATS`
- `0x02`: `FW_CAPABILITY_LL_PDU_META`
- `0x04`: `FW_CAPABILITY_LL_STATS_EXT`

En firmware `1.0.0` actual: `capabilities = 0x07`.

## 3) Estado de comandos

### Estables en el baseline actual

- `0x01` `RADIO_INIT`
- `0x02` `SET_CHANNEL`
- `0x03` `SET_POWER`
- `0x04` `SET_PHY`
- `0x05` `GET_INFO`
- `0x06` `GET_STATS`
- `0x07` `SET_ADV_HOP`
- `0x08` `SET_PROP_CONFIG`
- `0x10` `RX_START`
- `0x11` `RX_STOP`
- `0x20` `TX_RAW`
- `0x21` `TX_CONTINUOUS`
- `0x22` `TX_BURST`
- `0x23` `TX_FRAME`
- `0x24` `TX_STOP`

### Implementados pero tratados como experimentales

- `0x30` `JAM_CONTINUOUS`
- `0x33` `JAM_STOP`

### Modos de prueba TX implementados

- `0x55` `TX_CW`
- `0x56` `TX_PRBS`
- `0x57` `TX_TEST_STOP`

### Aceleración criptográfica implementada

- `0x59` `CMD_RANDOM`
- `0x5A` `CMD_AES_ECB`
- `0x5B` `CMD_AES_CCM`
- `0x5C` `CMD_AES_CTR`
- `0x5D` `CMD_AES_CBC`
- `0x5E` `CMD_AES_GCM`
- `0x5F` `CMD_SHA256`
- `0x60` `CMD_ECDH`
- `0x61` `CMD_ECDSA_SIGN`
- `0x62` `CMD_ECDSA_VERIFY`

Los payloads criptográficos son contratos binarios específicos de cada operación y están validados por `command_processor.c` y construidos por `Radio` en `python/feralrf/radio.py`. Esta revisión conserva sus IDs y respuestas sin duplicar aquí toda la referencia de la API criptográfica.

### No implementados en el firmware actual

- `0x31` `JAM_REACTIVE`
- `0x32` `JAM_PATTERN`

Spectrum / RSSI scan aun no tiene IDs asignados: el rango `0x40-0x54` esta
retirado del stack BLE (removido 2026-07-20) y no se reusa.

Regla general:

- Un comando definido en host pero no implementado en firmware responde `RSP_ERROR(0x81)` con `ERR_INVALID_CMD(0x01)`.

## 4) Payloads de comandos implementados

### `RADIO_INIT (0x01)`

- Request payload: vacio.
- Response: `ACK` o `ERROR`.
- Efecto: reinicia estado de sesion, limpia RX, metricas y estadisticas LL.

### `SET_CHANNEL (0x02)`

- Request payload: `channel_u8`
- Response: `ACK` o `ERROR`

### `SET_POWER (0x03)`

- Request payload: `power_i8` enviado como byte
- Response: `ACK` o `ERROR`

### `SET_PHY (0x04)`

Payload soportado:

- corto: `phy_u8`
- extendido: `phy_u8 + channel_u16 + frequency_hz_u32`

Notas:

- La API Python usa el formato extendido por defecto.
- `channel` y `frequency_hz` pueden ser `0` para usar defaults del backend.

### `GET_INFO (0x05)`

- Request payload: vacio
- Response: `RSP_INFO`

### `GET_STATS (0x06)`

- Request payload: vacio
- Response: `RSP_STATS`

### `SET_ADV_HOP (0x07)`

- Request payload: `enabled_u8`
- `0`: deshabilitado
- `1`: habilitado
- Response: `ACK` o `ERROR`

Aplica a RX BLE sobre canales de advertising.

Nota: la direccion de advertising BLE (antes configurable via `SET_BLE_ADDR`,
retirado) y el modo de scan (antes `SET_BLE_SCAN_MODE`, retirado) ya no forman
parte del protocolo. La captura BLE actual es RX crudo por PHY, sin protocolo
de conexion (ver seccion 9 y `docs/ARCHITECTURE.md`).

### `SET_PROP_CONFIG (0x08)`

Payload de 18 bytes:

```text
freq_hz_u32 | mod_type_u8 | symbol_rate_u32 | deviation_u16 | rx_bw_u8 | sync_word_u32 | format_conf_u16
```

`format_conf` conserva el bitfield `formatConf` de 16 bits del comando TI propietario.
El firmware lo aplica a la estructura activa: `0` restaura `0x00A0` (sync word
de 32 bits, MSB first, sin FEC ni whitening) y un valor distinto de cero se
copia literalmente. El campo controla formato/framing TI —longitud del sync
word, inversión/orden de bits, coding y whitening/CRC—; no selecciona la
modulación ni implementa por sí mismo 4-FSK o 4-GFSK. Un payload de 16 bytes
sin `format_conf` se acepta por compatibilidad y usa `0`.

Notas:

- El host serializa `mod_type` como `u8`. Firmware lo parsea sin whitelist,
  lo copia al campo TI `modType` de 3 bits y responde ACK sin comprobar que el
  RF Core soporte el significado solicitado. Para `4`, `5` y `6`, el valor
  llega al backend, pero aceptación wire no equivale a modulación implementada.

| `mod_type` | Etiqueta usada por presets/API | Contrato del backend TI activo | Estado implementado |
|---:|---|---|---|
| `0` | FSK | FSK | Soportado por el backend; propiedades físicas no medidas independientemente. |
| `1` | GFSK | GFSK | Soportado por el backend; entrega raw demostrada para presets concretos, no caracterización física. |
| `2` | OOK/ASK | OOK | Ruta genOOK dedicada presente; lifecycle/recovery parcial. |
| `4` | MSK | Reservado | Valor aceptado y copiado, pero MSK no está implementado por este backend. |
| `5` | 4-FSK | Reservado | Valor aceptado y copiado, pero 4-FSK no está implementado por este backend. |
| `6` | 4-GFSK | Reservado | Valor aceptado y copiado, pero 4-GFSK no está implementado por este backend. |

- OOK carga patches MCE/RFE dedicados que el backend declara no descargables
  con seguridad durante la misma ejecución. Antes de seleccionar otra
  frecuencia o modo se requiere reiniciar realmente el CC1352, retirar y
  restaurar energía o reflashear; `RADIO_INIT` por wire no sustituye ese reset.
- El firmware no rechaza necesariamente una reconfiguración posterior: puede
  aceptar/copiar nuevos campos y limpiar su flag lógico OOK sin haber descargado
  de forma segura los patches. Ese ACK o cambio de estado software no demuestra
  una transición RF utilizable.
- [`Radio.reset_device()`](PYTHON_API.md#reset_devicewait-float--15---none)
  intenta el reset del CC1352 mediante Cat-Shell, pero no es un power-cycle y
  sólo es seguro cuando se verificó el puerto Shell asociado.

### `RX_START (0x10)`

- Request payload: vacio
- Response inmediata: `ACK` o `ERROR`

Notas:

- Si hay TX activo o jam activo, puede responder `ERR_INVALID_STATE`.
- Si el backend RF falla al iniciar, el host puede recibir despues un `RSP_ERROR` asincrono con `ERR_RF_INIT_FAILED`.

### `RX_STOP (0x11)`

- Request payload: vacio
- Response: `ACK` o `ERROR`

### `TX_RAW (0x20)`

Payload:

```text
tx_len_u8 | data[tx_len] | power_i8
```

Notas:

- `tx_len` no puede ser `0`.
- El firmware actual impone maximo efectivo de `125` bytes por la cola de control.
- Falla con `ERR_INVALID_STATE` si RX esta activo o ya existe otro TX pendiente.

### `TX_CONTINUOUS (0x21)`

Payload:

```text
tx_len_u8 | data[tx_len] | interval_us_u32
```

- Response: `ACK` o `ERROR`
- El flujo sigue activo hasta `TX_STOP`.

### `TX_BURST (0x22)`

Payload:

```text
tx_len_u8 | data[tx_len] | count_u16 | interval_us_u32
```

- Response: `ACK` o `ERROR`

### `TX_FRAME (0x23)`

Payload:

```text
tx_len_u8 | data[tx_len]
```

- Response: `ACK` o `ERROR`

Semantica:

- Reusa la potencia configurada con `SET_POWER`.
- El framing final depende del PHY activo.

### `TX_STOP (0x24)`

- Request payload: vacio
- Response: `ACK` o `ERROR`

### Modos de prueba TX (`0x55`–`0x57`)

- `TX_CW`: payload vacío; requiere una configuración RF previa.
- `TX_PRBS`: payload `pattern_u8`; `0x01` selecciona PRBS15 y `0x02` PRBS32.
- `TX_TEST_STOP`: payload vacío.
- Respuesta: `ACK` o `ERROR`.

### `JAM_CONTINUOUS (0x30)` experimental

Payload:

```text
channel_u8 | power_i8 | duration_ms_u16
```

- Response: `ACK` o `ERROR`

Notas:

- Solo se considera experimental.
- El firmware usa una sesion RF dedicada con TX repetido.
- No forma parte del baseline oficial de validacion de RF util.

### `JAM_STOP (0x33)` experimental

- Request payload: vacio
- Response: `ACK` o `ERROR`

## 5) Respuestas

### `RSP_ACK (0x80)`

- Payload: vacio

### `RSP_ERROR (0x81)`

Payload:

- `error_code_u8`

Codigos actuales:

- `0x01`: `ERR_INVALID_CMD`
- `0x02`: `ERR_INVALID_PAYLOAD`
- `0x03`: `ERR_INVALID_FRAME`
- `0x04`: `ERR_FRAME_TOO_LONG`
- `0x05`: `ERR_INVALID_STATE`
- `0x06`: `ERR_RF_INIT_FAILED`
- `0x07`: `ERR_RF_NOT_READY`

Notas:

- En errores de parseo temprano, `SEQ` puede llegar como `0`.
- `ERR_RF_INIT_FAILED` puede llegar de forma asincrona tras `RX_START`.

### `RSP_RX_PACKET (0x90)`

Stream asincrono de paquetes recibidos.

Payload:

- `[0..7]` `timestamp_us_u64`
- `[8]` `channel_u8`
- `[9]` `rssi_i8`
- `[10]` `lqi_u8`
- `[11]` `crc_ok_u8`
- `[12]` `data_len_u8`
- `[13 .. 13+data_len-1]` `data`
- `+ [ll_pdu_kind_u8, ll_pdu_type_u8, ll_pdu_flags_u8]`

`data_len` maximo emitido actual:

- `239` bytes por limite de payload del protocolo.

`ll_pdu_kind`:

- `0`: `UNKNOWN`
- `1`: `ADV`
- `2`: `SCAN`
- `3`: `CONNECT`
- `4`: `DATA`

`ll_pdu_flags`:

- `0x01`: `PRIMARY_ADV_CH`
- `0x02`: `DATA_CH`
- `0x04`: `EXT_ADV`
- `0x08`: `RESERVED`

Notas:

- La metadata LL solo es significativa para BLE.
- Para IEEE 802.15.4 y PHYs propietarios, el paquete se entrega como RX crudo con metadata LL por defecto.

### `RSP_STATS (0x93)`

Payload base de 16 bytes:

- `[0..3]` `rx_ok_u32`
- `[4..7]` `rx_crc_err_u32`
- `[8..11]` `rx_drop_u32`
- `[12..15]` `rx_overflow_u32`

Extension LL de 20 bytes adicionales:

- `[16..19]` `ll_kind_unknown_u32`
- `[20..23]` `ll_kind_adv_u32`
- `[24..27]` `ll_kind_scan_u32`
- `[28..31]` `ll_kind_connect_u32`
- `[32..35]` `ll_kind_data_u32`

Compatibilidad:

- Los clientes deben tolerar `16` o `36` bytes.
- Si `capabilities & 0x04`, el host puede interpretar la extension LL.

### `RSP_INFO (0x94)`

Payload de 12 bytes:

- `[0]` `fw_major_u8`
- `[1]` `fw_minor_u8`
- `[2]` `fw_patch_u8`
- `[3]` `capabilities_u8`
- `[4..11]` `serial_ascii_8bytes`

### Respuestas criptográficas (`0x95`–`0x9C`)

- `0x95` `RSP_RANDOM`
- `0x96` `RSP_AES`
- `0x97` `RSP_AES_CCM`
- `0x98` `RSP_AES_GCM`
- `0x99` `RSP_SHA256`
- `0x9A` `RSP_ECDH`
- `0x9B` `RSP_ECDSA_SIG`
- `0x9C` `RSP_ECDSA_VERIFY`

## 6) Semantica de `SEQ`

- En request/response de control (`ACK`, `ERROR`, `INFO`, `STATS`), el firmware responde con el mismo `SEQ` recibido.
- `RSP_RX_PACKET` usa un contador interno independiente de los requests.

## 7) Reglas de estado y limites utiles

- No iniciar TX mientras RX este activo.
- No iniciar RX mientras haya TX pendiente o jam activo.
- `TX_RAW`, `TX_FRAME`, `TX_BURST` y `TX_CONTINUOUS` comparten el mismo limite efectivo de payload de control: `125` bytes.
- En BLE advertising, la carga util final sigue limitada por el backend BLE.
- En `TX_FRAME` BLE sobre canales de advertising, el host debe respetar el limite de `31` bytes de AdvData.
- En IEEE 802.15.4, el payload de TX frame no debe exceder `125` bytes.
- Tras entrar en OOK, el backend requiere un reset real del CC1352, power-cycle
  o reflash antes de cambiar de frecuencia o modo. `reset_device()` es un
  mecanismo host posible, no un requisito wire ni una garantía universal.

## 8) Reglas de compatibilidad para clientes

- No asumir que todos los comandos imaginados por docs viejos siguen presentes o estan implementados.
- Tratar `RSP_ERROR` como respuesta valida del protocolo, no como corrupcion de frame.
- Al parsear `RSP_STATS`, aceptar base `16` y extendido `36`.
- Al parsear `RSP_RX_PACKET`, validar primero la longitud base y luego metadata LL segun capabilities y longitud real.
- Para OOK, documentar en UX/tooling el reset real requerido y presentar
  `reset_device()` sólo como helper condicionado a un mapeo Bridge↔Shell
  previamente verificado. La evidencia reciente sólo confirmó recuperación
  tras un power-cycle físico completo.

## 9) Relacion con el baseline oficial

Este contrato describe lo que hoy expone firmware. No todo lo implementado forma parte del baseline oficial de soporte.

Baseline recomendado:

- control de sesion
- RX/TX multi-PHY
- configuracion propietaria
- captura RX BLE cruda por PHY (sin scan activo/pasivo, sin protocolo de conexion)
- recovery OOK condicionado a reinicio completo del CC1352; lifecycle robusto no validado

Fuera del baseline oficial inicial:

- spectrum
- jamming reactivo/pattern
- tooling ofensivo no-BLE aun no implementado

Retirado permanentemente (2026-07-20): el stack de protocolo BLE (conexion, GATT,
scan activo/pasivo) fue removido del firmware. BLE PHY se mantiene solo para
captura cruda; el manejo de protocolo BLE lo hace Sniffle.

Para el estado actual defendible de capacidades y validación, consulte
[`CURRENT_VALIDATION_MATRIX.md`](CURRENT_VALIDATION_MATRIX.md). El antiguo
[`VALIDATION_MATRIX.md`](VALIDATION_MATRIX.md) se conserva como baseline histórico;
los procedimientos ejecutables pertenecerán al futuro Validation Runbook.
