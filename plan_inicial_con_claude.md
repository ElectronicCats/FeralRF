# CatSniffer Firmware - Plan Completo de Desarrollo  con Claude

## RESUMEN EJECUTIVO

Firmware universal para CC1352 que expone toda la API del radio vía Serial/USB con Python API, soportando sniffing, TX/RX, jamming y spectrum analysis para BLE, Zigbee y Sub-1GHz.

---

## 1. ARQUITECTURA DEL SISTEMA

copy


┌─────────────────────────────────────────────────────────┐
│                      HOST (PC/Linux)                     │
│                                                          │
│  ┌────────────────────────────────────────────────────┐ │
│  │         Python API (catsniffer)                    │ │
│  │  - Async/Sync interfaces                          │ │
│  │  - Command builder                                │ │
│  │  - Event dispatcher                               │ │
│  │  - Protocol codec (COBS)                          │ │
│  └─────────────────┬──────────────────────────────────┘ │
│                    │ Serial (921600 baud)                │
└────────────────────┼─────────────────────────────────────┘
                     │
┌────────────────────┼─────────────────────────────────────┐
│                    │         RP2040                      │
│  ┌─────────────────▼──────────────────────────────────┐ │
│  │     USB-CDC Bridge (TinyUSB)                       │ │
│  │  - Transparent UART↔USB                           │ │
│  │  - Flow control                                   │ │
│  │  - Optional: Bootloader trigger via DTR          │ │
│  └─────────────────┬──────────────────────────────────┘ │
└────────────────────┼─────────────────────────────────────┘
                     │ UART (921600 baud)
┌────────────────────┼─────────────────────────────────────┐
│                    │         CC1352P                     │
│  ┌─────────────────▼──────────────────────────────────┐ │
│  │          Command Processor                         │ │
│  │  - COBS framing                                   │ │
│  │  - Command dispatcher                             │ │
│  │  - Response formatter                             │ │
│  └─────────────────┬──────────────────────────────────┘ │
│                    │                                     │
│  ┌─────────────────▼──────────────────────────────────┐ │
│  │          Radio Abstraction Layer                   │ │
│  │  - PHY Manager (BLE/Zigbee/Sub-1GHz)             │ │
│  │  - TX/RX Engine                                   │ │
│  │  - Jamming Engine                                 │ │
│  │  - Spectrum Analyzer                              │ │
│  │  - Autonomous Policy Engine                       │ │
│  └─────────────────┬──────────────────────────────────┘ │
│                    │                                     │
│  ┌─────────────────▼──────────────────────────────────┐ │
│  │     TI Driverlib + RF Core                        │ │
│  │  - RF patches (BLE5, IEEE 802.15.4)              │ │
│  │  - SmartRF configs                                │ │
│  │  - Cortex-M0+ RF Core firmware                   │ │
│  └───────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────┘
---

## 2. STACK TECNOLÓGICO DEFINITIVO

### Firmware CC1352
- SDK: TI SimpleLink CC13xx/CC26xx SDK 7.10.01.24 (versión fija)
- Compiler: ARM GCC 10.3 (arm-none-eabi-gcc)
- Build: CMake 3.20+ (NO CCS)
- RTOS: TI-RTOS 7 (incluido en SDK, opcional FreeRTOS migration)
- Bootloader: Custom bootloader (8KB) + TI ROM bootloader fallback

### Python API
- Core: Python 3.9+
- Serial: pyserial-asyncio
- Protocol: custom COBS + struct
- Optional: msgpack (si queremos JSON-like para debugging)
- Testing: pytest, pytest-asyncio

### Build & CI/CD
- CMake: Cross-platform build
- GitHub Actions: 
  - Ubuntu runner para ARM GCC
  - Windows/Mac runners para Python wheels
- Artifacts: 
  - .hex, .bin, .elf del firmware
  - Python wheel .whl
