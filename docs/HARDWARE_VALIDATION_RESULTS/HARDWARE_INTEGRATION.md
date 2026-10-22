# Integración de hardware de CatSniffer con FeralRF

**Estado:** referencia técnica canónica para la instantánea auditada  
**Corte de evidencia:** 2026-10-22  
**FeralRF:** `feature/hardware-validation` @ `fbff1b7f5077964418fb63a3fdf346a27c35a003`  
**CatSniffer-Firmware disponible:** `v3.x` @ `c0cd5a45e019dbd14ed11d039aacb13f300e5731` (`v3.1.0.1`)  
**SDK TI:** `simplelink_cc13xx_cc26xx_sdk_8_30_01_01` @ `5b31d0a4903351e544546e23ef3330eaa4291ceb`

## 1. Propósito y límites

Este documento describe la arquitectura física y de firmware que interviene cuando FeralRF usa el radio integrado del CC1352P7 de CatSniffer. Separa lo confirmado por código o diseño de lo inferido y de lo todavía no medido. No convierte un ACK, una respuesta de Cat-Shell ni un nombre de preset en evidencia de frecuencia, potencia, modulación o ruta de antena efectivas.

La unidad física fue registrada por el proyecto como **CatSniffer v3.1** y el encapsulado de U8 se observó marcado `CC1352P74`. Sin embargo, el diseño publicado bajo el tag `CatSniffer:v3.1` (`5b99d984933a8b8c36e789853c08d3a2a785bed4`) identifica U8 como `CC1352P1F3RGZT`; además, los campos internos del PCB/esquema contienen revisiones discordantes. Por ello, el esquema histórico es evidencia de topología, pero **no prueba cada conexión ni la población exacta de las placas P7 evaluadas**.

## 2. Arquitectura del sistema

```text
Aplicación / API Python
          |
          | COBS + CRC16 sobre USB CDC Cat-Bridge
          v
PC <----> RP2040 ------------------------------------> Cat-Shell (USB CDC separado)
          |  puente transparente UART0, 921600 bit/s     | reset/boot CC1352
          |                                               | band1/band2/band3
          v                                               | estado/contadores RP2040
       CC1352P7
          | aplicación FeralRF bajo TI-RTOS
          | TI RF Driver -> RF core
          v
  red RF CC1352 / front-end ----\
                                  > U2 RFSW8006QTR7 -> J1 / antena
  SX1262 -> U6 switch TX/RX -----/
```

La cadena funcional de FeralRF es:

`PC/Python ↔ USB CDC Cat-Bridge ↔ RP2040 ↔ UART ↔ CC1352P7 ↔ RF core/front-end ↔ U2 ↔ antena`.

Cat-Shell es una interfaz de control independiente. El tráfico binario FeralRF no pasa por Cat-Shell; `band1`, `band2`, `band3`, `boot`, `exit`, `status` y los contadores del puente pertenecen al firmware RP2040.

El SX1262 y Cat-LoRa forman otra ruta. El SX1262 no ejecuta FeralRF, no recibe `SET_PROP_CONFIG` y no participa en `TX_RAW` del CC1352P7.

## 3. Componentes y responsabilidades

| Componente | Responsabilidad confirmada | No demuestra por sí solo |
|---|---|---|
| PC / `feralrf.Radio` | Descubrimiento y apertura de Cat-Bridge; framing COBS/CRC; secuencias; API síncrona; presets y helpers | Ejecución RF, posición de U2 o recepción OTA |
| RP2040 | Termina USB; expone Cat-Bridge, Cat-LoRa y Cat-Shell; puentea CDC0↔UART0; controla reset/boot y `CTF1..3`; mantiene contadores locales | Interpretación de comandos FeralRF ni configuración del RF core |
| CC1352P7 | Ejecuta FeralRF; procesa el protocolo; mantiene estado RX/TX; configura PHY, potencia y comandos TI; ejecuta primitivas crypto | Selección externa de U2 ni identidad del binario instalado a partir de `1.0.0` |
| TI RF Driver / RF core | Ejecuta setup, FS, RX, TX y test commands con las estructuras seleccionadas | Que el front-end/antena sea correcto o que un peer haya recibido |
| U2 `RFSW8006QTR7` | Selector RF externo de tres vías hacia J1 según el diseño publicado | Estado físico únicamente a partir del texto `Band` |
| U4 / red de adaptación publicada | Camino documentado 862–928 MHz; el diseño publicado también muestra la ruta 2.4 GHz | Población y respuesta de toda unidad P7 disponible |
| SX1262 + U6 | Radio LoRa/(G)FSK controlado por el RP2040, con ruta separada | Soporte MIOTY/Sidewalk/FeralRF por el CC1352P7 |

