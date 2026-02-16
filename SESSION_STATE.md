# FeralRF - Estado de Sesión

## Última actualización: 2024-02-16

## FASE 0: Completada ✅

### Builds funcionando:
- **RP2040**: `firmware/rp2040/build/feralrf_rp2040.uf2` (46KB)
- **CC1352**: `firmware/cc1352/build/feralrf_cc1352.elf` (392 bytes code)

### Python:
- Tests: 13/13 passing
- venv en: `python/.venv/`
- Deps: pyserial, pyserial-asyncio, cobs

### SDKs instalados:
- TI SDK 7.10.01.24: `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_7_10_01_24/`
- Pico SDK 2.0.0: `firmware/sdk/pico-sdk/`

## Próximo paso: FASE 1 - BLE Sniffer MVP

Tareas:
1. COBS implementation en C (CC1352)
2. Command processor funcional
3. BLE PHY initialization con TI SDK
4. RX streaming vía UART
5. Python API básica (sync)
6. Ejemplo: ble_sniffer.py

## Comandos útiles:

```bash
# Activar Python env
cd python && source .venv/bin/activate

# Build RP2040
cd firmware/rp2040/build && cmake .. && make

# Build CC1352
cd firmware/cc1352/build && cmake .. && make

# Run Python tests
cd python && pytest -v
```

## Hardware Pinout:
- UART: RP2040 GPIO0/1 → CC1352 DIO12/13
- RTS/CTS: GPIO2/3 → DIO14/15
- RESET_CC: GPIO15
- LEDs: GPIO26/27/28 (active low)

## Decisiones tomadas:
- Licencia: GPL-3.0
- SDK TI: 7.10.01.24 (fijo)
- Reactive jamming en MVP: Sí
- Repositorio: Privado