### Testing
- Unit: Unity (C), pytest (Python)
- Mocking: CMock para RF HAL
- Coverage: gcov/lcov (C), pytest-cov (Python)
- HW Testing: Manual con segundo CatSniffer + spectrum analyzer

---

## 3. ESTRUCTURA DEL PROYECTO

copy


catsniffer-universal/
├── firmware/
│   ├── CMakeLists.txt                    # Build root
│   ├── sdk/
│   │   └── simplelink_cc13xx_cc26xx_sdk_7_10_01_24/  # Submodule o descargado
│   ├── bootloader/
│   │   ├── CMakeLists.txt
│   │   ├── src/
│   │   │   ├── main.c
│   │   │   ├── uart_update.c
│   │   │   └── flash.c
│   │   └── linker/cc1352p_bootloader.ld
│   ├── application/
│   │   ├── CMakeLists.txt
│   │   ├── src/
│   │   │   ├── main.c
│   │   │   ├── command_processor.c       # COBS + dispatcher
│   │   │   ├── radio_hal.c               # Abstraction layer
│   │   │   ├── phy_manager.c             # BLE/Zigbee/Sub-1GHz switching
│   │   │   ├── tx_engine.c
│   │   │   ├── rx_engine.c
│   │   │   ├── jam_engine.c
│   │   │   ├── spectrum.c
│   │   │   ├── policy_engine.c           # Autonomous modes
│   │   │   └── uart.c
│   │   ├── include/
│   │   │   ├── protocol.h                # Command/response structs
│   │   │   ├── radio_hal.h
│   │   │   └── config.h
│   │   ├── smartrf_settings/             # From SmartRF Studio
│   │   │   ├── ble_settings.c
│   │   │   ├── ieee802154_settings.c
│   │   │   └── subghz_settings.c
│   │   └── linker/cc1352p_app.ld
│   ├── tests/
│   │   ├── unit/
│   │   │   ├── test_command_processor.c
│   │   │   ├── test_protocol.c
│   │   │   └── mocks/
│   │   └── integration/                   # Manual HW tests
│   └── tools/
│       ├── flash.sh
│       └── debug_gdb.sh
│
├── python/
│   ├── pyproject.toml                     # PEP 518 build
│   ├── setup.py
│   ├── catsniffer/
│   │   ├── __init__.py
│   │   ├── radio.py                       # Main Radio class
│   │   ├── protocol.py                    # COBS codec + structs
│   │   ├── commands.py                    # Command builders
│   │   ├── responses.py                   # Response parsers
│   │   ├── enums.py                       # Modulation, channels, etc.
│   │   ├── jamming.py                     # JammingPolicy class
│   │   ├── spectrum.py                    # Spectrum utilities
│   │   └── exceptions.py
│   ├── examples/
│   │   ├── ble_sniffer.py
│   │   ├── zigbee_jam.py
│   │   ├── spectrum_scan.py
│   │   └── reactive_jam_demo.py
│   ├── tests/
│   │   ├── test_protocol.py
│   │   ├── test_radio.py
│   │   └── test_integration.py            # Requires hardware
│   └── docs/
│       ├── api.md
│       └── examples.md
│
├── .github/
│   └── workflows/
│       ├── build_firmware.yml             # Build + artifacts
│       ├── test_python.yml
│       ├── release.yml                    # Tag → GitHub release
│       └── docs.yml                       # Sphinx/MkDocs
│
├── docs/
│   ├── hardware.md                        # Pinout, schematics reference
│   ├── protocol.md                        # Binary protocol spec
│   ├── regulatory.md                      # Legal warnings
│   ├── firmware_dev.md
│   └── python_dev.md
│
├── LICENSE
├── README.md
└── CONTRIBUTING.md
---

## 4. PROTOCOLO DE COMUNICACIÓN DETALLADO

### Framing: COBS (Consistent Overhead Byte Stuffing)

copy


