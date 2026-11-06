# API Python de FeralRF

> **Referencia canónica de la superficie pública prevista del paquete `feralrf`.**

Reconciliada con el código fuente el 2026-11-06. Los exports de [`python/feralrf/__init__.py`](../python/feralrf/__init__.py) y la implementación pública actual determinan qué símbolos existen cuando esta referencia y el código difieren; una diferencia sigue siendo una discrepancia que debe resolverse explícitamente.

Documentos relacionados:

- [Arquitectura de software](ARCHITECTURE.md)
- [Contrato del protocolo wire](protocol.md)
- [Índice y autoridad documental](README.md)
- [Matriz canónica actual de capacidades y validación](CURRENT_VALIDATION_MATRIX.md)
- [Baseline histórico de validación](VALIDATION_MATRIX.md)
- [Material de trabajo para el futuro Validation Runbook](User-Manual/)

Este documento describe interfaces, parámetros y semántica de resultados. Las secuencias exactas de ensayo, terminales, marcadores, configuración física, criterios PASS/FAIL y recuperación pertenecen al futuro Validation Runbook.

## 1. Símbolos exportados por el paquete

`feralrf.__all__` exporta:

- Clases y tipos: `Radio`, `Packet`, `DeviceInfo`, `DeviceStats`, `RxStreamError`.
- Enums y estado de comandos: `PHY`, `Command`, `Response`, `STABLE_COMMANDS`, `EXPERIMENTAL_COMMANDS`, `PENDING_COMMAND_IDS`.
- Presets: `PROP_PRESETS`.
- Excepciones: `FeralRFError`, `ConnectionError`, `ProtocolError`, `CommandError`, `RadioError`, `CryptoError`.

`CommandError` expone `error_code`. Los métodos también pueden producir errores de timeout internos derivados de `FeralRFError`; `TimeoutError` existe en `feralrf.exceptions`, pero no forma parte de `feralrf.__all__` en la revisión actual.

## 2. Tipos de datos

### `Packet`

Paquete recibido con `timestamp_us`, `channel`, `rssi_dbm`, `lqi`, `crc_ok`, `data` y metadata opcional `ll_pdu_kind`, `ll_pdu_type` y `ll_pdu_flags`.

La interpretación física de metadata depende del PHY y de la implementación correspondiente. Un valor expuesto por software no sustituye una medición RF independiente.

### `DeviceInfo`

Resultado de `Radio.init()` con `firmware_version`, `capabilities` y `serial`. La versión y el serial reportados no identifican por sí solos un commit, hash binario o unidad física.

### `DeviceStats`

Contadores `rx_ok`, `rx_crc_err`, `rx_drop` y `rx_overflow`, más contadores LL opcionales cuando firmware anuncia la extensión correspondiente.

### `RxStreamError`

Evento asíncrono producido por `read_packets()` para un `RSP_ERROR` de firmware fuera del ciclo request/response normal. Contiene `error_code` y `context`.

## 3. Construcción y ciclo de conexión

### `Radio(port: Optional[str] = None, baudrate: int = 921600)`

Crea el cliente síncrono. Si `port` es `None`, `connect()` usa el primer dispositivo que devuelve `list_devices()`.

### `Radio.list_devices() -> list`

Método estático que devuelve candidatos seriales. Para VID CatSniffer `0x1209`, filtra la descripción/producto para conservar interfaces que contienen `Bridge`.

### `connect() -> None`

Abre el puerto serial y prepara los buffers. Es idempotente si la instancia ya tiene el puerto abierto.

### `disconnect() -> None`

Cierra el puerto abierto. También se llama al salir de un bloque `with Radio(...) as radio:`; `__enter__` conecta y `__exit__` desconecta.

### `init() -> DeviceInfo`

Conecta si es necesario, envía `RADIO_INIT`, solicita `GET_INFO`, almacena las capacidades y devuelve `DeviceInfo`.

No existe un método público separado `get_info()` en la implementación actual. `init()` es la interfaz que devuelve la información del dispositivo.

### `reset_device(wait: float = 1.5) -> None`

La implementación actual deduce un puerto Cat-Shell sumando 2 al sufijo numérico del puerto Bridge, cierra temporalmente Bridge, envía `boot` y `exit`, espera y reinicializa si la instancia estaba conectada. Esos comandos intentan un reset de hardware del CC1352; el método no corta físicamente su alimentación y `RADIO_INIT` posterior sólo reinicializa el estado de control host/firmware.

Esta inferencia `Bridge + 2` no es universalmente fiable: la enumeración USB puede asignar pares no aritméticos. El método sólo debe usarse cuando ese mapeo haya sido comprobado para el equipo y la sesión. Es un mecanismo de recovery posible para OOK, no una garantía de recovery universal: la evidencia reciente conservada en la [matriz canónica](CURRENT_VALIDATION_MATRIX.md) confirmó recuperación después de un power-cycle físico completo, pero no estableció un lifecycle OOK robusto mediante este helper. Consulte las [preguntas abiertas de hardware](../hardware/OPEN_HARDWARE_QUESTIONS.md) para los límites de reset/boot y el material operativo en [`User-Manual/`](User-Manual/) para el estado de los procedimientos.

