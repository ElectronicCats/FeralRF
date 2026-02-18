# FeralRF - Nuevo Plan Maestro (Estado Real)

**Versión:** 2.0 | **Fecha:** 2026-02-17

Firmware universal para CatSniffer (CC1352P + RP2040) con capacidades de sniffing, TX/RX, jamming y spectrum analysis para BLE, Zigbee y Sub-1GHz.

---

## Visión del Proyecto

> "Firmware universal con acceso total a la API del CC1352 controlado desde Python para simular dispositivos, hacer ataques y sniffing"

### Principios Clave

| Principio | Decisión |
|-----------|----------|
| Nivel API | Comandos predefinidos (no bytecode/scripts) |
| Extensibilidad | Firmware fijo (nuevos comandos = recompilar) |
| Control TX | Ambos modos: raw (control total) + framing PHY (simulación legítima) |
| Ataques | Algunos predefinidos + capacidad de construir propios con TX_RAW |

---

## Estado Real del Proyecto (2026-02-17)

### Implementado y Funcional

| Componente | Estado | Detalle |
|------------|--------|---------|
| Comunicación COBS + CRC16 | ✅ | UART 921600 baud |
| Command processor | ✅ | Parseo y dispatch de comandos |
| BLE RX | ✅ | Sniffing completo con LL metadata |
| 802.15.4 RX | ✅ | Sniffing validado en hardware |
| Métricas RX | ✅ | `rx_ok`, `rx_crc_err`, `rx_drop`, `rx_overflow` |
| BLE advertising hopping | ✅ | Canales 37/38/39 |
| Python API | ✅ | Sync/async, comando básicos |
| BLE Release Gate | ✅ | Soak test 30min, canary regression |

### NO Implementado (a pesar de lo que dice el plan anterior)

| Componente | Estado Plan Anterior | Estado Real |
|------------|---------------------|-------------|
| TX_RAW | "Completado" | ❌ No implementado |
| TX_FRAME | No definido | ❌ No existe |
| TX_CONTINUOUS/BURST | "Completado" | ❌ No implementado |
| Jamming CW | "Completado" | ❌ No implementado |
| Jamming Reactivo | "Completado" | ❌ No implementado |
| Spectrum Analyzer | "Completado MVP" | ❌ No implementado |
| Sub-1GHz | "Completado MVP" | ❌ No implementado |
| Comandos de ataque | No definidos | ❌ No existen |

---

## Arquitectura del Sistema

```
┌─────────────────────────────────────────────────────────────┐
│                      HOST (PC/Linux)                         │
│  ┌────────────────────────────────────────────────────────┐ │
│  │              Python API (feralrf)                      │ │
│  │  - Async/Sync interfaces                              │ │
│  │  - Command builder                                    │ │
│  │  - Event dispatcher                                   │ │
│  │  - Protocol codec (COBS)                              │ │
│  └──────────────────────┬─────────────────────────────────┘ │
│                         │ USB-CDC                           │
└─────────────────────────┼───────────────────────────────────┘
                          │
┌─────────────────────────┼───────────────────────────────────┐
│                         │         RP2040                     │
│  ┌──────────────────────▼────────────────────────────────┐  │
│  │            USB-CDC Bridge (TinyUSB)                   │  │
│  │  - Transparent UART ↔ USB                             │  │
│  │  - Hardware Flow Control (RTS/CTS)                    │  │
│  │  - Microsecond Timestamping                           │  │
│  │  - CC1352 Reset Monitoring & Recovery                 │  │
│  └──────────────────────┬────────────────────────────────┘  │
└─────────────────────────┼───────────────────────────────────┘
                          │ UART (921600, RTS/CTS)
┌─────────────────────────┼───────────────────────────────────┐
│                         │         CC1352P                    │
│  ┌──────────────────────▼────────────────────────────────┐  │
│  │            Command Processor                          │  │
│  │  - COBS framing                                       │  │
│  │  - Command dispatcher                                 │  │
│  │  - Response formatter                                 │  │
│  └──────────────────────┬────────────────────────────────┘  │
│                         │                                    │
│  ┌──────────────────────▼────────────────────────────────┐  │
│  │            Radio Abstraction Layer                    │  │
│  │  - PHY Manager (BLE/Zigbee/Sub-1GHz)                 │  │
│  │  - TX/RX Engine                                       │  │
│  │  - Jamming Engine                                     │  │
│  │  - Spectrum Analyzer                                  │  │
│  │  - Attack Engine (nuevo)                              │  │
│  └──────────────────────┬────────────────────────────────┘  │
│                         │                                    │
│  ┌──────────────────────▼────────────────────────────────┐  │
│  │          TI Driverlib + RF Core                       │  │
│  │  - RF patches (BLE5, IEEE 802.15.4)                  │  │
│  │  - SmartRF configs                                    │  │
│  │  - Cortex-M0+ RF Core firmware                       │  │
│  └───────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────┘
```