Frame format:
┌──────┬────────┬────────┬─────────────┬─────────┬──────┐
│ 0x00 │ LENGTH │ CMD_ID │   PAYLOAD   │ CRC16   │ 0x00 │
│(SYNC)│ (1B)   │ (1B)   │  (0-255B)   │ (2B BE) │(SYNC)│
└──────┴────────┴────────┴─────────────┴─────────┴──────┘
       └────────── COBS encoded ──────────┘

COBS removes all 0x00 bytes from the payload, allowing 0x00 as frame delimiter.
CRC16: CRC-16-CCITT over LENGTH, CMD_ID, PAYLOAD
### Command IDs (MVP + Extended)

`c
// ============= Configuration =============
#define CMD_RADIO_INIT          0x01  // Initialize radio subsystem
#define CMD_SET_CHANNEL         0x02  // Set channel (protocol-dependent)
#define CMD_SET_POWER           0x03  // TX power in dBm
#define CMD_SET_PHY             0x04  // Switch PHY (BLE/Zigbee/Sub-1GHz)
#define CMD_GET_INFO            0x05  // Firmware version, capabilities

// ============= RX Operations =============
#define CMD_RX_START            0x10  // Start receiving
#define CMD_RX_STOP             0x11  // Stop receiving
#define CMD_RX_SET_FILTER       0x12  // Address/CRC filtering
#define CMD_RX_SET_PROMISCUOUS  0x13  // Promiscuous mode

// ============= TX Operations =============
#define CMD_TX_RAW              0x20  // Transmit raw packet
#define CMD_TX_CONTINUOUS       0x21  // Continuous TX (carrier/pattern)
#define CMD_TX_BURST            0x22  // Burst N packets

// ============= Jamming =============
#define CMD_JAM_CONTINUOUS      0x30  // CW jamming
#define CMD_JAM_REACTIVE        0x31  // React to RX (fast)
#define CMD_JAM_PATTERN         0x32  // Custom pattern
#define CMD_JAM_STOP            0x33  // Stop jamming

// ============= Spectrum Analysis =============
#define CMD_SPECTRUM_SCAN       0x40  // Sweep frequency range
#define CMD_SPECTRUM_MONITOR    0x41  // Continuous monitoring
#define CMD_SPECTRUM_STOP       0x42  // Stop monitoring

// ============= Autonomous Policies =============
#define CMD_POLICY_SET          0x50  // Configure policy
#define CMD_POLICY_START        0x51  // Start autonomous mode
#define CMD_POLICY_STOP         0x52  // Stop autonomous mode
#define CMD_POLICY_GET          0x53  // Read policy config

// ============= Status/Debug =============
#define CMD_GET_STATUS          0x60  // Radio status
#define CMD_GET_STATS           0x61  // Counters (RX/TX/jammed)
#define CMD_RESET               0x62  // Soft reset
#define CMD_GET_RSSI            0x63  // Current RSSI

// ============= Bootloader =============
#define CMD_ENTER_BOOTLOADER    0xF0  // Jump to bootloader
#define CMD_BOOTLOADER_VERSION  0xF1  // Get bootloader info

// ============= Responses (0x80 bit) =============
#define RSP_ACK                 0x80  // Success
#define RSP_ERROR               0x81  // Error with code
#define RSP_RX_PACKET           0x90  // Received packet
#define RSP_SPECTRUM_DATA       0x91  // Spectrum data
#define RSP_STATUS              0x92  // Status info
#define RSP_STATS               0x93  // Statistics
#define RSP_INFO                0x94  // Device info
#define RSP_JAM_EVENT           0x95  // Jamming triggered event
copy



### **Ejemplo de comandos desglosados:**

#### **CMD_SET_PHY (0x04)**
c
Payload:
  uint8_t  phy_type;       // 0=BLE_5_1M, 1=BLE_5_2M, 2=IEEE_802_15_4, 3=SUB_1GHZ
  uint8_t  channel;        // PHY-specific channel
  uint32_t frequency_hz;   // For Sub-1GHz custom freq (optional)

Response: RSP_ACK or RSP_ERROR
copy



#### **CMD_TX_RAW (0x20)**
c
Payload:
  uint8_t  packet_length;  // 1-255 bytes
  uint8_t  packet[];       // Raw packet data
  int8_t   power_dbm;      // TX power override (optional, -128 = use default)

Response: RSP_ACK with TX timestamp
copy



#### **CMD_JAM_REACTIVE (0x31)**
c
Payload:
  uint8_t  trigger_type;   // 0=PREAMBLE, 1=SYNC_WORD, 2=ADDRESS_MATCH
  uint8_t  trigger_value[8]; // Match pattern (e.g., sync word)
  uint8_t  trigger_mask[8];  // Mask for pattern
  uint16_t jam_duration_us;  // Jam duration in microseconds
  uint8_t  jam_type;         // 0=CW, 1=PATTERN, 2=PACKET
  uint8_t  jam_data[];       // Pattern or packet data (if jam_type > 0)

Response: RSP_ACK
Events: RSP_JAM_EVENT when triggered
copy



#### **CMD_SPECTRUM_SCAN (0x40)**
c
Payload:
  uint32_t start_freq_hz;
  uint32_t end_freq_hz;
  uint16_t step_khz;
  uint8_t  samples_per_freq; // Averaging
  uint8_t  dwell_time_ms;    // Time per frequency

Response: Stream of RSP_SPECTRUM_DATA
Final: RSP_ACK when complete
`
#### RSP_RX_PACKET (0x90)
C