## 4. Configuración de radio

### `set_phy(phy: PHY, channel: int = 0, frequency_hz: int = 0) -> None`

Selecciona el PHY y envía el formato extendido de `SET_PHY`. `channel=0` y `frequency_hz=0` permiten que el backend use sus defaults.

### `set_channel(channel: int) -> None`

Actualiza el canal mediante `SET_CHANNEL`.

### `set_power(power_dbm: int) -> None`

Envía la potencia solicitada como entero de 8 bits con signo en el protocolo. La aceptación del comando no prueba potencia RF medida.

### `set_adv_hop(enabled: bool) -> None`

Activa o desactiva hopping de canales de advertising durante RX BLE. Solo aplica a recepción BLE en esos canales; no restaura el stack BLE de conexión/GATT retirado.

### `configure_prop(frequency_hz: int, mod_type: int = 1, symbol_rate: int = 50000, deviation: int = 100, rx_bw: int = 0x52, sync_word: int = 0x930B51DE, format_conf: int = 0) -> None`

Envía `SET_PROP_CONFIG` para el backend propietario. Los valores son configuración solicitada, no mediciones físicas. El host puede serializar cualquier `u8`, pero el backend TI activo sólo define `0=FSK`, `1=GFSK` y `2=OOK`; los valores `4/5/6` usados por presets llamados MSK/4-FSK/4-GFSK se aceptan y copian a un campo reservado, sin implementar esas modulaciones nombradas. `format_conf` configura el bitfield TI de formato/framing y no selecciona modulación.