Fuentes: [`radio.py`](../python/feralrf/radio.py), [`protocol.h`](../firmware/cc1352/include/protocol.h), [`main_rtos.c`](../firmware/cc1352/src/main_rtos.c), [`radio_if.c`](../firmware/cc1352/src/radio_if.c), [`RP2040/catsniffer/src/main.c`](https://github.com/ElectronicCats/CatSniffer-Firmware/blob/c0cd5a45e019dbd14ed11d039aacb13f300e5731/RP2040/catsniffer/src/main.c), [`shell_commands.c`](https://github.com/ElectronicCats/CatSniffer-Firmware/blob/c0cd5a45e019dbd14ed11d039aacb13f300e5731/RP2040/catsniffer/src/shell_commands.c) y [`rpi_pico.overlay`](https://github.com/ElectronicCats/CatSniffer-Firmware/blob/c0cd5a45e019dbd14ed11d039aacb13f300e5731/RP2040/catsniffer/boards/rpi_pico.overlay). Las tres referencias RP2040 están fijadas al commit auditado; no requieren un checkout hermano.

## 4. Interfaces físicas y lógicas

### 4.1 USB y UART

El overlay activo declara tres CDC:

| Interfaz | Rol | Evidencia de fuente |
|---|---|---|
| Cat-Bridge / CDC0 | Passthrough CC1352P7 | `rpi_pico.overlay`, nodos `cdc_acm_uart0` L123–126; `main.c`, `cdc0_interrupt_handler()` L240–264 |
| Cat-LoRa / CDC1 | Datos SX1262 | `rpi_pico.overlay`, L128–131 |
| Cat-Shell / CDC2 | Consola de control RP2040 | `rpi_pico.overlay`, L133–136; `shell_commands.c` |

UART0 opera a 921600 bit/s en passthrough (`rpi_pico.overlay` L139–143). `boot` cambia el modo y la velocidad usada con el cargador del CC1352; `exit` restaura passthrough. El RP2040 no decodifica COBS ni comandos FeralRF en CDC0.

El camino CC1352→USB incrementa `uart_overrun_count` y `ring_overflow_count` cuando detecta overrun o no cabe todo el bloque (`main.c` L202–223). El camino USB→CC1352 inserta byte a byte sin comprobar el retorno de `safe_ring_buf_put()` (`main.c` L240–251). Por tanto, la observabilidad del transporte es asimétrica y no cubre toda la cadena.

### 4.2 Reset y arranque

El overlay disponible asigna:

- `CC1352_BOOT`: RP2040 GPIO2.
- `CC1352_RESET`: RP2040 GPIO3.
- cJTAG/JTAG: RP2040 GPIO11–14.

`Radio.reset_device()` no opera estas señales directamente. Deriva Cat-Shell como número de puerto Cat-Bridge + 2, cierra el bridge, envía `boot`, luego `exit`, reconecta e invoca `init()` (`python/feralrf/radio.py`, `_get_shell_port()` y `reset_device()`, L184–246). El mapa observado COM88/COM87 demuestra que esa aritmética no es una asociación segura. La función tampoco lee ni valida las respuestas de Cat-Shell.

El archivo [`hardware/PINOUT.md`](../hardware/PINOUT.md) menciona GPIO15 para reset en material anterior. Esa discrepancia frente al overlay ejecutable no se resuelve suponiendo que ambos describen la misma revisión.

### 4.3 Selección de banda

`shell_commands.c` confirma:

| Comando | Estado lógico | Uso pretendido |
|---|---|---|
| `band1` | `GIG` | camino CC1352 de 2.4 GHz |
| `band2` | `SUBGIG_1` | camino CC1352 Sub-GHz |
| `band3` | `SUBGIG_2` | camino SX1262 / LoRa |

El overlay asigna `CTF1`, `CTF2`, `CTF3` a GPIO8, GPIO9 y GPIO10 (`rpi_pico.overlay` L50–60). El diseño publicado muestra esas señales en U2. La respuesta `Band: 0/1/...` sólo acredita estado lógico del RP2040. Las campañas 3D/3E/4A entregaron paquetes Sub-GHz aun reportando `Band: 0`; esto impide usar el texto de estado como explicación causal de la ruta física, pero no invalida la topología documentada.

## 5. Arquitectura interna del CC1352P7

### 5.1 TI-RTOS y tareas

`main_rtos.c` crea una tarea UART y una tarea RF, ambas cooperativas en la implementación auditada:

- `UartTask_taskFxn()` inicializa `HostIF`, `TaskEvent`, `ControlTask`, `CommandProcessor` y `HostIFTask`, y bombea comandos/respuestas.
- `RfTask_taskFxn()` inicializa crypto, abre al arranque un handle 433 MHz persistente con estructuras dedicadas y ejecuta `DataTask_poll()`.
- `DataTask_poll()` procesa el comando pendiente, solicitudes de stop, TX diferido, timeout de jamming, arranque RX, polling del backend y hasta ocho paquetes RX por iteración.

La cola de salida tiene profundidad 32. `PacketQueue` cruza las tareas RF/UART sin primitivas de sincronización visibles; esto es un riesgo estático, no una causa demostrada de los timeouts.

### 5.2 Protocolo y asincronía

La trama lógica es:

`CMD:u8 | SEQ:u8 | LEN:u16-le | PAYLOAD | CRC16-CCITT`, seguida de COBS y delimitador `0x00`.

El límite C es 255 bytes de payload (`protocol.h`, `PROTOCOL_MAX_PAYLOAD`). RX usa `RSP_RX_PACKET=0x90`; los eventos se producen con una secuencia propia 0…254. El host preserva como asincrónicos principalmente los eventos con `seq=0`; durante una espera síncrona, otros `RSP_RX_PACKET` con secuencia distinta pueden descartarse (`radio.py`, `_read_response()` L278–397). Esta frontera contractual explica pérdida host-visible posible, no prueba el origen único de `RX_STOP`.

### 5.3 Configuración y ejecución RF

- `SET_PHY`, `SET_CHANNEL` y `SET_POWER` actualizan estado y responden ACK; la operación RF efectiva puede ocurrir después.
- `SET_PROP_CONFIG` copia frecuencia, tipo, tasa, desviación, BW, sync y `format_conf` a estructuras TI, pero `RadioIF_setPropConfig()` es `void`; el handler responde ACK sin readback ni resultado del RF core (`command_processor.c` L168–185).
- `RX_START` agenda el arranque y responde antes de `RadioIF_startRx()`; un fallo posterior se notifica como `RSP_ERROR` asincrónico.
- RAW/FRAME/BURST/CONTINUOUS se aceptan y ejecutan de forma diferida. Sólo RAW emite un error posterior genérico en la ruta actual; BURST/CONTINUOUS no reportan progreso o completion.
- CW/PRBS comprueban que el comando de prueba pudo postearse, pero no miden señal radiada.

## 6. Compatibilidad por banda y front-end

| Banda / función | Silicio o fuente | Implementación actual | Hardware publicado | Estado defendible |
|---|---|---|---|---|
| 868/915/902.2 MHz | CC1352P7 y comando proprietary | FSK/GFSK implementados; múltiples entregas raw históricas | U4 documentado 862–928 MHz; U2→J1 | Compatible y demostrada entre dos CatSniffer para configuraciones concretas; frecuencia/potencia no medidas |
| 433 MHz | Silicio soporta bandas Sub-1; hay estructuras SysConfig 433 y handle persistente | FSK/GFSK/OOK tienen ruta de código; 4/5/6 siguen reservados | Población/ruta/antena de las placas P7 no establecida | Bloqueado por evidencia de hardware para una afirmación de soporte de placa |
| 169.45 MHz | Fuera de las bandas listadas para el CC1352P7 auditado | Selecciona modo 433 para `<861 MHz`; overrides 169 se escriben en la estructura compartida, no de forma coherente en la ruta ejecutada | U4 publicado no cubre 169 MHz | No defendible en el hardware actual; no ensayar sin rediseño/target distinto |
| Proprietary 2.4 GHz | CC1352P7 puede operar 2.4 GHz | `configure_prop(2440 MHz)` es sobrescrito por `RadioIF_applySub1ghzChannelConfig()`, que sólo acepta `frequency_hz<1000 MHz` y cae a 868 MHz | Camino 2.4 GHz pretendido por `band1` | Bloqueado por defecto de backend; un ACK no prueba 2440 MHz |
| BLE / IEEE 2.4 GHz | Comandos TI dedicados | BLE advertising limitado e IEEE PHY/transporte MAC | Camino `band1` previsto | IEEE demostrado; BLE no cuenta con interoperabilidad externa consolidada |
| High-PA | Tablas locales incluyen niveles altos según PHY | Se elige valor exacto o cercano sin readback efectivo | Depende de front-end, revisión y región | No habilitar como garantía general sin variante, carga, medición y restricciones regulatorias |

La ficha TI de referencia usada por las auditorías es [**CC1352P7 SimpleLink High-Performance Multi-Band Wireless MCU, SWRS251A**](https://www.ti.com/lit/ds/symlink/cc1352p7.pdf), especialmente la tabla de bandas/características de RF y la arquitectura del RF core. El diseño de placa se trazó contra las [fuentes de hardware de CatSniffer v3.1 fijadas al commit publicado](https://github.com/ElectronicCats/CatSniffer/tree/5b99d984933a8b8c36e789853c08d3a2a785bed4/hardware); el balun/filtro publicado es `0900FM15D0039` (datasheet local [`0900FM15D0039.pdf`](../hardware/Datasheet/0900FM15D0039.pdf)). Ninguna de esas referencias sustituye una BOM/ECO de la placa P7 física.

## 7. Potencia, antena y seguridad de ensayo

`set_power()` solicita un dBm. `RadioIF_resolveTxPowerValue()` resuelve contra tablas por modo y puede elegir el valor más cercano; la API no devuelve el valor aplicado ni el tipo de PA. Sin power meter o analizador no se conoce potencia conducida/radiada.

Requisitos antes de futuros ensayos:

1. Identificar placa, revisión, front-end poblado y antena/carga.
2. Asociar Bridge/Shell por identidad USB, no por suma de COM.
3. Seleccionar el camino RP2040 y el PHY CC1352 por separado.
4. Aplicar límites regionales y usar entorno apantallado cuando corresponda.
5. No conectar TX a RX por coaxial sin fixture, atenuación y presupuesto de potencia verificados.
6. Registrar hash de binarios y configuración; para claims físicos, usar instrumento independiente.

## 8. Evidencia, supuestos e incertidumbres

| Clase | Conclusión |
|---|---|
| Verificado por fuente | Tres CDC; UART0 921600; GPIO2/3 y GPIO8/9/10; comandos Cat-Shell; protocolo binario; tareas CC; drivers y rutas de software |
| Verificado por diseño publicado | RP2040↔CC1352 por UART/control; U2 común; rutas CC 2.4/Sub-G y SX1262 hacia J1 en el tag v3.1 |
| Verificado experimentalmente | Control Cat-Shell; enlaces IEEE y proprietary raw concretos; resultados contradictorios y recuperación documentados |
| Observación física registrada | U8 marcado `CC1352P74` en la unidad reportada |
| Supuesto no permitido | Que el esquema P1 histórico represente exactamente toda placa P7 disponible |
| Pendiente | BOM/ECO/revisión eléctrica P7; niveles CTF/U2; antena activa; potencia/frecuencia/modulación medidas; hashes de binarios instalados |

## 9. Referencias de trazabilidad

Este documento incorpora las conclusiones de arquitectura y hardware necesarias para entender la integración. Las referencias siguientes aportan trazabilidad adicional, pero ninguna es necesaria para interpretar sus afirmaciones principales:

- Estado y arquitectura implementada: [`PROJECT_STATUS_REPORT.md`](PROJECT_STATUS_REPORT.md).
- Compatibilidad condicionada y defectos por banda/modulación: [`FUNCTIONAL_GAPS.md`](FUNCTIONAL_GAPS.md).
- Evidencia experimental consolidada y sus limitaciones: [`EXPERIMENTAL_EVIDENCE_REFERENCE.md`](EXPERIMENTAL_EVIDENCE_REFERENCE.md).
- Contrato local: [`protocol.md`](protocol.md), [`ARCHITECTURE.md`](ARCHITECTURE.md) y [`PYTHON_API.md`](PYTHON_API.md), leídos como documentación histórica/corriente y no como autoridad superior al source.
- Fuentes externas fijadas: [hardware CatSniffer v3.1](https://github.com/ElectronicCats/CatSniffer/tree/5b99d984933a8b8c36e789853c08d3a2a785bed4/hardware), [firmware RP2040 v3.1.0.1](https://github.com/ElectronicCats/CatSniffer-Firmware/tree/c0cd5a45e019dbd14ed11d039aacb13f300e5731/RP2040/catsniffer) y [datasheet CC1352P7](https://www.ti.com/lit/ds/symlink/cc1352p7.pdf).

