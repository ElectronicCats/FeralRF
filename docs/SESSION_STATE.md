# FeralRF - Estado de Sesión

## Última actualización: 2025-02-16

## FASE 0: Completada ✅

### Builds funcionando:
- **RP2040**: `firmware/rp2040/build/feralrf_rp2040.uf2` (46KB)
- **CC1352**: `firmware/cc1352/build/feralrf_cc1352.elf`
  - text: 488 bytes (con DriverLib)
  - data: 0 bytes
  - bss: 4096 bytes
  - Total: 4584 bytes

### Python:
- Tests: 13/13 passing
- venv en: `python/.venv/`
- Deps: pyserial, pyserial-asyncio, cobs

### SDKs instalados:
- TI SDK 7.10.01.24: `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_7_10_01_24/` (git submodule)
- Pico SDK 2.0.0: `firmware/sdk/pico-sdk/`

### Pre-commit configurado:
- clang-format (C)
- cppcheck (C static analysis)
- black/isort/flake8/mypy (Python)
- CMake build check
- Ver: `.pre-commit-config.yaml`

## FASE 1: En Progreso 🔄

### Completado:
- [x] COBS + CRC16 en C (`firmware/cc1352/src/protocol.c`)
- [x] DriverLib integration (GPIO, PRCM, IOC)
- [x] LED blinky funcional en DIO24

### Pendiente:
- [ ] UART driver con ring buffer (921600)
- [ ] Command processor funcional
- [ ] BLE PHY initialization con TI SDK
- [ ] RX streaming vía UART
- [ ] Python API testing con HW
- [ ] Ejemplo: ble_sniffer.py

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

# Run pre-commit
pre-commit run --all-files
```

## Hardware Pinout:
- UART: RP2040 GPIO0/1 → CC1352 DIO12/13
- RTS/CTS: GPIO2/3 → DIO14/15
- RESET_CC: GPIO15
- LEDs: GPIO26/27/28 (active low)
- **LED CC1352**: DIO24 (CatSniffer v3)

## Decisiones tomadas:
- Licencia: GPL-3.0
- SDK TI: 7.10.01.24 (fijo, git submodule)
- Reactive jamming en MVP: Sí
- Repositorio: Privado
- DriverLib: Pre-compiled GCC library from SDK

## Referencias temporales:
- `examples-catsniffer/` - Ejemplos de CatSniffer-Firmware para referencia