Payload:
  uint64_t timestamp_us;   // RX timestamp (from radio timer)
  uint8_t  channel;
  int8_t   rssi_dbm;
  uint8_t  lqi;            // Link Quality Indicator
  uint8_t  crc_ok;         // 0=fail, 1=pass
  uint8_t  packet_length;
  uint8_t  packet[];
---

## 5. FASES DE DESARROLLO

### FASE 0: Setup y Skeleton (Semana 1)

Entregables:
- ✅ Repo GitHub con estructura
- ✅ CMake compilando hello world en CC1352
- ✅ GitHub Actions compilando firmware
- ✅ Python package estructura básica
- ✅ UART bridge funcionando con echo test

Tareas:
1. Crear repo, configurar .gitignore
2. Descargar TI SDK 7.10.01.24, crear CMakeLists.txt
3. Blinky LED en CC1352 con CMake
4. UART echo test (CC1352 ↔ RP2040 ↔ PC)
5. Python script que envía/recibe bytes

---

### FASE 1: MVP - BLE Sniffer (Semanas 2-3)

Objetivo: Replicar funcionalidad de SmartRF Packet Sniffer pero con Python API.

Entregables:
- ✅ BLE RX funcionando (advertising packets)
- ✅ Protocolo COBS implementado
- ✅ Comandos: INIT, SET_PHY, SET_CHANNEL, RX_START, RX_STOP
- ✅ Python API básica (sync)
- ✅ Ejemplo: ble_sniffer.py

Firmware:
- Command processor con COBS framing
- BLE PHY initialization (SmartRF Studio config)
- RX callback → UART streaming
- CRC16 implementation

Python:
Python


from catsniffer import Radio, PHY

radio = Radio('/dev/ttyUSB0')
radio.init()
radio.set_phy(PHY.BLE_1M)
radio.set_channel(37)  # BLE advertising channel

radio.start_rx()
for packet in radio.read_packets(timeout=10):
    print(f"RSSI: {packet.rssi} | Data: {packet.data.hex()}")
radio.stop_rx()
---

### FASE 2: TX + Jamming Básico (Semanas 4-5)

Entregables:
- ✅ TX raw packets (BLE)
- ✅ Jamming continuo (CW)
- ✅ Comandos: TX_RAW, JAM_CONTINUOUS, JAM_STOP
- ✅ Regulatory warnings en Python API

Firmware:
- TX engine con timing control
- CW generation usando RF Core
- Power control (-20 a +20 dBm)

Python:
Python