---

## Protocolo de Comunicación (Actualizado)

### Framing: COBS + CRC16-CCITT

```
Frame format (pre-COBS):
┌────────┬────────┬────────┬─────────────┬─────────┐
│ CMD_ID │  SEQ   │  LEN   │   PAYLOAD   │  CRC16  │
│  (1B)  │  (1B)  │ (2B LE)│  (0-255B)   │ (2B LE) │
└────────┴────────┴────────┴─────────────┴─────────┘
└──────────── COBS encoded, 0x00 delimited ─────────┘
```

### Comandos Implementados (Firmware 1.0.0)

```c
// ============= Configuration (IMPLEMENTADOS) =============
#define CMD_RADIO_INIT          0x01  // ✅
#define CMD_SET_CHANNEL         0x02  // ✅
#define CMD_SET_POWER           0x03  // ✅
#define CMD_SET_PHY             0x04  // ✅
#define CMD_GET_INFO            0x05  // ✅
#define CMD_GET_STATS           0x06  // ✅

// ============= RX Operations (IMPLEMENTADOS) =============
#define CMD_RX_START            0x10  // ✅
#define CMD_RX_STOP             0x11  // ✅
```

### Comandos Pendientes de Implementación

```c
// ============= TX Operations (PENDIENTES) =============
#define CMD_TX_RAW              0x20  // ❌ CRÍTICO - TX bytes crudos
#define CMD_TX_CONTINUOUS       0x21  // ❌ TX continuo
#define CMD_TX_BURST            0x22  // ❌ Burst de N paquetes
#define CMD_TX_FRAME            0x23  // ❌ NUEVO - TX con framing PHY
#define CMD_TX_STOP             0x24  // ❌ Detener TX continuo/burst

// ============= Jamming (PENDIENTES) =============
#define CMD_JAM_CONTINUOUS      0x30  // ❌ Jamming CW
#define CMD_JAM_REACTIVE        0x31  // ❌ Jamming reactivo
#define CMD_JAM_PATTERN         0x32  // ❌ Jamming con patrón
#define CMD_JAM_STOP            0x33  // ❌ Detener jamming

// ============= Spectrum Analysis (PENDIENTES) =============
#define CMD_SPECTRUM_SCAN       0x40  // ❌ Scan de spectrum
#define CMD_SPECTRUM_MONITOR    0x41  // ❌ Monitor continuo
#define CMD_SPECTRUM_STOP       0x42  // ❌ Detener monitor

// ============= BLE Attacks (NUEVOS - NO DEFINIDOS) =============
#define CMD_BLE_DEAUTH          0x60  // ❌ Desconexión forzada
#define CMD_BLE_ADV_SPOOF       0x61  // ❌ Spoofing/replay advertising
#define CMD_BLE_MITM_START      0x62  // ❌ Iniciar MITM
#define CMD_BLE_MITM_STOP       0x63  // ❌ Detener MITM

// ============= 802.15.4 Attacks (NUEVOS - NO DEFINIDOS) =============
#define CMD_IEEE154_BEACON_INJECT  0x70  // ❌ Inyección de beacons
#define CMD_IEEE154_PAN_HIJACK     0x71  // ❌ Hijacking de PAN
#define CMD_IEEE154_REPLAY         0x72  // ❌ Replay de paquetes

// ============= Radio Primitives (NUEVOS - OPCIONALES) =============
#define CMD_SET_SYNC_WORD       0x80  // ❌ Sync word custom
#define CMD_SET_PREAMBLE        0x81  // ❌ Configuración de preamble
```