OOK (`mod_type=2`) carga patches dedicados. Antes de cambiar de frecuencia o modo, el backend requiere un reset real del CC1352, power-cycle o reflash; [`reset_device()`](#reset_devicewait-float--15---none) intenta una de esas rutas mediante reset de hardware condicionado al mapeo Cat-Shell, pero no es un power-cycle ni tiene recovery OOK universalmente validado.

## 5. Recepción y estadísticas

### `get_stats(timeout: float = 2.0) -> DeviceStats`

Solicita `GET_STATS`. Acepta el payload base y la extensión LL cuando la capacidad está anunciada.

### `start_rx() -> None`

Solicita el inicio de RX. Puede producir `CommandError` por estado inválido y posteriormente un `RxStreamError` si firmware emite un fallo RF asíncrono.

### `read_packets(timeout: Optional[float] = 1.0) -> Iterator[Union[Packet, RxStreamError]]`

Itera paquetes y errores asíncronos durante una ventana total de `timeout` segundos. `timeout=None` mantiene el stream hasta que el consumidor deja de iterar.

### `read_one_packet(timeout: float = 1.0) -> Optional[Packet]`

Devuelve el primer `Packet` recibido dentro del timeout o `None`. Omite eventos `RxStreamError`; los consumidores que necesiten observar errores asíncronos deben usar `read_packets()`.

### `stop_rx(retries: int = 3, timeout: float = 1.0) -> None`

Solicita `RX_STOP` hasta `retries` veces. Un ACK confirma la respuesta de control; no es una medición independiente del estado físico RF.

## 6. Transmisión

Todos estos métodos confirman la respuesta de control del firmware. Un ACK no equivale a terminación RF, radiación medida ni recepción por un peer.

### `transmit(packet: bytes, power_dbm: int = -128, timeout: float = 5.0) -> None`

Envía `TX_RAW`. El valor por defecto `-128` se serializa como potencia solicitada; el firmware actual resuelve valores que no están en la tabla de potencia activa al valor soportado más cercano. Para reutilizar de forma explícita la potencia establecida por `set_power()`, use `transmit_frame()` o pase a `transmit()` el valor deseado.

### `transmit_frame(packet: bytes, timeout: float = 5.0) -> None`

Envía `TX_FRAME`; rechaza un payload vacío. El framing final depende del PHY activo.

### `transmit_burst(packet: bytes, count: int, interval_us: int = 0, timeout: float = 5.0) -> None`

Programa un burst; valida payload no vacío, `count` en `1..65535` e intervalo en el rango `uint32`.

### `transmit_continuous(packet: bytes, interval_us: int = 0, timeout: float = 5.0) -> None`

Inicia TX repetitivo hasta `TX_STOP`; valida payload e intervalo.

### `stop_transmit(timeout: float = 5.0) -> None`

Envía `TX_STOP` para una operación repetitiva activa.

## 7. Modos de prueba TX

- `tx_cw(power_dbm: int = 0) -> None`: configura potencia e inicia portadora continua; requiere un PHY previamente seleccionado.
- `tx_prbs(power_dbm: int = 0, pattern: str = "prbs15") -> None`: inicia PRBS15 o PRBS32; cualquier otro patrón produce `ValueError`.
- `tx_test_stop() -> None`: detiene CW o PRBS y puede llamarse sin un test activo.

Estos métodos exponen control de firmware. La forma, frecuencia, potencia y cese de la señal requieren validación instrumental separada.

## 8. Aceleración criptográfica

La superficie pública actual incluye:

- `random_bytes(n: int) -> bytes`;
- `aes_encrypt(key: bytes, data: bytes, mode: str, iv: Optional[bytes] = None) -> bytes`;
- `aes_decrypt(key: bytes, data: bytes, mode: str, iv: Optional[bytes] = None) -> bytes`;
- `aes_ccm_encrypt(key: bytes, nonce: bytes, aad: bytes, plaintext: bytes, tag_len: int) -> tuple`;
- `aes_ccm_decrypt(key: bytes, nonce: bytes, aad: bytes, ciphertext: bytes, tag: bytes, tag_len: int) -> bytes`;
- `aes_gcm_encrypt(key: bytes, iv: bytes, aad: bytes, plaintext: bytes) -> tuple`;
- `aes_gcm_decrypt(key: bytes, iv: bytes, aad: bytes, ciphertext: bytes, tag: bytes) -> bytes`;
- `sha256(data: bytes) -> bytes`;
- `ecdh(my_priv: bytes, peer_pub: bytes, curve: str) -> bytes`;
- `ecdsa_sign(priv: bytes, msg_hash: bytes, curve: str) -> bytes`;
- `ecdsa_verify(pub: bytes, msg_hash: bytes, sig: bytes, curve: str) -> bool`.

Los métodos validan tamaños y formatos antes de enviar sus comandos. Los fallos criptográficos del firmware se exponen como `CryptoError`; las firmas exactas y restricciones actuales están en `radio.py`.

## 9. API experimental y pendiente

### Experimental

- `start_jam(channel: int, power_dbm: int = 20, duration_ms: int = 3000, timeout: float = 5.0) -> None`
- `stop_jam(timeout: float = 5.0) -> None`

Los comandos existen y devuelven respuestas de control. No forman parte del baseline funcional estable porque un ACK no demuestra efecto de interferencia.

### Pendiente

- Spectrum/RSSI scan no tiene una API pública final ni IDs de firmware asignados.
- `JAM_REACTIVE` (`0x31`) y `JAM_PATTERN` (`0x32`) permanecen en `PENDING_COMMAND_IDS` y no son métodos implementados.
- El stack de protocolo BLE de conexión/GATT/scan activo o pasivo fue retirado; permanecen los PHY BLE para captura raw.

## 10. Enums y presets

### `PHY`

Expone `BLE_1M`, `BLE_2M`, `BLE_CODED_S8`, `BLE_CODED_S2`, `IEEE_802_15_4`, `SUB_1GHZ_868`, `SUB_1GHZ_915` y `PROPRIETARY_GFSK`.

`Command` y `Response` reflejan los IDs documentados en [`protocol.md`](protocol.md). `STABLE_COMMANDS`, `EXPERIMENTAL_COMMANDS` y `PENDING_COMMAND_IDS` permiten inspeccionar el estado declarado de comandos.

### `PROP_PRESETS`

Diccionario público de parámetros que puede aplicarse con:

```python
radio.configure_prop(**PROP_PRESETS["gfsk_868_50k"])
```

Los nombres son etiquetas de configuración. Un preset llamado Wi-SUN, Sidewalk, Wireless M-Bus o MIOTY no implementa automáticamente framing, MAC, seguridad, asociación ni interoperabilidad de ese protocolo. La evaluación vigente de implementación y evidencia está en [`CURRENT_VALIDATION_MATRIX.md`](CURRENT_VALIDATION_MATRIX.md), sustentada por [`Presets_Reports/`](Presets_Reports/).

## 11. Restricciones de estado y errores

- Los métodos de control pueden elevar `ConnectionError`, `ProtocolError` o `CommandError`; las operaciones criptográficas pueden elevar `CryptoError`.
- Firmware puede rechazar RX durante TX/jamming o TX durante RX con un error de estado.
- `read_packets()` es la interfaz que conserva errores RF asíncronos como datos del stream.
- Los límites wire y payload se definen en [`protocol.md`](protocol.md); la validación de una capacidad RF pertenece a la documentación de validación.

## 12. Integración KillerBee

El adaptador vive en `feralrf.integrations.killerbee` y usa la API pública de `Radio`, incluido `read_one_packet()`. La guía de integración está en [`killerbee-catsniffer.md`](killerbee-catsniffer.md) y [`TESTING-ON-LINUX.md`](TESTING-ON-LINUX.md). Es documentación de integración supporting; la existencia del adaptador y sus pruebas host no demuestra por sí sola una validación completa con hardware y herramientas KillerBee.