# Transmit BLE advertising packet
radio.set_phy(PHY.BLE_1M)
radio.set_channel(37)
radio.transmit(packet=b'\x42\x04\x01\x02\x03\x04', power_dbm=0)

# Jam BLE channel 37
radio.jam_continuous(channel=37, duration_ms=5000, power_dbm=10)
---

### FASE 3: Spectrum Analyzer (Semana 6)

Entregables:
- ✅ Spectrum scan (2.4 GHz band)
- ✅ RSSI measurements
- ✅ Python visualization (matplotlib)

Firmware:
- Frequency hopping loop
- RSSI sampling via RF Core
- Streaming results via UART

Python:
Python


spectrum = radio.spectrum_scan(
    start_freq=2400_000_000,  # 2.4 GHz
    end_freq=2480_000_000,
    step_khz=1000,
    samples=10
)

# spectrum = [(freq_hz, rssi_dbm), ...]
import matplotlib.pyplot as plt
plt.plot([f/1e6 for f, _ in spectrum], [r for _, r in spectrum])
plt.xlabel('Frequency (MHz)')
plt.ylabel('RSSI (dBm)')
plt.show()
---

### FASE 4: Zigbee + Multi-PHY (Semanas 7-8)

Entregables:
- ✅ IEEE 802.15.4 RX/TX (Zigbee)
- ✅ PHY switching (BLE ↔ Zigbee)
- ✅ Channel translation (BLE ch 37 → 2402 MHz, Zigbee ch 11 → 2405 MHz)

Firmware:
- PHY manager con configs de SmartRF Studio
- RF Core mode switching
- Address filtering para Zigbee

---

### FASE 5: Reactive Jamming + Autonomous (Semanas 9-10)

Entregables:
- ✅ Reactive jamming (trigger on packet detection)
- ✅ Policy engine (autonomous modes)
- ✅ <500µs reaction time
- ✅ Event streaming (RSP_JAM_EVENT)

Firmware:
- RX callback con state machine
- Fast TX trigger (sin consultar host)
- Policies en RAM (pre-configuradas)

Python:
Python


from catsniffer import JammingPolicy, Trigger

# Configure autonomous reactive jamming
policy = JammingPolicy(
    trigger=Trigger.ZIGBEE_ACK,
    action="jam_cw",
    duration_us=500,
    channel=15,
    power_dbm=10
)

radio.set_policy(policy)
radio.start_autonomous()

# Receive events
async for event in radio.events():
    if event.type == "jam_triggered":
        print(f"Jammed ACK at {event.timestamp_us} µs")
---

### FASE 6: Sub-1GHz + Testing (Semanas 11-12)
Entregables:
- ✅ Sub-1GHz PHY (868/915 MHz)
- ✅ Unit tests (>80% coverage)
- ✅ Integration test suite
- ✅ Documentation completa

---

### FASE 7: Bootloader + Release (Semanas 13-14)

Entregables:
- ✅ Custom bootloader
- ✅ OTA firmware update via Python
- ✅ GitHub release con binarios
- ✅ PyPI package publicado

---

## 6. SNIFFING SIMULTÁNEO - ACLARACIÓN

Pregunta 3: ¿Se puede sniffing simultáneo de múltiples protocolos?

Respuesta corta: No literalmente al mismo tiempo, pero SÍ con time-slicing rápido.

Explicación:

El CC1352 tiene un solo RF Core → no puede recibir en 2 frecuencias/modulaciones simultáneamente.

PERO puedes:

### Opción A: Time-Slicing (muy rápido)
Python


# El firmware alterna entre PHYs rápidamente
radio.enable_multi_sniff([
    (PHY.BLE_1M, channel=37, duration_ms=50),
    (PHY.ZIGBEE, channel=15, duration_ms=50),
])

# Internamente:
# 0-50ms: BLE channel 37
# 50-100ms: Zigbee channel 15
# 100-150ms: BLE channel 37
# ...