---

## Especificación de Comandos TX (Propuesta)

### CMD_TX_RAW (0x20)

Envía bytes crudos sin ninguna modificación del firmware.

**Payload Request:**
```
[data_len:1][data:N]
```

| Campo | Tipo | Descripción |
|-------|------|-------------|
| data_len | u8 | Longitud de datos (1-255) |
| data | bytes | Datos a transmitir (exactamente como están) |

**Comportamiento:**
- El firmware NO agrega preamble, sync word, ni CRC
- Útil para protocolos propietarios y ataques custom
- Requiere PHY configurada (afecta modulación)

**Ejemplo Python:**
```python
radio.set_phy(PHY.PROPRIETARY_GFSK)
radio.set_channel(1)
radio.tx_raw(b'\xaa\xbb\xcc\xdd')  # Bytes exactos
```

### CMD_TX_FRAME (0x23) - NUEVO

Envía datos con framing automático según la PHY configurada.

**Payload Request:**
```
[data_len:1][data:N]
```

| Campo | Tipo | Descripción |
|-------|------|-------------|
| data_len | u8 | Longitud del payload |
| data | bytes | Payload (sin preamble/sync/CRC) |

**Comportamiento según PHY:**

| PHY | Lo que agrega el firmware |
|-----|--------------------------|
| BLE_1M/2M/CODED | Preamble + Access Address + CRC24 |
| IEEE_802_15_4 | Preamble + SFD + CRC16 |
| PROPRIETARY_* | Configurable via flags |

**Ejemplo Python:**
```python
radio.set_phy(PHY.BLE_1M)
radio.set_channel(37)
# Solo envío el payload BLE, firmware agrega framing
radio.tx_frame(ble_adv_pdu)
```

### CMD_TX_CONTINUOUS (0x21)

Transmisión continua para jamming/DoS.

**Payload Request:**
```
[data_len:1][data:N][interval_us:4]
```

| Campo | Tipo | Descripción |
|-------|------|-------------|
| data_len | u8 | Longitud de datos |
| data | bytes | Datos a transmitir repetidamente |
| interval_us | u32 LE | Intervalo entre transmisiones (microsegundos) |

### CMD_TX_BURST (0x22)

Burst de N paquetes.

**Payload Request:**
```
[data_len:1][data:N][count:2][interval_us:4]
```

### CMD_TX_STOP (0x24)

Detiene TX_CONTINUOUS o TX_BURST activo.

**Payload Request:** Vacío

---

## Especificación de Comandos de Ataque (Propuesta)

### CMD_BLE_DEAUTH (0x60)

Envía paquete LL_TERMINATE para desconectar una conexión BLE.

**Payload Request:**
```
[conn_handle:2][reason:1]
```

| Campo | Tipo | Descripción |
|-------|------|-------------|
| conn_handle | u16 LE | Handle de conexión objetivo |
| reason | u8 | Razón de terminación (0x13 = "Remote User Terminated") |

**Requisitos:**
- Debe estar en canal de datos correcto
- Timing crítico (<150µs para respuesta)

### CMD_BLE_ADV_SPOOF (0x61)

Clona y retransmite paquetes advertising.

**Payload Request:**
```
[mode:1][interval_ms:2][adv_data_len:1][adv_data:N]
```

