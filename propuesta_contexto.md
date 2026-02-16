# Contexto y Documentación para IA - FeralRF

Si quieres que la IA genere código de radio funcional, estos son los recursos necesarios:

---

## 1. SDK y Documentación TI

| Recurso | Descripción | Ubicación |
|---------|-------------|-----------|
| TI SimpleLink SDK 7.10.01.24 | SDK principal para CC1352 | `firmware/sdk/` (descargar manualmente) |
| CC13x2/CC26x2 Technical Reference Manual | Capítulo 25 (Radio) - Registros y comandos del Radio Core | [TI Website](https://www.ti.com/lit/ug/swcu185/swcu185.pdf) |
| TI Driverlib RF API | RF_open, RF_postCmd, RF_runCmd, RF_CmdHandle | Incluido en SDK |
| TI-RTOS 7 Documentation | Task creation, queues, semaphores | Incluido en SDK |

---

## 2. Stack de Protocolos

### BLE 5.x
- Advertising packets (legacy + extended)
- PHY: 1M, 2M, Coded (S8/S2)
- Channels: 37, 38, 39 (advertising), 0-36 (data)

### IEEE 802.15.4 (Zigbee)
- Frame format: preamble, SFD, length, payload, FCS
- Channels: 11-26 (2.4 GHz)
- Addressing modes: short, extended

### Sub-1GHz Proprietary
- 868 MHz band (EU)
- 915 MHz band (US/AU)
- GFSK modulation configurable

---

## 3. Hardware CatSniffer - Pinout

Ver archivo completo: `hardware/PINOUT.md`

### UART RP2040 ↔ CC1352 (3Mbps)
```
RP2040 TX  (GPIO0)  → CC1352 DIO12 (RX)
RP2040 RX  (GPIO1)  ← CC1352 DIO13 (TX)
RP2040 RTS (GPIO2)  → CC1352 DIO14
RP2040 CTS (GPIO3)  ← CC1352 DIO15
```

### Control
```
RP2040 GPIO15 → CC1352 RESET_N
```

### LEDs (Active Low)
```
LED1 = GPIO28
LED2 = GPIO27
LED3 = GPIO26
```

### CC1352 Radio
- 2.4 GHz: RF_P (Pin 32), RF_N (Pin 33)
- Sub-1 GHz: RF_SUB1_P (Pin 45), RF_SUB1_N (Pin 46)

### JTAG (Default CC1352)
- TMSC: DIO16
- TCK: DIO17

---

## 4. SmartRF Settings

Generar con SmartRF Studio desde TI:
- `ble_1m_settings.c/h` - BLE 1M PHY
- `ble_2m_settings.c/h` - BLE 2M PHY
- `ieee802154_settings.c/h` - Zigbee
- `sub1ghz_868_settings.c/h` - 868 MHz
- `sub1ghz_915_settings.c/h` - 915 MHz

Ubicación: `firmware/cc1352/smartrf_settings/`

---

## 5. Protocolo de Comunicación

Ver: `PLAN_MAESTRO.md` Sección 3

- Framing: COBS (0x00 como delimitador)
- CRC: CRC-16-CCITT
- Campos: CMD_ID (1B) + SEQ (1B) + LEN (2B) + PAYLOAD + CRC16 (2B)

---

## 6. Librerías de Soporte

| Librería | Uso | Documentación |
|----------|-----|---------------|
| TI-RTOS 7 | Tasks, queues, sync primitives | Incluido en SDK |
| COBS | Framing protocol | Implementación propia |
| TinyUSB | USB-CDC en RP2040 | Pico SDK |

---

## 7. Descarga de Recursos

### TI SDK (manual)
```bash
# Descargar desde TI (requiere cuenta)
# https://www.ti.com/tool/download/SIMPLELINK-CC13XX-CC26XX-SDK

# Extraer en:
firmware/sdk/simplelink_cc13xx_cc26xx_sdk_7_10_01_24/
```

### SmartRF Studio
```bash
# Descargar desde TI
# https://www.ti.com/tool/SMARTRFTM-STUDIO

# Generar settings para cada PHY y exportar como .c/.h
```

---

## 8. Archivos de Referencia en Este Repo

| Archivo | Contenido |
|---------|-----------|
| `PLAN_MAESTRO.md` | Arquitectura, fases, protocolo, decisiones |
| `hardware/PINOUT.md` | Pinout completo del CatSniffer |
| `hardware/CatSniffer.kicad_sch` | Esquemático fuente (KiCad) |
| `hardware/CatSniffer.pdf` | Esquemático en PDF |
| `CLAUDE.md` | Guía para Claude Code |