# Resultado: Capturas ambos pero con gaps de ~50ms
Latencia de switching: ~2ms (re-configurar RF Core)

Caso de uso:
- Monitorear BLE advertising (intermitente) + Zigbee data (también intermitente)
- Acceptable para análisis de red, NO para sniffer de alta fidelidad

### Opción B: Dual CatSniffer (recomendado para producción)
- Un CatSniffer por protocolo
- Python API con multi-device support:
Python


ble_radio = Radio('/dev/ttyUSB0')
zigbee_radio = Radio('/dev/ttyUSB1')
Decisión para MVP:
- FASE 1-5: Solo un protocolo a la vez (más simple)
- FASE 6 (opcional): Agregar time-slicing experimental

¿Te parece bien?

---

## 7. TESTING STRATEGY DETALLADO

### Unit Tests (CI/CD)

Firmware (C):
C


// tests/unit/test_command_processor.c
void test_parse_set_channel(void) {
    uint8_t frame[] = {0x00, 0x02, CMD_SET_CHANNEL, 0x0F, 0xAB, 0xCD, 0x00};
    command_t cmd;
    
    int result = parse_command(frame, sizeof(frame), &cmd);
    
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(CMD_SET_CHANNEL, cmd.id);
    TEST_ASSERT_EQUAL(15, cmd.payload[0]);
}

// Mock del RF HAL
void radio_set_channel(uint8_t ch) {
    // Mocked - no hardware access
}
Python:
Python


# tests/test_protocol.py
def test_cobs_encode():
    data = bytes([0x01, 0x00, 0x02])
    encoded = cobs_encode(data)
    assert 0x00 not in encoded[1:-1]  # No zeros except delimiters
    
def test_command_build():
    cmd = SetChannelCommand(channel=15)
    frame = cmd.to_bytes()
    assert frame[2] == 0x02  # CMD_SET_CHANNEL
### Integration Tests (Manual con HW)

Python


# tests/test_integration.py
@pytest.mark.hardware
def test_ble_sniffer_receive():
    radio = Radio('/dev/ttyUSB0')
    radio.set_phy(PHY.BLE_1M)
    radio.set_channel(37)
    radio.start_rx()
    
    # Usar otro CatSniffer o phone BLE para transmitir
    packets = []
    for pkt in radio.read_packets(timeout=5):
        packets.append(pkt)
        if len(packets) >= 10:
            break
    
    assert len(packets) > 0
    assert all(pkt.rssi < 0 for pkt in packets)
### RF Validation (Manual)

Checklist con spectrum analyzer:
- [ ] BLE 1M TX en 2402 MHz (canal 37)
- [ ] Zigbee TX en 2405 MHz (canal 11)
- [ ] Jamming CW potencia medida (-20 a +20 dBm)
- [ ] Spectrum scan accuracy (comparar con SA)

---

## 8. CI/CD PIPELINE

### **GitHub Actions: .github/workflows/build_firmware.yml**

`yaml
name: Build Firmware

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Cache TI SDK
        uses: actions/cache@v3
        with:
          path: firmware/sdk
          key: ti-sdk-7.10.01.24
      
      - name: Download TI SDK
        if: steps.cache.outputs.cache-hit != 'true'
run: |
          wget https://dr-download.ti.com/...simplelink_cc13xx_cc26xx_sdk_7_10_01_24.run
          chmod +x *.run
          ./simplelink_*.run --mode unattended --prefix ./firmware/sdk
      
      - name: Install ARM GCC
        run: |
          sudo apt-get update
          sudo apt-get install gcc-arm-none-eabi
      
      - name: Build Firmware
        run: |
          cd firmware
          mkdir build && cd build
          cmake ..
          make -j$(nproc)
      
      - name: Run Unit Tests
        run: |
          cd firmware/build
          ctest --output-on-failure
      
      - name: Upload Artifacts
        uses: actions/upload-artifact@v3
        with:
          name: firmware-binaries
          path: |
            firmware/build/*.hex
            firmware/build/*.bin
            firmware/build/*.elf
copy



### **Release workflow**

yaml
name: Release

on:
  push:
    tags:
      - 'v*'

jobs:
  release:
    runs-on: ubuntu-latest
    steps:
      # ... build firmware y Python wheel ...
      
      - name: Create Release
        uses: softprops/action-gh-release@v1
        with:
          files: |
            firmware/build/catsniffer_universal_v*.hex
            python/dist/*.whl
          body: |
            ## CatSniffer Universal Firmware ${{ github.ref_name }}
            
        WARNINGNG**: This firmware enables RF jamming. Use only in authorized environments.
            
            ### Features
            - BLE 5.x sniffer/TX
            - Zigbee (IEEE 802.15.4) sniffer/TX/jam
            - Spectrum analyzer (2.4 GHz)
            - Reactive jamming (<500µs latency)
`