| Campo | Tipo | Descripción |
|-------|------|-------------|
| mode | u8 | 0=clone, 1=custom |
| interval_ms | u16 LE | Intervalo entre transmisiones |
| adv_data_len | u8 | Longitud de advertising data |
| adv_data | bytes | Advertising payload |

### CMD_IEEE154_BEACON_INJECT (0x70)

Inyecta beacons falsos en red Zigbee/802.15.4.

**Payload Request:**
```
[channel:1][interval_ms:2][beacon_len:1][beacon_data:N]
```

### CMD_IEEE154_REPLAY (0x72)

Replay de paquete capturado.

**Payload Request:**
```
[packet_id:4]  // ID del paquete en buffer de captura
```

**Alternativa:**
```
[channel:1][packet_len:1][packet_data:N]
```

---

## PHY IDs

```c
#define PHY_BLE_1M           0
#define PHY_BLE_2M           1
#define PHY_BLE_CODED_S8     2
#define PHY_BLE_CODED_S2     3
#define PHY_IEEE_802_15_4    4
#define PHY_SUB_1GHZ_868     5
#define PHY_SUB_1GHZ_915     6
#define PHY_PROPRIETARY_GFSK 7
```

---

## Responses

```c
#define RSP_ACK              0x80
#define RSP_ERROR            0x81
#define RSP_RX_PACKET        0x90
#define RSP_SPECTRUM_DATA    0x91
#define RSP_STATS            0x93
#define RSP_INFO             0x94
#define RSP_JAM_EVENT        0x95
#define RSP_ATTACK_EVENT     0x96  // NUEVO
```

### RSP_ERROR (0x81) - Códigos

```c
#define ERR_INVALID_CMD      0x01
#define ERR_INVALID_PAYLOAD  0x02
#define ERR_INVALID_FRAME    0x03
#define ERR_FRAME_TOO_LONG   0x04
#define ERR_PHY_NOT_READY    0x05  // NUEVO
#define ERR_TX_FAILED        0x06  // NUEVO
#define ERR_ATTACK_FAILED    0x07  // NUEVO
```

---

## Fases de Desarrollo (REVISADAS - Estado Real)

### FASE 0: Setup ✅ COMPLETADA
- [x] Repo con estructura completa
- [x] Dockerfile con TI SDK + ARM GCC + Pico SDK
- [x] CMake compilando firmware
- [x] GitHub Actions operativo
- [x] Python package skeleton
- [x] COBS protocol implementado

### FASE 1: MVP BLE Sniffer ✅ COMPLETADA
- [x] COBS implementation (C + Python)
- [x] Command processor funcional
- [x] BLE PHY initialization
- [x] RX streaming via UART
- [x] Python API básica
- [x] BLE advertising hopping
- [x] Métricas RX

### FASE 2: 802.15.4 RX ✅ COMPLETADA
- [x] IEEE 802.15.4 PHY
- [x] RX validado en hardware
- [x] Barrido de canales 11-26

### FASE 3: TX Vertical 🔄 EN PROGRESO (Nueva prioridad)
- [ ] CMD_TX_RAW implementado
- [ ] CMD_TX_FRAME implementado
- [ ] Smoke test TX BLE
- [ ] Smoke test TX 802.15.4
- [ ] CMD_TX_CONTINUOUS
- [ ] CMD_TX_BURST
- [ ] CMD_TX_STOP

### FASE 4: BLE Attacks ⏳ PENDIENTE
- [ ] CMD_BLE_DEAUTH
- [ ] CMD_BLE_ADV_SPOOF
- [ ] CMD_BLE_MITM (básico)
- [ ] Ejemplos Python de ataques

### FASE 5: 802.15.4 Attacks ⏳ PENDIENTE
- [ ] CMD_IEEE154_BEACON_INJECT
- [ ] CMD_IEEE154_REPLAY
- [ ] CMD_IEEE154_PAN_HIJACK
- [ ] Ejemplos Python de ataques

