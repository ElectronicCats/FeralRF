# FeralRF - Plan Maestro Consolidado

Firmware universal para CatSniffer (CC1352P + RP2040) con capacidades de sniffing, TX/RX, jamming y spectrum analysis para BLE, Zigbee y Sub-1GHz.

---

## 1. Arquitectura del Sistema

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
│  │  - Autonomous Policy Engine                           │  │
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

## 2. Stack Tecnológico

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
| SDK | Pico SDK |
| Compiler | ARM GCC |
| Build | CMake |
| USB | TinyUSB (CDC dual interface) |

### Python API
| Componente | Tecnología |
|------------|------------|
| Core | Python 3.9+ |
| Serial | pyserial-asyncio |
| Protocol | COBS (cobs library) + struct |
| Testing | pytest, pytest-asyncio |

### Build & CI/CD
| Componente | Tecnología |
|------------|------------|
| Container | Docker (Ubuntu 22.04) |
| CI | GitHub Actions |
| Artifacts | .hex, .bin, .elf, .whl |

---

## 3. Protocolo de Comunicación

### Framing: COBS

```
Frame format (pre-COBS):
┌────────┬────────┬────────┬─────────────┬─────────┐
│ CMD_ID │  SEQ   │  LEN   │   PAYLOAD   │  CRC16  │
│  (1B)  │  (1B)  │ (2B LE)│  (0-255B)   │ (2B LE) │
└────────┴────────┴────────┴─────────────┴─────────┘
└──────────── COBS encoded, 0x00 delimited ─────────┘
```

- **COBS**: Elimina todos los 0x00 del payload, permite usar 0x00 como delimitador
- **CRC16**: CRC-16-CCITT sobre CMD_ID + SEQ + LEN + PAYLOAD
- **SEQ**: Número de secuencia para detectar paquetes perdidos

### Command IDs

```c
// ============= Configuration =============
#define CMD_RADIO_INIT          0x01
#define CMD_SET_CHANNEL         0x02
#define CMD_SET_POWER           0x03
#define CMD_SET_PHY             0x04
#define CMD_GET_INFO            0x05

// ============= RX Operations =============
#define CMD_RX_START            0x10
#define CMD_RX_STOP             0x11
#define CMD_RX_SET_FILTER       0x12
#define CMD_RX_SET_PROMISCUOUS  0x13

// ============= TX Operations =============
#define CMD_TX_RAW              0x20
#define CMD_TX_CONTINUOUS       0x21
#define CMD_TX_BURST            0x22

// ============= Jamming =============
#define CMD_JAM_CONTINUOUS      0x30
#define CMD_JAM_REACTIVE        0x31
#define CMD_JAM_PATTERN         0x32
#define CMD_JAM_STOP            0x33

// ============= Spectrum Analysis =============
#define CMD_SPECTRUM_SCAN       0x40
#define CMD_SPECTRUM_MONITOR    0x41
#define CMD_SPECTRUM_STOP       0x42

// ============= Autonomous Policies =============
#define CMD_POLICY_SET          0x50
#define CMD_POLICY_START        0x51
#define CMD_POLICY_STOP         0x52

// ============= Bootloader =============
#define CMD_ENTER_BOOTLOADER    0xF0
#define CMD_BOOTLOADER_VERSION  0xF1

// ============= Responses =============
#define RSP_ACK                 0x80
#define RSP_ERROR               0x81
#define RSP_RX_PACKET           0x90
#define RSP_SPECTRUM_DATA       0x91
#define RSP_JAM_EVENT           0x95
```

---

## 4. Estructura del Proyecto

```
feralrf/
├── docker/
│   └── Dockerfile                    # Build container
│
├── firmware/
│   ├── cc1352/
│   │   ├── CMakeLists.txt
│   │   ├── src/
│   │   │   ├── main.c
│   │   │   ├── command_processor.c
│   │   │   ├── radio_hal.c
│   │   │   ├── phy_manager.c
│   │   │   ├── tx_engine.c
│   │   │   ├── rx_engine.c
│   │   │   ├── jam_engine.c
│   │   │   ├── spectrum.c
│   │   │   ├── policy_engine.c
│   │   │   └── uart.c
│   │   ├── include/
│   │   │   ├── protocol.h
│   │   │   ├── radio_hal.h
│   │   │   └── config.h
│   │   └── smartrf_settings/
│   │
│   ├── rp2040/
│   │   ├── CMakeLists.txt
│   │   ├── src/
│   │   │   ├── main.c
│   │   │   ├── usb_bridge.c
│   │   │   ├── uart.c
│   │   │   └── timestamp.c
│   │   └── include/
│   │
│   ├── bootloader/
│   │   └── src/
│   │
│   └── tests/
│       ├── unit/
│       └── integration/
│
├── python/
│   ├── pyproject.toml
│   ├── feralrf/
│   │   ├── __init__.py
│   │   ├── radio.py
│   │   ├── protocol.py
│   │   ├── commands.py
│   │   ├── responses.py
│   │   ├── enums.py
│   │   ├── jamming.py
│   │   └── spectrum.py
│   ├── examples/
│   │   ├── ble_sniffer.py
│   │   ├── zigbee_jam.py
│   │   └── spectrum_scan.py
│   └── tests/
│
├── docs/
│   ├── hardware.md
│   ├── protocol.md
│   ├── regulatory.md
│   └── api.md
│
├── .github/
│   └── workflows/
│       ├── build.yml
│       └── release.yml
│
├── LICENSE
├── README.md
└── CLAUDE.md
```

