Plan to implement                                                                                  │
│                                                                                                    │
│ Plan Fase 1: BLE Sniffer MVP - Implementacion Incremental                                          │
│                                                                                                    │
│ Contexto                                                                                           │
│                                                                                                    │
│ El firmware CC1352 actual (Fase 0) es un blinky bare-metal que no esta correctamente configurado   │
│ para funcionar en hardware real:                                                                   │
│                                                                                                    │
│ - No tiene CCFG (Customer Configuration): sin esto el CC1352 no arranca correctamente              │
│ - DriverLib no esta linkeada: la variable DRIVERLIB_LIB se define en CMakeLists.txt pero nunca se  │
│ usa con target_link_libraries()                                                                    │
│ - La tabla de vectores esta incompleta: solo tiene 16 entries (Cortex-M4 necesita 16 +             │
│ perifericos)                                                                                       │
│ - main.c usa registros hardcodeados: funcional pero fragil                                         │
│ - No hay float ABI correcta: el CMake usa -mfloat-abi=soft pero las libs del SDK (RF driver)       │
│ requieren -mfloat-abi=hard                                                                         │
│                                                                                                    │
│ El plan es incremental: primero hacer que el blink funcione de verdad en hardware, validarlo, y    │
│ luego avanzar paso a paso hacia el BLE sniffer.                                                    │
│                                                                                                    │
│ ---                                                                                                │
│ Checkpoint 1: Blink Funcional en Hardware (PRIMER OBJETIVO)                                        │
│                                                                                                    │
│ 1.1 Agregar CCFG (Customer Configuration)                                                          │
│                                                                                                    │
│ Archivo nuevo: firmware/cc1352/ccfg.c                                                              │
│                                                                                                    │
│ El CCFG es una estructura de configuracion que el bootloader del CC1352 lee al arrancar. Sin ella, │
│  el chip puede no arrancar o comportarse de forma erratica. Necesitamos:                           │
│                                                                                                    │
│ - Configurar XOSC (cristal externo) para clock preciso                                             │
│ - Habilitar bootloader backdoor (para poder re-flashear)                                           │
│ - Configurar el modo de arranque                                                                   │
│                                                                                                    │
│ Se usa el mecanismo del SDK: definir macros SET_CCFG_* antes de incluir el ccfg.c del SDK.         │
│                                                                                                    │
│ 1.2 Corregir CMakeLists.txt                                                                        │
│                                                                                                    │
│ Archivo: firmware/cc1352/CMakeLists.txt                                                            │
│                                                                                                    │
│ Cambios necesarios:                                                                                │
│ - Cambiar -mfloat-abi=soft a -mfloat-abi=hard -mfpu=fpv4-sp-d16 (CC1352P tiene FPU)                │
│ - Agregar -DDeviceFamily_CC13X2 como compile definition                                            │
│ - Agregar target_link_libraries() con driverlib.lib                                                │
│ - Agregar ccfg.c a la lista de sources                                                             │
│                                                                                                    │
│ 1.3 Corregir linker script                                                                         │
│                                                                                                    │
│ Archivo: firmware/cc1352/linker/cc1352p.ld                                                         │
│                                                                                                    │
│ Cambios necesarios:                                                                                │
│ - Agregar seccion .ccfg en la direccion fija 0x57FA8 (donde el bootloader espera encontrarla)      │
│ - Es critico: sin la seccion CCFG en la direccion correcta, el chip no arranca                     │
│                                                                                                    │
│ 1.4 Validar main.c                                                                                 │
│                                                                                                    │
│ Archivo: firmware/cc1352/src/main.c                                                                │
│                                                                                                    │
│ El codigo actual deberia funcionar con los fixes anteriores. Se puede verificar que:               │
│ - El GPIO clock se habilita correctamente via PRCM                                                 │
│ - DIO24 (LED) parpadea visiblemente                                                                │
│                                                                                                    │
│ Verificacion Checkpoint 1                                                                          │
│                                                                                                    │
│ 1. cd firmware/cc1352/build && cmake .. && make - debe compilar sin errores                        │
│ 2. Flashear al CC1352 via JTAG/bootloader                                                          │
│ 3. El LED en DIO24 debe parpadear - esto confirma:                                                 │
│   - CCFG correcto (el chip arranco)                                                                │
│   - Clock configurado (PRCM funciona)                                                              │
│   - GPIO configurado (IOC + GPIO funcionan)                                                        │
│   - Linker script correcto (codigo ejecuta desde flash)                                            │
│                                                                                                    │
│ ---                                                                                                │
│ Checkpoint 2: UART Echo (post-validacion del blink)                                                │
│                                                                                                    │
│ 2.1 UART driver basico                                                                             │
│                                                                                                    │
│ Archivo nuevo: firmware/cc1352/src/uart_drv.c + include/uart_drv.h                                 │
│                                                                                                    │
│ UART con DriverLib (driverlib/uart.h) a 3Mbps en DIO12/DIO13 con RTS/CTS en DIO14/DIO15:           │
│ - Ring buffer RX de 16KB y TX de 4KB (estaticos)                                                   │
│ - Interrupciones para RX (no polling)                                                              │
│ - uart_init(), uart_read(), uart_write()                                                           │
│                                                                                                    │
│ 2.2 Actualizar startup con vector UART                                                             │
│                                                                                                    │
│ Archivo: firmware/cc1352/src/startup_cc13x2_cc26x2_gcc.c                                           │
│                                                                                                    │
│ Extender tabla de vectores a 54 entries (CC13x2 completo) incluyendo UART0 (IRQ 5).                │
│                                                                                                    │
│ Verificacion Checkpoint 2                                                                          │
│                                                                                                    │
│ 1. Flashear firmware                                                                               │
│ 2. Conectar CatSniffer via USB (RP2040 actua como bridge)                                          │
│ 3. Enviar bytes por serial a 3Mbps → deben regresar (echo)                                         │
│                                                                                                    │
│ ---                                                                                                │
│ Checkpoint 3: NoRTOS + RF_open (post-UART)                                                         │
│                                                                                                    │
│ 3.1 Integrar NoRTOS DPL                                                                            │
│                                                                                                    │
│ Compilar los fuentes del SDK de kernel/nortos/dpl/:                                                │
│ - HwiPCC26XX_nortos.c, SwiP_nortos.c, SemaphoreP_nortos.c                                          │
│ - ClockPSysTick_nortos.c, MutexP_nortos.c, SystemP_nortos.c                                        │
│ - NoRTOS.c                                                                                         │
│                                                                                                    │
│ Esto provee la capa de abstraccion que el RF driver necesita sin requerir TI-RTOS completo.        │
│                                                                                                    │
│ 3.2 SmartRF Settings para BLE                                                                      │
│                                                                                                    │
│ Archivos nuevos: firmware/cc1352/smartrf_settings/ble_settings.c + .h                              │
│                                                                                                    │
│ Adaptados de los ejemplos del SDK, con:                                                            │
│ - RF_Mode RF_modeBle con patches rf_patch_cpe_multi_protocol + rf_patch_mce_bt5                    │
│ - Override arrays para BLE (sin BLE_STACK_OVERRIDES ya que no usamos el BLE stack)                 │
│ - TX power table para 2.4 GHz                                                                      │
│ - CMD_BLE5_RADIO_SETUP con frontEndMode para CatSniffer                                            │
│                                                                                                    │
│ 3.3 Radio init                                                                                     │
│                                                                                                    │
│ Archivo nuevo: firmware/cc1352/src/radio_ble.c + include/radio_ble.h                               │
│                                                                                                    │
│ - radio_init(): RF_open + CMD_FS                                                                   │
│ - Linkar rf_multiMode_cc13x2.a (RF driver precompilado)                                            │
│                                                                                                    │
│ Verificacion Checkpoint 3                                                                          │
│                                                                                                    │
│ 1. RF_open() retorna handle no-NULL (radio inicializada)                                           │
│ 2. Sin crash = RF core funcional                                                                   │
│ 3. LED indica estado (blink rapido = OK, solido = error)                                           │
│                                                                                                    │
│ ---                                                                                                │
│ Checkpoint 4: BLE Sniffer MVP (post-RF)                                                            │
│                                                                                                    │
│ 4.1 RX Engine con CMD_BLE5_GENERIC_RX                                                              │
│                                                                                                    │
│ En radio_ble.c:                                                                                    │
│ - Data queue circular con 4 entries estaticas                                                      │
│ - CMD_BLE5_GENERIC_RX configurado para BLE advertising (access address 0x8E89BED6)                 │
│ - Callback RF_EventRxEntryDone marca flag para main loop                                           │
│ - radio_poll() procesa entries y envia paquetes via UART                                           │
│                                                                                                    │
│ 4.2 Command Processor                                                                              │
│                                                                                                    │
│ Archivo nuevo: firmware/cc1352/src/command_processor.c + include/command_processor.h               │
│                                                                                                    │
│ - Acumula bytes UART, busca delimitador 0x00 (COBS)                                                │
│ - Decodifica COBS → parsea frame → dispatch por CMD_ID                                             │
│ - Comandos Phase 1: RADIO_INIT, SET_CHANNEL, SET_POWER, GET_INFO, RX_START, RX_STOP                │
│ - Envia RSP_ACK/RSP_ERROR/RSP_RX_PACKET                                                            │
│                                                                                                    │
│ 4.3 Main loop completo                                                                             │
│                                                                                                    │
│ int main(void) {                                                                                   │
│     board_init();                                                                                  │
│     NoRTOS_start();                                                                                │
│     uart_init();                                                                                   │
│     cmd_processor_init();                                                                          │
│                                                                                                    │
│     while (1) {                                                                                    │
│         cmd_processor_poll();  // Recibir comandos                                                 │
│         radio_poll();          // Procesar paquetes RX                                             │
│     }                                                                                              │
│ }                                                                                                  │
│                                                                                                    │
│ Verificacion Checkpoint 4                                                                          │
│                                                                                                    │
│ radio = Radio('/dev/ttyUSB0')                                                                      │
│ radio.init()                                                                                       │
│ radio.set_channel(37)                                                                              │
│ radio.start_rx()                                                                                   │
│ packets = list(radio.read_packets(timeout=10))                                                     │
│ assert len(packets) > 0  # BLE advertisements capturados                                           │
│                                                                                                    │
│ ---                                                                                                │
│ Archivos a Modificar/Crear (Resumen)                                                               │
│                                                                                                    │
│ ┌─────────────────────────────────────────────────┬───────────┬────────────┐                       │
│ │                     Archivo                     │  Accion   │ Checkpoint │                       │
│ ├─────────────────────────────────────────────────┼───────────┼────────────┤                       │
│ │ firmware/cc1352/ccfg.c                          │ Crear     │ 1          │                       │
│ ├─────────────────────────────────────────────────┼───────────┼────────────┤                       │
│ │ firmware/cc1352/CMakeLists.txt                  │ Modificar │ 1          │                       │
│ ├─────────────────────────────────────────────────┼───────────┼────────────┤                       │
│ │ firmware/cc1352/linker/cc1352p.ld               │ Modificar │ 1          │                       │
│ ├─────────────────────────────────────────────────┼───────────┼────────────┤                       │
│ │ firmware/cc1352/src/uart_drv.c                  │ Crear     │ 2          │                       │
│ ├─────────────────────────────────────────────────┼───────────┼────────────┤                       │
│ │ firmware/cc1352/include/uart_drv.h              │ Crear     │ 2          │                       │
│ ├─────────────────────────────────────────────────┼───────────┼────────────┤                       │
│ │ firmware/cc1352/src/startup_cc13x2_cc26x2_gcc.c │ Modificar │ 2          │                       │
│ ├─────────────────────────────────────────────────┼───────────┼────────────┤                       │
│ │ firmware/cc1352/smartrf_settings/ble_settings.c │ Crear     │ 3          │                       │
│ ├─────────────────────────────────────────────────┼───────────┼────────────┤                       │
│ │ firmware/cc1352/smartrf_settings/ble_settings.h │ Crear     │ 3          │                       │
│ ├─────────────────────────────────────────────────┼───────────┼────────────┤                       │
│ │ firmware/cc1352/src/radio_ble.c                 │ Crear     │ 3          │                       │
│ ├─────────────────────────────────────────────────┼───────────┼────────────┤                       │
│ │ firmware/cc1352/include/radio_ble.h             │ Crear     │ 3          │                       │
│ ├─────────────────────────────────────────────────┼───────────┼────────────┤                       │
│ │ firmware/cc1352/src/command_processor.c         │ Crear     │ 4          │                       │
│ ├─────────────────────────────────────────────────┼───────────┼────────────┤                       │
│ │ firmware/cc1352/include/command_processor.h     │ Crear     │ 4          │                       │
│ ├─────────────────────────────────────────────────┼───────────┼────────────┤                       │
│ │ firmware/cc1352/include/config.h                │ Modificar │ 1-4        │                       │
│ ├─────────────────────────────────────────────────┼───────────┼────────────┤                       │
│ │ firmware/cc1352/src/main.c                      │ Modificar │ 2-4        │                       │
│ └─────────────────────────────────────────────────┴───────────┴────────────┘                       │
│                                                                                                    │
│ Presupuesto de Memoria (80KB SRAM)                                                                 │
│                                                                                                    │
│ ┌───────────────────────┬───────────────┐                                                          │
│ │      Componente       │     Bytes     │                                                          │
│ ├───────────────────────┼───────────────┤                                                          │
│ │ UART RX buffer        │ 16,384        │                                                          │
│ ├───────────────────────┼───────────────┤                                                          │
│ │ UART TX buffer        │ 4,096         │                                                          │
│ ├───────────────────────┼───────────────┤                                                          │
│ │ RF RX entries (4x280) │ 1,120         │                                                          │
│ ├───────────────────────┼───────────────┤                                                          │
│ │ COBS buffers          │ ~800          │                                                          │
│ ├───────────────────────┼───────────────┤                                                          │
│ │ RF driver interno     │ ~4,000        │                                                          │
│ ├───────────────────────┼───────────────┤                                                          │
│ │ Stack (8KB)           │ 8,192         │                                                          │
│ ├───────────────────────┼───────────────┤                                                          │
│ │ DPL/NoRTOS            │ ~2,000        │                                                          │
│ ├───────────────────────┼───────────────┤                                                          │
│ │ .data/.bss            │ ~1,000        │                                                          │
│ ├───────────────────────┼───────────────┤                                                          │
│ │ Total                 │ ~37,600 (47%) │                                                          │
│ └───────────────────────┴───────────────┘                                                          │
│                                                                                                    │
│ Decision Arquitectural: NoRTOS vs TI-RTOS 7                                                        │
│                                                                                                    │
│ Se usa NoRTOS (bare-metal con DPL). Razones:                                                       │
│ - El RF driver funciona con NoRTOS via DPL abstractions                                            │
│ - Build mas simple (compilar fuentes DPL directamente)                                             │
│ - Menor footprint de memoria                                                                       │
│ - Main loop unico con polling es suficiente para Phase 1                                           │
│ - Se puede migrar a TI-RTOS 7 en Phase 5 si el reactive jamming lo requiere  