---
9. PUNTOS CRÍTICOS Y RIESGOSOS**

⚠️ RIESGO ALTOTO1. Latencia de jamming reactiveve*Meta:a:** <500µs desde RX detection → TX jaMitigación:n:** 
  - Autonomous mode (no host involvement)
  - Pre-cargar TX buffer en RF Core
  - Test con oscillosco2. TI SDK breaking changeses*Mitigación:n:**
  - Fijar versión del SDK (7.10.01.24)
  - Versionas configs de SmartRF Studio
  - Documentar cómo regenerar si se necesita nueva versi3. Regulatory compliancece*Mitigación:n:**
  - Warnings explícitos en README/docs
  - Potencia limitada por defecto (0 dBm)
  - Require `--enable-jamming` flag en Python

⚠️ RIESGO MEDIOIO4. UART buffer overflow en RX intensoso*Mitigación:n:**
  - Flow control (RTS/CTS) si el RP2040 lo soporta
  - Rate limiting en firmware
  - Ring buffer grande (4KB5. RF Core crasheses*Mitigación:n:**
  - Watchdog timer
  - Auto-recovery en command processor
  - Logging detallado

⚠️ RIESGO BAJOJO6. Python compatibility (3.9 vs 3.12)2)*Mitigación:n:** Tox testing multi-versión

---
10. ROADMAP POST-MVPVPV2.0 (Q3 2025):):**
- Sub-1GHz PHYs (868/915 MHz)
- BLE 5.2 coded PHY (long range)
- Sniffer multi-protocol con time-sliciV3.0 (Q4 2025):):**
- GUI (Electron/Tauri) para visualización
- Wireshark dissector plugin
- Cloud telemetry (opcional)

---
11. DECISIONES PENDIENTESES**

Antes de empezar necesito que confirmes:

A. Hardware - RP2040 ↔ CC135252**
1. ¿El RP2040 tiSWDWD** conectado al CC1352? (SWDIO, SWCLK)
 Si SÍ:Í:** Podemos hacer bootloader muy robusto
 Si NO:O:** Limitados a TI ROM bootloader via UART

2. ¿RESETET** pin del CC1352 controlable por RP2040?
   - Para recovery automático

3. ¿UART tiflow controlol** (RTS/CTS)?

B. Alcance del MVPVP**
1. ¿FASE 5 (reactive jamming) es crítico para MVP o puede ser V1.1?
 Si crítico:o:** MVP = 10 semanas
 Si NO:O:** MVP = 6 semanas (hasta spectrum analyzer)

2. ¿Necesitas GUI o solo Python CLI/API?

C. Licenciaia**Firmware:e:** GPL-3.0 / Apache-2.0 / MIT?Python API:I:** Misma licencia?
3. ¿Open source público o privado inicialmente?

---
12. PRÓXIMOS PASOS SI APRUEBAS EL PLANAN**
Te genero el skeleton completoto** (estructura + CMakeLists + Python package)Documentación inicialal** (README, protocol.md)Primera PR:R:** FASE 0 completa (blinky + UART echo)Establecemos milestoneses** en GitHub Projects