### FASE 6: Jamming ⏳ PENDIENTE
- [ ] CMD_JAM_CONTINUOUS (CW)
- [ ] CMD_JAM_REACTIVE
- [ ] CMD_JAM_PATTERN
- [ ] Policy engine básico

### FASE 7: Spectrum Analyzer ⏳ PENDIENTE
- [ ] CMD_SPECTRUM_SCAN
- [ ] CMD_SPECTRUM_MONITOR
- [ ] Visualización Python

### FASE 8: Sub-1GHz ⏳ PENDIENTE
- [ ] PHY_SUB_1GHZ_868
- [ ] PHY_SUB_1GHZ_915
- [ ] RX/TX validado

### FASE 9: Bootloader + Release ⏳ PENDIENTE
- [ ] Custom bootloader
- [ ] OTA firmware update
- [ ] GitHub release
- [ ] PyPI package

---

## Stack Tecnológico

### Firmware CC1352
| Componente | Tecnología |
|------------|------------|
| SDK | TI SimpleLink CC13xx/CC26xx SDK 7.10.01.24 (fijo) |
| Compiler | ARM GCC 10.3 / TI Arm Clang v3.x |
| Build | CMake 3.20+ + Ninja |
| RTOS | TI-RTOS 7 |
| Memoria | Asignación estática (no malloc) |
| Buffer RX | 16KB circular buffer |

### Firmware RP2040
| Componente | Tecnología |
|------------|------------|
| SDK | Pico SDK 2.0.0 |
| USB | TinyUSB (CDC dual interface) |

### Python API
| Componente | Tecnología |
|------------|------------|
| Core | Python 3.9+ |
| Serial | pyserial-asyncio |
| Testing | pytest, pytest-asyncio |

---

## Riesgos y Mitigaciones

| Riesgo | Nivel | Mitigación |
|--------|-------|------------|
| Latencia jamming reactivo >500µs | Alto | Modo autónomo, pre-cargar TX buffer |
| TI SDK breaking changes | Alto | Versión fija 7.10.01.24 |
| Cumplimiento regulatorio | Alto | Warnings explícitos, potencia limitada por defecto |
| UART buffer overflow | Medio | RTS/CTS flow control, 16KB ring buffer |
| RF Core crashes | Medio | Watchdog timer, auto-recovery |
| Ataques maliciosos por usuarios | Medio | GPL-3.0, documentación ética |

---

## Próximos Pasos Inmediatos

### Prioridad 1: TX_RAW (Semana 1-2)
1. Implementar `CMD_TX_RAW` en `firmware/cc1352/src/radio_if.c`
2. Añadir comando a `command_processor.c`
3. Actualizar Python API en `feralrf/radio.py`
4. Crear smoke test `python/examples/smoke_tx_raw.py`
5. Validar con segundo dispositivo o SDR

### Prioridad 2: TX_FRAME (Semana 2-3)
1. Implementar framing automático según PHY
2. Validar CRC correcto
3. Smoke test BLE advertising TX
4. Smoke test 802.15.4 TX

### Prioridad 3: BLE Deauth (Semana 3-4)
1. Implementar `CMD_BLE_DEAUTH`
2. Tracking de conexiones en firmware
3. Ejemplo Python de uso

---

## Referencia de Archivos

| Archivo | Propósito |
|---------|-----------|
| [PLAN_MAESTRO.md](PLAN_MAESTRO.md) | Plan anterior (desactualizado) |
| [protocol.md](protocol.md) | Protocolo implementado actualmente |
| [SESSION_STATE.md](SESSION_STATE.md) | Estado de sesión |
| [../hardware/PINOUT.md](../hardware/PINOUT.md) | Pinout de hardware |

---

## Changelog

| Fecha | Versión | Cambios |
|-------|---------|---------|
| 2026-02-17 | 2.0 | Reescritura completa con estado real, nuevos comandos TX/ataque |
| 2026-02-18 | 1.x | Versión anterior (marcaba fases incompletas como completadas) |