---

## 5. Fases de Desarrollo

### FASE 0: Setup (Semana 1) ✅ COMPLETADA
- [x] Repo con estructura completa
- [x] Dockerfile con TI SDK + ARM GCC + Pico SDK
- [x] CMake compilando blinky en CC1352 (392 bytes code)
- [x] CMake compilando USB echo en RP2040 (46KB uf2)
- [x] GitHub Actions operativo (build.yml, release.yml)
- [x] Python package skeleton (13/13 tests passing)
- [x] COBS protocol implementado (librería cobs)
- [x] Documentación actualizada (CLAUDE.md, PLAN_MAESTRO.md, PINOUT.md)

### FASE 1: MVP BLE Sniffer (Semanas 2-3)
- [ ] COBS implementation (C + Python)
- [ ] Command processor funcional
- [ ] BLE PHY initialization
- [ ] RX streaming vía UART
- [ ] Python API básica (sync)
- [ ] Ejemplo: `ble_sniffer.py`

### FASE 2: TX + Jamming Básico (Semanas 4-5)
- [ ] TX raw packets
- [ ] Jamming continuo (CW)
- [ ] Power control (-20 a +20 dBm)
- [ ] Regulatory warnings

### FASE 3: Spectrum Analyzer (Semana 6)
- [ ] Spectrum scan (2.4 GHz)
- [ ] RSSI measurements
- [ ] Python visualization (matplotlib)

### FASE 4: Zigbee + Multi-PHY (Semanas 7-8)
- [ ] IEEE 802.15.4 RX/TX
- [ ] PHY switching dinámico
- [ ] Channel translation

### FASE 5: Reactive Jamming (Semanas 9-10)
- [ ] Jamming reactivo (<500µs)
- [ ] Policy engine autónomo
- [ ] Event streaming

### FASE 6: Sub-1GHz + Testing (Semanas 11-12)
- [ ] Sub-1GHz PHY (868/915 MHz)
- [ ] Unit tests >80% coverage
- [ ] Integration tests
- [ ] Documentación

### FASE 7: Bootloader + Release (Semanas 13-14)
- [ ] Custom bootloader
- [ ] OTA firmware update
- [ ] GitHub release
- [ ] PyPI package

---

## 6. Testing Strategy

### Unit Tests (CI/CD)
```c
// tests/unit/test_command_processor.c
void test_parse_set_channel(void) {
    uint8_t frame[] = {CMD_SET_CHANNEL, 0x01, 0x01, 0x00, 0x0F, 0xAB, 0xCD};
    command_t cmd;
    int result = parse_command(frame, sizeof(frame), &cmd);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(CMD_SET_CHANNEL, cmd.id);
}
```

### Integration Tests (HW)
```python
@pytest.mark.hardware
def test_ble_sniffer_receive():
    radio = Radio('/dev/ttyUSB0')
    radio.set_phy(PHY.BLE_1M)
    radio.set_channel(37)
    radio.start_rx()
    packets = list(radio.read_packets(timeout=5))
    assert len(packets) > 0
```

---

## 7. Riesgos y Mitigaciones

| Riesgo | Nivel | Mitigación |
|--------|-------|------------|
| Latencia jamming reactive >500µs | Alto | Autonomous mode, pre-cargar TX buffer |
| TI SDK breaking changes | Alto | Fijar versión 7.10.01.24, versionar configs |
| Regulatory compliance | Alto | Warnings explícitos, potencia limitada por defecto |
| UART buffer overflow | Medio | Flow control RTS/CTS, ring buffer 16KB |
| RF Core crashes | Medio | Watchdog timer, auto-recovery |

---

## 8. Decisiones Finalizadas

### Especificaciones Técnicas

| Parámetro | Valor |
|-----------|-------|
| Licencia | GPL-3.0 |
| Repositorio | Privado |
| Reactive Jamming | En MVP, target <500µs |
| Potencia TX máxima | +20 dBm |
| Bandas Sub-1GHz | 868 MHz + 915 MHz |
| Python package | `feralrf` |

### Hardware RP2040 ↔ CC1352

| Conexión | RP2040 | CC1352 | Dirección |
|----------|--------|--------|-----------|
| UART TX | UART0_TX | DIO12 | RP2040 → CC1352 |
| UART RX | UART0_RX | DIO13 | RP2040 ← CC1352 |
| UART RTS | UART0_RTS | DIO14 | RP2040 → CC1352 |
| UART CTS | UART0_CTS | DIO15 | RP2040 ← CC1352 |
| RESET_CC | GPIO15 | RESET_N | RP2040 → CC1352 |
| LED1 | GPIO28 | - | Active Low |
| LED2 | GPIO27 | - | Active Low |
| LED3 | GPIO26 | - | Active Low |
| SWD | No | - | Solo JTAG (DIO16/DIO17) |

### Proceso

| Aspecto | Decisión |
|---------|----------|
| Workflow tracking | PLAN_MAESTRO.md |
| Docker | Dockerfile local |
| Bootloader custom | FASE 7 (no en MVP) |

---

## 9. Próximos Pasos

1. ~~Confirmar decisiones pendientes~~ ✅
2. Crear estructura del proyecto
3. Configurar Docker + CMake
4. Implementar FASE 0
5. Primera iteración: Blinky + UART echo
