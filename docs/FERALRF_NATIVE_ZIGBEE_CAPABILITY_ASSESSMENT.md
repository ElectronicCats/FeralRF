# Evaluación de capacidades Zigbee nativas para FeralRF

**Tipo de documento:** addendum técnico basado en fuente  
**Fecha de investigación:** 2026-11-03  
**Repositorio examinado:** `31f9535d5a78171b51ac3320170791c2d090a62c`  
**SDK fijado:** `5b31d0a4903351e544546e23ef3330eaa4291ceb`, SimpleLink `8.30.01.01`  
**Alcance:** ampliar la persona FeralRF existente; una imagen ZNP, ZED u otra persona Zigbee separada queda expresamente fuera de alcance.

## 1. Resumen y respuesta corta

La máxima integración defendible con la evidencia actual es una **extensión nativa y predominantemente host-side** de FeralRF que conserve su transporte IEEE 802.15.4 raw y añada, con parsers acotados y verificables:

1. disección MAC IEEE 802.15.4;
2. disección Zigbee NWK y APS;
3. decodificación ZDO y ZCL plaintext, incluido el sistema de tipos ZCL seleccionado;
4. interpretación de cabeceras de seguridad, preservando ciphertext y MIC;
5. observación pasiva con estado —mapeos de direcciones, correlación, reensamble y topología observada— sin presentarla como estado de red autorizado;
6. descifrado autenticado opcional sólo con claves autorizadas y contexto de nonce/AAD completo, y sólo después de verificar el MIC;
7. construcción protocol-aware de tramas y su entrega al TX raw ya existente, siempre etiquetada como construcción/inyección, no como participación Zigbee.

El código visible de Z-Stack aporta algoritmos pequeños y definiciones útiles, sobre todo en ZCL y algunos formatos ZDO, pero no aporta implementaciones fuente independientes de los parsers NWK/APS ni del MAC operacional. La adaptación razonable consiste en **extraer el comportamiento wire y añadir límites, errores y propiedad explícita**, o implementar equivalentes estrechos desde una revisión normativa fijada. Compilar `zcl.c`, `zd_object.c`, ZMAC o el runtime de seguridad dentro del target FeralRF no está justificado.

Las funciones activas MAC —filtrado, auto-ACK, CSMA-CA, espera de ACK, reintentos, asociación y turnaround RX→TX— no existen hoy como servicio FeralRF. El RF Core expone mecanismos parciales, pero esa disponibilidad no demuestra una integración correcta. Sólo el disparo temporal y cadenas RF simples merecen investigación posterior como primitivas acotadas; auto-ACK/CSMA/ACK/reintentos requieren antes un contrato MAC, estado, terminación correlacionada y validación temporal.

## 2. Alcance y lenguaje de evidencia

Este informe no propone una segunda imagen, no añade APIs públicas, no modifica código, no compila y no ejecuta RF. Describe **qué sería justificable investigar o incorporar**, no qué está implementado.

Se usan estas clasificaciones:

- **A — Existente en FeralRF:** implementación identificable en el HEAD actual.
- **B — Fuente adaptable:** lógica visible y trazable cuyas dependencias pueden aislarse o reemplazarse; no significa que el archivo completo sea enlazable.
- **C — Funcionalidad de protocolo implementable independientemente:** wire format respaldado por definiciones accesibles y versionables, aunque la implementación TI no sea visible o no sea adecuada.
- **D — Requiere servicios operacionales adicionales:** depende de temporización MAC, estado de red/seguridad, persistencia, scheduling u otros servicios ausentes.
- **E — No soportada o no resuelta:** la evidencia disponible no permite establecer factibilidad.

Además:

- **Confirmado por fuente** significa que el comportamiento se puede seguir en el checkout.
- **Confirmado por SDK** significa que el SDK fijado declara/implementa el mecanismo; no prueba integración con FeralRF.
- **Evidencia histórica** describe una observación previa con sus límites; no prueba Zigbee.
- **Condicional** identifica requisitos previos verificables.

## 3. Línea base verificada

| Elemento | Resultado del 2026-11-03 | Consecuencia |
|---|---|---|
| Repositorio | `HEAD 31f9535d5a78171b51ac3320170791c2d090a62c`, rama `feature/hardware-validation`, seguimiento de `origin/feature/hardware-validation` | Baseline de este addendum. |
| Working tree inicial | Limpio | No había modificaciones locales que reconciliar o preservar al iniciar. |
| Baseline solicitado | `bfe056cc323911b6fc4c6f224d1f4f626ee15942` | `git diff bfe056c..HEAD -- firmware python hardware` no muestra cambios: la implementación relevante no cambió. |
| Diferencia desde `bfe056c` | Commit documental `31f9535`, que reorganiza informes y añade la propuesta de convergencia | Las conclusiones de fuente siguen aplicando; varias rutas documentales cambiaron. |
| SDK | Submódulo `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01` en `5b31d0a4903351e544546e23ef3330eaa4291ceb`; `.metadata/product.json:4` declara `8.30.01.01` | Es el único SDK usado. No se sustituyó por otra instalación. |
| Procesador | `CC1352P7`; `firmware/cc1352/CMakeLists.txt:12-17`; configuración generada `CC1352P7RGZ` en `syscfg/ti_radio_config.c:5-22` | Todas las conclusiones embedded se limitan a este target/configuración. |
| Toolchain configurado | GNU Arm: `arm-none-eabi-gcc/g++`, `CMakeLists.txt:35-43`, con bibliotecas driver GCC `:123-124` | El target actual no consume las librerías Z-Stack/TIMAC de rol. |
| Toolchain resoluble | `arm-none-eabi-gcc` no está en `PATH` en esta sesión; no se ejecutó build | La versión efectiva del compilador sigue **no verificada**; esto no contradice el toolchain configurado. |
| Librerías Z-Stack | `source/ti/zstack/lib/{iar,ticlang}`; selección por SysConfig en `.meta/zstack.syscfg.js:150-226` | No hay un conjunto Z-Stack de rol para GCC demostrado en el checkout. |

### 3.1 Reconciliación de informes previos

Se revisaron:

- `docs/FERALRF_DEEP_CODE_IMPLEMENTATION_AUDIT_2026-10-27.md`;
- `docs/Z-Stack_Reports/ZSTACK_INTEGRATION_FEASIBILITY_AUDIT_2026-10-27.md`;
- `docs/Z-Stack_Reports/ZSTACK_PARTIAL_REUSE_AND_ZIGBEE_DECODER_AUDIT.md`;
- `docs/Z-Stack_Reports/ZSTACK_FERALRF_API_CONVERGENCE_PROPOSAL.md`;
- `docs/Z-Stack_Reports/ZSTACK_FERALRF_API_CONVERGENCE_SOURCES.md`.

Las correcciones A–F del informe de reuso parcial están presentes en su sección `Audit Corrections and Evidence Reconciliation` y ahora forman parte del commit documental de `HEAD`. Por tanto, ya no existe la modificación local pendiente descrita por la propuesta anterior. Permanecen como preguntas técnicas —no como correcciones editoriales incompletas— la revisión Zigbee objetivo, corpus, semántica exacta de timestamp, memoria libre, ABI, vectores CCM* y versiones de herramientas de comparación.

Se detectó una discrepancia de inventario: `docs/Z-Stack_Reports/ZSTACK_FERALRF_API_CONVERGENCE_SOURCES.md:92` menciona `source/ti/zstack/stack/nwk/nwk.c`, pero ese archivo **no existe** en el submódulo fijado. El directorio contiene `nwk.h`, `nwk_bufs.h`, `nwk_globals.c`, `nwk_globals.h` y `nwk_util.h`. Como no hubo cambio del puntero SDK, la referencia previa era incorrecta, no una eliminación reciente. El efecto es conservador: refuerza —no debilita— la conclusión de que el parser/core NWK no está disponible como fuente adaptable. Este addendum no modifica el informe anterior.

## 4. Inventario de fuentes primarias

### 4.1 FeralRF

| Área | Ruta, función y líneas | Hecho confirmado |
|---|---|---|
| Contrato Python | `python/feralrf/radio.py`: `Packet` `18-30`; `set_phy()` `455-466`; `set_channel()` `468-478`; `start_rx()` `599-607`; `stop_rx()` `609-634`; `read_packets()` `636-709`; `transmit()` `725-735`; `transmit_frame()` `737-750` | Bytes raw, metadatos y control host. |
| Transporte | `python/feralrf/protocol.py:11-90`; `firmware/cc1352/src/protocol.c`: `protocol_build_frame()` `115-143`, `protocol_parse_frame()` `145-180` | COBS sobre `CMD|SEQ|LEN|PAYLOAD|CRC16`; no es una trama Zigbee. |
| Despacho | `firmware/cc1352/src/command_processor.c`: `handle_command()` `101-270`, casos IEEE/RX/TX `128-258` | Los comandos se validan y se encolan; el ACK puede preceder la operación RF. |
| Control | `firmware/cc1352/src/control_task.c`: init `194-223`, PHY `225-237`, TX `249-264,332-358`, RX `440-454` | Estado, buffers y eventos diferidos. |
| Bucle de datos | `firmware/cc1352/src/data_task.c`: `DataTask_emitRxPacket()` `28-65`, `DataTask_poll()` `76-139` | Orden control→RF→LL→respuesta asíncrona. |
| Selección PHY | `firmware/cc1352/src/phy_manager.c:9-75` | IEEE usa backend RF y link layer `DEFAULT`. |
| Link layer | `firmware/cc1352/src/ll_manager.c`: `LLManager_processRxPacket()` `40-159` | Sólo clasifica BLE; IEEE pasa sin parseo MAC/Zigbee. |
| Interfaz RF | `firmware/cc1352/src/radio_if.c`: colas/constantes `32-75`; TX IEEE `457-488`; RX IEEE `1335-1371,1497-1568`; despacho/estado `2182-2204,2297-2400` | Propiedad RF, PHR/FCS, buffers y callbacks. |
| SmartRF IEEE | `firmware/cc1352/src/smartrf_ieee_15_4_0.c`: RX `81-144`; TX `146-163` | Filtrado/auto-ACK/CCA apagados; TX raw inmediato. |
| RTOS | `firmware/cc1352/src/main_rtos.c:54-77,110-178,220-232` | Tareas UART/RF, prioridad 3 y stacks de 4096 bytes. |
| Crypto | `python/feralrf/radio.py:943-1004`; `firmware/cc1352/src/command_processor.c:357-414`; `crypto_engine.c:197-246` | AES-CCM genérico one-shot con nonce 7–13 y tags pares 4–16; no existe orquestación Zigbee. |
| Build/hardware | `firmware/cc1352/CMakeLists.txt:12-17,35-43,123-184`; `syscfg/ti_drivers_config.c:119-160,202-303`; `hardware/PINOUT.md:3-17,31-73` | Target GNU, RF driver generado y control de front-end/antena. |

### 4.2 SDK TI fijado

| Área | Ruta y líneas | Uso permitido por la evidencia |
|---|---|---|
| ZCL | `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/stack/zcl/zcl.c:40-60,2412-2490,2961-3117,3195-3322`; `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/stack/zcl/zcl.h:1927-1994` | Algoritmos/header/tipos visibles; necesitan bounds y desacoplamiento. |
| ZDO | `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/stack/zdo/zd_object.c:2914-3733`; `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/stack/zdo/zd_object.h:399-602` | Orden de campos y propiedad TI; muchos parsers reservan memoria y carecen de bounds completos. |
| NWK | `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/stack/nwk/nwk_util.h:111-201,286-321`; `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/stack/nwk/nwk.h`; `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/stack/nwk/nwk_globals.c`; `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/stack/nwk/nwk_globals.h`; `nwk.c` ausente | Máscaras/estructuras y declaración de `NLDE_ParseMsg()`; no su definición fuente. |
| APS | `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/stack/nwk/aps_mede.h:91-173,231-234`; `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/stack/nwk/aps_util.h:86-117`; `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/stack/nwk/aps.h` | Definiciones de trama y declaración de `APSDE_ParseMsg()`; no su definición fuente. |
| ZMAC/TIMAC | `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/zmac/f8w/zmac.c:480-560,870-917,1160-1218`; `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/zmac/f8w/zmac_cb.c:165-240` | Wrappers/callbacks que llaman `MAP_MAC_*` y colas OSAL; el MAC real no está disponible como fuente reutilizable. |
| RF Core IEEE | `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/devices/cc13x2x7_cc26x2x7/driverlib/rf_ieee_cmd.h:80-207,266-394,548-620` | Campos de RX, filtering/auto-ACK, trigger temporal, TX, CSMA, RX_ACK, CCA y salida; mecanismo, no servicio MAC completo. |
| Selección de libs | `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/.meta/zstack.syscfg.js:150-226`; `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/lib/{iar,ticlang}` | Acoplamiento a device/role/toolchain y librerías binarias. |
| Seguridad/estado | `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/stack/sec/`; `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/stack/zdo/zd_sec_mgr.c`; `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/stack/nwk/nwk_globals.h:144-145,458-466`; `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/stack/nwk/aps_mede.h:231-234` | Contadores, keys y política pertenecen al runtime; las definiciones wire sí son referencia. |

### 4.3 Documentación y evidencia histórica

El registro versionado `docs/Z-Stack_Reports/ZSTACK_FERALRF_API_CONVERGENCE_SOURCES.md:110-140` conserva URLs oficiales, versiones y fecha de acceso 2026-11-02. En particular:

- documentación TI Z-Stack 8.30.00/SDK 8.30.01.01 de arquitectura y configuración;
- manifest y release notes oficiales del SDK 8.30.01.01;
- catálogo CSA consultado, que ofrece familias Zigbee Core R22.1/R23.x, ZCL R7/R8 y BDB 3.x;
- ZCL rev. 6 pública como referencia histórica, no como revisión objetivo implícita;
- experimentos IEEE raw bajo `docs/HARDWARE_VALIDATION_RESULTS/OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-12/`.

La evidencia histórica respalda RX/TX IEEE raw en un setup y canal concretos; no respalda parsing Zigbee, asociación, join, ACK automático, CSMA, seguridad Zigbee ni interoperabilidad.

## 5. Arquitectura IEEE 802.15.4 existente

### 5.1 Recorrido real de comandos y paquetes

```text
Python Radio
  └─ CommandBuilder + COBS/CRC16
       └─ UART/RP2040 bridge
            └─ HostIFTask_processPendingCommand()
                 └─ CommandProcessor::handle_command()
                      └─ ControlTask (copia + TaskEvent)
                           └─ DataTask_poll() en tarea RF
                                └─ RadioIF → TI RF Driver → RF Core

RF Core RX queue (3 entradas)
  └─ callback: sólo marca evento
       └─ RadioIF_poll() extrae PHR/MPDU/metadatos
            └─ cola RadioIF (8 paquetes)
                 └─ LLManager (IEEE: pass-through)
                      └─ DataTask_emitRxPacket()
                           └─ respuesta asíncrona → Python Packet
```

1. `Radio.set_phy()`, `set_channel()`, `start_rx()` y `transmit()` originan comandos host (`radio.py:455-478,599-607,725-735`).
2. `handle_command()` valida y llama a `ControlTask`; RX/TX se representan mediante eventos (`command_processor.c:128-258`; `control_task.c:249-264,440-448`).
3. `DataTask_poll()` consume esos eventos en la tarea RF y llama a `RadioIF` (`data_task.c:76-127`).
4. `RadioIF` posee el handle RF persistente y ejecuta `RF_postCmd()` para RX y `RF_runCmd()` para TX (`radio_if.c:324-381,1080-1120,1373-1397`).
5. El callback RX no parsea Zigbee ni transmite ACK desde software; marca eventos, y el polling posterior drena las colas (`radio_if.c:2354-2379`).
6. `LLManager_processRxPacket()` no añade semántica IEEE/Zigbee (`ll_manager.c:144-159`).
7. `DataTask_emitRxPacket()` serializa timestamp, canal, RSSI, correlación, CRC, longitud, bytes y metadatos LL (`data_task.c:28-65`); `Radio.read_packets()` reconstruye `Packet` (`radio.py:673-707`).

### 5.2 Contrato RX

`RadioIF_startIeee154RfBackend()` fuerza `bIncludePhyHdr=1`, `bIncludeCrc=1` y añade RSSI, corr/CRC y timestamp (`radio_if.c:1347-1355`). `RadioIF_processIeee154Packets()`:

- usa el primer byte como longitud PHR;
- elimina ese PHR;
- conserva `pkt.data_len` bytes posteriores, que incluyen el FCS recibido;
- deriva `crc_ok` del bit `bCrcErr` y descarta tramas con CRC malo;
- expone el valor de correlación en siete bits como `lqi`; no se ha demostrado que sea un LQI normalizado Zigbee;
- convierte RAT a microsegundos dividiendo entre cuatro; el evento exacto, wrap y continuidad no están validados;
- limita almacenamiento a `RADIO_IF_MAX_PACKET_DATA=242`, aunque una MPDU IEEE estándar es menor (`radio_if.h:15-27`; `radio_if.c:1497-1568`).

Por tanto, `Packet.data` es una entrada suficiente para un parser pasivo, pero éste debe recibir explícitamente la política `FCS presente`. El decoder no debe adivinarla a partir de la longitud.

### 5.3 Contrato TX

`RadioIF_transmitIeee154Raw()` acepta de 1 a 125 bytes, copia a un buffer propiedad del firmware y usa `CMD_IEEE_TX` (`radio_if.c:468-488`). El SmartRF TX tiene `bIncludePhyHdr=0` y `bIncludeCrc=0`, por lo que el RF Core calcula PHR y FCS (`smartrf_ieee_15_4_0.c:146-162`; SDK `rf_ieee_cmd.h:289-300`).

El input actual es, por tanto, un MPDU **sin FCS** de hasta 125 bytes. `TX_FRAME` es un alias de `TX_RAW`, no un constructor MAC (`control_task.c:262-264`). No hay generación de FCF, sequence number, direcciones, security headers, counters, CSMA o retries.

### 5.4 Sincronía, terminación y propiedad RF

- El ACK COBS de `RX_START`, `RX_STOP`, `TX_RAW` y `TX_FRAME` se emite al aceptar/encolar (`command_processor.c:188-258`), antes de ejecutar RF en `DataTask_poll()`.
- Un fallo TX posterior sólo produce `RSP_ERROR` asíncrono con `seq=0` (`control_task.c:332-357`). No existe una confirmación terminal correlacionada de éxito RF.
- RX es continuo mediante `RF_postCmd()`; TX usa `RF_runCmd()` bloqueante en el contexto de la tarea RF (`radio_if.c:343-377`).
- `RadioIF_transmitRaw()` rechaza TX si `s_rx_running` es verdadero (`radio_if.c:2182-2187`). No existe turnaround RX→TX en la sesión actual.
- Dos tareas de igual prioridad 3 cooperan mediante `Task_yield`; ambas tienen stack estático de 4096 bytes (`main_rtos.c:54-77,220-232`).
- El handle RF y la configuración de antena/frontend son recursos compartidos por todos los PHY de FeralRF. Cambiar `RadioIF` puede afectar BLE y sub-GHz, no sólo IEEE.

## 6. Fronteras de integración defendibles

| Frontera | Procesador/componente | Input/output actual | Uso Zigbee razonable | Límite/riesgo |
|---|---|---|---|---|
| `Packet.data` | Host Python | MPDU con FCS + metadatos | Parser MAC→NWK→APS→ZDO/ZCL, estado pasivo, PCAP y crypto contextual | Menor riesgo; requiere contrato FCS/revisión/corpus. |
| `Radio.transmit()` | Host→CC1352P7 | MPDU sin FCS, ACK de aceptación | Enviar bytes construidos para pruebas autorizadas | No CSMA/ACK/retry/completion; no nodo Zigbee. |
| AES-CCM genérico | Host→CC1352P7 crypto driver | key, nonce, AAD, data, tag | Acelerar una operación CCM válida si el host construye exactamente el contexto | No implementa CCM* Zigbee, selección de key, nonce ni counters; no admite tag 0. |
| `LLManager_processRxPacket()` | Firmware, después de RF | `RadioIF_RxPacket` mutable | Sólo si se justificara metadata ligera y estable | Parsear Zigbee aquí consume MCU, complica transporte y no ayuda al análisis host. |
| `RadioIF` | Firmware/RF Driver | Comandos RF y colas | Única frontera para timing, filtering, auto-ACK o cadenas RF | Máximo impacto cruzado, ownership único, exige validación de hardware/RTOS/timing. |
| `CommandProcessor`/`ControlTask` | Firmware control | COBS y eventos | Controles futuros sólo si una primitiva embedded queda demostrada | El protocolo no tiene completion correlacionado; cambios requieren versionado. |

**Decisión que puede tomarse ahora:** el decoder y el estado pasivo deben vivir inicialmente en Python sobre el contrato raw. No necesitan modificar firmware.

**Decisión condicional:** una primitiva MAC temporal sólo debe entrar al firmware si no puede realizarse en host y si existe una especificación de su contrato, terminación y efecto sobre captura/otros PHY.

## 7. Evaluación por función de Z-Stack

### 7.1 ZCL

| Función TI | Comportamiento visible | Dependencias/problemas | Clasificación | Uso defendible |
|---|---|---|---|---|
| `zclParseHdr()` (`zcl.c:2412-2446`) | Lee frame control, manufacturer code opcional, TSN y command ID; devuelve puntero al payload | No recibe longitud; puede leer 3 o 5 bytes fuera de rango; usa tipos/macros ZCL | **B** | Adaptar algoritmo o usarlo como referencia; API nueva debe recibir `buffer+length` y error estructurado. |
| `zclBuildHdr()` (`:2459-2482`) | Serializa header de 3/5 bytes | Es `static`, no tiene capacidad destino ni valida bits | **B** | Reproducir lógica acotada; no exportar/compilar `zcl.c` por este helper. |
| `zclGetDataTypeLength()` (`:3195-3283`) | Mapea tipos escalares a 1–16 bytes | Cero significa no-data, unknown o tipo variable; depende de constantes | **B** | Tabla de tipos con resultado explícito fijo/variable/no soportado. |
| `zclGetAttrDataLength()` (`:3296-3322`) | Obtiene longitud fija o prefijo de string corto/largo y detecta overflow aritmético largo | Lee uno/dos bytes sin longitud disponible; no comprueba que el contenido exista | **B** | Conservar regla de longitud, añadir bounds y límites configurables. |
| `zclSerializeData()` (`:2961-3076`) | Serializa escalares little-endian, strings y key de 128 bits | Sin capacidad destino; asume layout de C/host; cobertura no equivale a ZCL completo | **B** | Adaptar por tipo con writer acotado; validar contra vectores y round-trip. |

El translation unit completo incluye configuración, AF, BDB y Green Power (`zcl.c:40-60`) y contiene procesamiento runtime. **No es reutilizable de forma independiente.** La mejor frontera es una implementación host acotada, inspirada en estos algoritmos y en una revisión ZCL fijada. Reusar líneas casi literalmente exige revisión de licencia por archivo; este informe no emite conclusión legal.

### 7.2 ZDO

Los `ZDO_Parse*` demuestran orden de campos, pero no forman una biblioteca de parsing defensivo:

- `ZDO_ParseBindUnbindReq()` lee directamente `inMsg->asdu` sin usar `asduLen` (`zd_object.c:2969-2991`).
- `ZDO_ParseAddrRsp()` inspecciona una longitud mínima, reserva con `OsalPort_malloc` y luego confía en el count al copiar la lista (`:3005-3057`).
- `ZDO_ParseEPListRsp()` toma `cnt=msg[2]`, reserva y copia sin comprobar el ASDU completo (`:3172-3194`).
- `ZDO_ParseMgmtLqiRsp()` reserva según count y recorre registros sin validar bytes restantes (`:3225-3284`).
- `ZDO_ParseSimpleDescBuf()` no recibe longitud, reserva dos listas y define reglas de liberación (`:3543-3594`).
- `ZDO_ParseDeviceAnnce()` es field-order simple, pero tampoco recibe longitud (`:3607-3618`).

Clasificación: **B** para algoritmos/orden de campos concretos, no para `zd_object.c` completo. Para un decoder host general se prefiere **C**, una implementación independiente y acotada por cluster ZDO, porque elimina `zdoIncomingMsg_t`, OSAL heap y ownership TI. Los parsers que devuelven buffers TI no deben cruzar la frontera Python/firmware.

### 7.3 NWK y APS

`NLDE_ParseMsg(byte *buf, byte bufLength, NLDE_FrameFormat_t *ff)` sólo está declarado en `nwk_util.h:321`; `APSDE_ParseMsg(...)` sólo está declarado en `aps_util.h:112-113`. La búsqueda en todo `source/ti/zstack` no encuentra definiciones fuente, y el `stack/nwk/nwk.c` citado por un registro anterior tampoco existe en este checkout. Una declaración no es una implementación reusable.

Sí permanecen útiles las máscaras y estructuras wire —por ejemplo NWK frame control en `nwk_util.h:111-155` y APS frame/delivery/security en `aps_mede.h:91-173`. Clasificación: **C** para parsers independientes basados en una revisión Zigbee fijada; **E** para adaptación de una implementación TI no visible.

### 7.4 ZMAC/TIMAC

`ZMacInit()` llama `MAP_MAC_Init/InitCoord/InitDevice` (`zmac.c:506-519`); asociación usa `MAP_MAC_MlmeAssociateReq()` (`:870-891`); data request reserva/copia y delega a `MAP_MAC_McpsDataReq()` (`:1160-1189`). Los callbacks reservan mensajes OSAL y los envían a la tarea NWK (`zmac_cb.c:165-240`). SysConfig selecciona `maclib_nosecure_*` y una librería Z-Stack por rol/toolchain (`zstack.syscfg.js:150-226`).

Clasificación: **D** para servicios MAC; no hay core fuente adaptable. El RF backend FeralRF no ofrece equivalentes de MLME/MCPS, PIB, confirmaciones, association, poll, scan, source match, ACK/retry o colas NWK.

### 7.5 Seguridad

Los headers y fuentes de seguridad muestran key sequence, frame counters, política, tablas y persistencia, pero esos elementos pertenecen al runtime. Son útiles para definir campos y pruebas, no para importar seguridad activa.

FeralRF ya implementa AES-CCM genérico one-shot (**A**) y rechaza un tag incorrecto (`crypto_engine.c:197-246`). Eso no constituye descifrado Zigbee: faltan selección de network/link key, EUI-64 origen, security control, frame counter, key sequence, AAD exacto, variantes CCM*, política de replay y persistencia. Además, la API actual sólo admite tags 4–16 pares (`radio.py:969-978`), no nivel sin MIC/tag cero. La orquestación Zigbee es **C condicional** para análisis host; seguridad activa de red es **D**.

## 8. Evaluación de las doce capacidades

### 8.1 Capacidades pasivas y de construcción

1. **Parsing MAC IEEE 802.15.4 — C.** FeralRF conserva los bytes, pero no los parsea. Un parser host debe manejar FCF/versiones, sequence suppression, PAN-ID compression, addressing, auxiliary security header, IEs y FCS según la revisión seleccionada. Input: `Packet.data` con FCS; output: campos + payload/slices + errores de truncación. Validación: vectores por combinación de addressing/FCF, malformed corpus y dissector independiente.

2. **Parsing NWK — C.** No existe en FeralRF y `NLDE_ParseMsg()` no tiene fuente visible. Las máscaras SDK y una especificación Zigbee fijada permiten parsear header base, extended source/destination, source route, multicast y security flag. No se debe inferir routing state de una sola trama.

3. **Parsing APS — C.** No existe y `APSDE_ParseMsg()` no tiene fuente visible. Puede implementarse por frame type/delivery mode, endpoints/group, cluster/profile, counter, extended header, fragmentación y security flag. Reensamble, ACK/duplicate state y binding son capas separadas.

4. **Parsing ZDO — B/C.** Los `ZDO_Parse*` son evidencia de field order, no son seguros con input hostil. Son defendibles esquemas acotados por APS cluster ID, separando TSN y command ASDU. La cobertura debe declararse por comando; “ZDO completo” no está soportado.

5. **Parsing de header ZCL — B.** La lógica de `zclParseHdr()` es pequeña y trazable; debe adaptarse con longitud. Puede ejecutarse enteramente en Python, usando APS profile/cluster/endpoints como contexto.

6. **Comandos/atributos ZCL — B/C.** Helpers de tipos y serialización son adaptables, pero foundation commands, cluster-specific commands y manufacturer data exigen schemas versionados. Unknown commands deben conservar raw payload, no rechazarse ni inventarse.

7. **Construcción/serialización — C, con helpers ZCL B.** Es viable construir MAC/NWK/APS/ZDO/ZCL sintácticos y producir un MPDU sin FCS para `Radio.transmit()`. Construcción válida no aporta association, counters aceptables, key state, CSMA, ACK ni respuesta. Secured construction requiere key/contexto y política de counters; sin ello sólo debe construirse una representación, no transmitirse como válida.

8. **Observación pasiva con estado — C.** En Python se pueden mantener mapeos short↔EUI-64, PAN/network IDs, devices observados, ZDO transactions, APS fragments, ZCL attributes y topología inferida. Cada entrada debe conservar provenance/timestamp y distinguir “observado”, “inferido” y “confirmado por varias tramas”. Pérdidas de captura impiden asumir exhaustividad.

9. **Interpretación de security headers — C.** Es posible exponer security control, key identifier/sequence, frame counter, source extendida presente/derivada, nivel/MIC esperado y ciphertext. La presencia de un header no autentica el frame.

10. **Descifrado autenticado — C condicional.** Puede vivir en host y usar una librería crypto verificada o la primitiva AES-CCM de FeralRF. Requiere clave autorizada, nonce/AAD exactos, address mapping y MIC. Sólo se entrega plaintext si la autenticación pasa. CCM* y niveles sin cifrado/MIC necesitan verificación específica; la primitiva actual no basta para todos los niveles.

11. **TX raw de tramas construidas — A para transporte, C para builder.** El bearer existe y evidencia histórica limitada observó tráfico IEEE raw. Antes de afirmar éxito deben verificarse bytes/FCS con un sniffer independiente y el resultado del peer. Un ACK host no prueba emisión ni aceptación. Nunca debe etiquetarse como “comando Zigbee enviado con éxito” sin respuesta/captura y contexto.

12. **MAC activo estrecho — D en conjunto.** El RF Core tiene piezas, pero FeralRF no implementa un servicio MAC. Timed TX es la candidata más estrecha para investigación; filtering/auto-ACK/CSMA/ACK/retry requieren estado y cadenas RF. Association/join quedan fuera de lo justificable con la arquitectura actual.

## 9. Factibilidad RF/MAC activa

| Mecanismo | FeralRF actual | Capacidad SDK/hardware visible | Servicio faltante y efecto | Clase / conclusión |
|---|---|---|---|---|
| Frame filtering | `frameFiltEn=0`; acepta todos los frame types (`smartrf_ieee_15_4_0.c:103-122`) | Filtros, PAN/local/ext/short lists en `rf_ieee_cmd.h:121-192` | Configuración de direcciones/PAN, actualización atómica, política de frames e impacto sobre sniffer | **D**. Activarlo perdería visibilidad raw y no equivale a MAC. |
| Auto-ACK | `autoAckEn=0`, `slottedAckEn=0` (`smartrf...:105-106`) | Auto-ACK y contador `nTxAck` (`rf_ieee_cmd.h:125-148,581-590`) | Filtering coherente, frame pending/source match, addresses, seguridad y semántica de confirmación | **D**. Mecanismo visible, integración no demostrada. |
| CCA | Opciones CCA del RX en cero (`smartrf...:123-129`) | `CMD_IEEE_CCA_REQ` y CCA state (`rf_ieee_cmd.h:548-573`) | Política, umbrales, lectura durante RX y error/completion host | **D**. No usado. |
| CSMA-CA | TX usa sólo `CMD_IEEE_TX` | `CMD_IEEE_CSMA`, NB/BE/max backoffs (`rf_ieee_cmd.h:305-356`) | Instanciar/encadenar comandos, random state, CCA config, statuses y retry policy | **D**. Header no es implementación FeralRF. |
| TX temporal | `startTrigger=TRIG_NOW`, `pastTrig=1`; `startTime` no usado (`radio_if.c:479-484`) | `startTime` y triggers en cada RF op (`rf_ieee_cmd.h:270-300`) | Timebase RAT↔host, wrap, deadlines, completion y scheduling con otros PHY | **B condicional** como primitiva; no como MAC. |
| Espera de ACK | No existe; TX termina al completar `CMD_IEEE_TX` | `CMD_IEEE_RX_ACK` con `seqNo` y timeout (`rf_ieee_cmd.h:360-394`) | Chain TX→RX_ACK, sequence ownership, timeout/status, RX restoration | **D**. |
| RX→TX turnaround | TX se rechaza cuando `s_rx_running` (`radio_if.c:2182-2187`) | RF operations pueden encadenarse mediante `pNextOp/condition` | Cadena en RF Core, buffers compartidos, deadlines y retorno a RX | **D**. Polling host/RTOS actual no cumple por sí solo timing MAC. |
| Callbacks/completion | RX callback marca evento; TX `RF_runCmd()` bloquea | RF Driver ofrece eventos/comandos | Correlación entre request y terminal result; status detallado; saturación | **A** a nivel RF básico, insuficiente para MAC. |
| Retries | Ninguno | Pueden construirse cadenas/software sobre CSMA/RX_ACK | contador, policy, duplicate handling, cancelación y fairness | **D**. |
| Scan/association/poll | Ausentes | ZMAC llama TIMAC MLME binario (`zmac.c:870-1123`) | PIB, beacon scan, association state, timers, MAC commands, persistence | **D**. No portar wrappers sin TIMAC. |
| Seguridad MAC/NWK activa | Ausente | Z-Stack mantiene keys/counters/tablas/NV | anti-replay, counters monotónicos, key update, reset/power-loss | **D**. AES-CCM aislado no sustituye el servicio. |

### 9.1 Qué sí puede investigarse sin prometer un MAC

- **Timed raw TX** con RAT explícito es una primitiva fuente-visible; debe probar exactitud, wrap, late trigger y coexistencia con RX/otros PHY.
- **CCA observacional** podría exponer información del canal, pero no debe etiquetarse CSMA hasta implementar y validar el algoritmo/política.
- **Filtering o auto-ACK aislados** no son recomendables como primer cambio: alteran el comportamiento de sniffer, requieren identidad MAC y crean una apariencia de nodo sin confirmations/retries/association.
- **Cadenas TX→RX_ACK** son técnicamente concebibles con los comandos visibles, pero siguen siendo un nuevo servicio operacional y no están soportadas por evidencia de ejecución.

## 10. Matriz de capacidad, integración y validación

| Capacidad / capa | Fuente/función exacta | Interfaz FeralRF / ubicación candidata | Clase | Dependencias y modificación necesaria | Riesgo / evidencia faltante | Validación mínima antes de afirmar funcionamiento |
|---|---|---|---|---|---|---|
| MAC parse / IEEE | `firmware/cc1352/src/radio_if.c:1347-1355,1497-1560`; `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/devices/cc13x2x7_cc26x2x7/driverlib/rf_ieee_cmd.h:610-620` | `Packet.data`; Python | **C** | Parser bounded, revisión IEEE, flag FCS-present | FCF/version/IE coverage; timestamp/LQI contract | Golden vectors, truncación/fuzz, FCS good/bad y comparación independiente. |
| NWK header / NWK | `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/stack/nwk/nwk_util.h:111-155,321`; definición `NLDE_ParseMsg` ausente | Payload MAC; Python | **C** | Esquema por revisión, extended/source-route/multicast/security | Implementación TI oculta; revisión Zigbee no fijada | Vectores de todas las flags y malformed lengths; comparación con dissector fijado. |
| APS header / APS | `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/stack/nwk/aps_mede.h:91-173`; `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/stack/nwk/aps_util.h:112-113`, definición ausente | Payload NWK plaintext; Python | **C** | Delivery modes, frame types, fragmentation/security | APS encrypted necesita contexto; parser TI oculto | Data/cmd/ack, group/unicast, fragments y negative vectors. |
| ZDO messages / ZDO | `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/stack/zdo/zd_object.c:2914-3733`; `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/stack/zdo/zd_object.h:399-602` | APS profile 0x0000; Python | **B/C** | Parsers por cluster con bounds; reemplazar OSAL alloc/ownership | Counts no acotados en TI; cobertura parcial | Vectores request/response por cluster, counts 0/max/truncados, TSN. |
| ZCL header / ZCL | `zclParseHdr()`, `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/stack/zcl/zcl.c:2412-2446` | APS payload+contexto; Python | **B** | Longitud explícita; output inmutable/error | TI lee sin bounds | Headers 3/5 bytes, direcciones, unknown/reserved bits, truncación. |
| ZCL commands/attrs / ZCL | `zclSerializeData()`, `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/stack/zcl/zcl.c:2961-3076`; lengths `:3195-3322` | Parser/builder host | **B/C** | Tipos/schemas versionados, writer bounded, manufacturer fallback raw | Revisión/cluster coverage no definida | Round-trip/endian, strings límite, tipos, foundation/cluster vectors. |
| Frame build / varias | `zclBuildHdr()`, `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/stack/zcl/zcl.c:2459-2482`; `firmware/cc1352/src/radio_if.c:468-488` | Builder Python→`Radio.transmit()` | **C** | Sequence/counters como input explícito; MPDU ≤125 sin FCS | “Sintáctico” puede no ser aceptable en red | Round-trip parser, captura independiente byte/FCS, peer sólo en entorno autorizado. |
| Estado pasivo / varias | `python/feralrf/radio.py:18-30`; `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/stack/zdo/zd_object.c:3607-3618`; `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/stack/nwk/aps_mede.h:91-173` | Servicio Python sobre stream | **C** | Store por sesión/PAN, provenance, expiración/reassembly | Pérdidas, aliases, address reuse | Capturas ordenadas/desordenadas/duplicadas, gaps, restart y fixtures multi-device. |
| Security header / NWK/APS | `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/stack/nwk/nwk_globals.h:144-145,458-466`; `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/stack/nwk/aps_mede.h:105,231-234`; `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/stack/sec/` | Parser Python | **C** | Definir nonce inputs/MIC length/key id; preservar ciphertext | Source EUI puede faltar; nivel/revisión no fijados | Known headers, key sequence transitions, malformed aux headers. |
| Authenticated decrypt / security | `python/feralrf/radio.py:943-1004`; `firmware/cc1352/src/crypto_engine.c:197-246` | Preferido Python; opcional primitiva CC1352P7 | **C cond.** | Key autorizada, nonce/AAD/CCM* exactos, MIC obligatorio | API actual no cubre tag 0; no hay key/context service | Known-answer CCM*, captura conocida, wrong key/counter/AAD/MIC deben fallar. |
| Raw TX / IEEE | `Radio.transmit()`, `python/feralrf/radio.py:725-735`; `firmware/cc1352/src/radio_if.c:468-488,2182-2204` | Existente host→RF | **A** | Builder entrega MPDU sin FCS; completion mejor definido para claim fuerte | ACK anticipado; RX debe estar detenido; sin CCA/ACK/retry | Sniffer independiente, límites 1/125, FCS, canal, failure injection. |
| MAC activo estrecho / IEEE | `firmware/cc1352/src/smartrf_ieee_15_4_0.c:81-163`; `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/devices/cc13x2x7_cc26x2x7/driverlib/rf_ieee_cmd.h:80-207,266-394,548-620`; `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/zstack/zmac/f8w/zmac.c:506-1218` | `RadioIF` firmware, sólo si se justifica | **D** | Estado MAC, RF chains, timings, callbacks, buffers, protocolo/completion, regresión multi-PHY | No implementación, no pruebas temporales, TIMAC core oculto | Analizador lógico/RF, timing ACK/CCA/turnaround, peer independiente, carga/overflow y regresión completa. |

## 11. Impacto de cambios embedded sobre FeralRF

Aunque este informe no prescribe implementación, cualquier candidato firmware debe evaluarse contra:

- **PHY existentes:** `RadioIF` conmuta BLE/IEEE/sub-GHz sobre handles persistentes; una cadena IEEE mal cancelada puede bloquear otros modos.
- **Memoria:** tres entradas RF, ocho paquetes app, buffers TX estáticos y dos stacks de 4096 bytes ya existen. No hay mapa actual que pruebe margen para parsers/estado adicional.
- **Scheduling:** parsing o crypto bloqueante en la tarea RF aumenta riesgo de overflow y starvation UART.
- **Ownership:** auto-ACK/filtering modifican la operación background RX; no son funciones laterales sin efecto.
- **Transporte:** el payload COBS máximo y layout RX no tienen campos Zigbee estructurados. Añadir resultados embedded implicaría versionado/capability discovery; no se debe sobrecargar silenciosamente `Packet`.
- **Completion:** la aceptación anticipada actual es insuficiente para operaciones MAC con timeout/retry/ACK.
- **Hardware:** la callback y DIO de antena/front-end deben seguir siendo la autoridad; disponibilidad de un comando RF no prueba route/power/timing en CatSniffer.

Por estas razones, no existe ventaja probada en mover parsing pasivo al CC1352P7. El host preserva RAM/CPU RF, permite bounds/fuzzing y mantiene el byte raw como evidencia.

## 12. Prioridades de investigación basadas en evidencia

Estas prioridades expresan valor y dependencia técnica, no un calendario de implementación.

| Prioridad | Capacidad | Justificación | Dependencia / relación |
|---:|---|---|---|
| 1 | Fijar contrato raw y revisiones objetivo | Toda capa superior depende de FCS, revisión IEEE/Zigbee/ZCL y corpus | Fundamento secuencial. |
| 2 | Parser pasivo bounded MAC→NWK→APS | Máximo valor con cero cambio firmware y validación independiente | Depende de 1; fundamento acumulativo. |
| 3 | ZDO/ZCL declarativos, tipos y unknown-raw | Aprovecha helpers/field order visibles sin runtime TI | Depende de 2; acumulativo. |
| 4 | PCAPNG/comparación y corpus malformed | Hace reproducibles las afirmaciones y reduce sesgo del parser propio | En paralelo con 2–3 una vez fijado el contrato. |
| 5 | Estado pasivo: mapeos, fragments, transactions, topology/attrs | Añade valor analítico sin convertir el dispositivo en nodo | Depende de parsers y de manejo explícito de gaps. |
| 6 | Security headers y decryption autenticado autorizado | Útil, pero sólo después de campos/contexto y known-answer vectors | Security header tras 2; decryption tras 5/contexto. |
| 7 | Builders y TX raw verificado independientemente | Reusa TX existente; separa sintaxis de operación de red | Depende de parsers/round-trip y contrato FCS. |
| 8 | Timed raw TX/CCA como primitivas RF experimentales | Únicas extensiones embedded estrechas con mecanismo visible | Rama condicional; exige completion/timebase/regresión. |
| 9 | Auto-ACK, TX→RX_ACK, CSMA/retries | Mayor riesgo y estado; no son necesarios para análisis pasivo | Rama alternativa/condicional después de 8; no están justificados hoy. |
| No priorizar | Association, join, commissioning, routing o nodo Zigbee operacional | Requieren un MAC/NWK/APS/security/NV runtime que FeralRF no posee | Fuera del máximo alcance defendible actual. |

## 13. Validación requerida antes de implementación o claims

### 13.1 Contrato y parser

1. Fijar explícitamente la revisión IEEE 802.15.4, Zigbee Core y ZCL que se soportará.
2. Verificar con captura independiente que RX entrega MPDU+FCS y TX recibe MPDU−FCS en canales 11 y 26, longitudes límite y reinicios de sesión.
3. Construir corpus legal/versionado con frames plaintext, secured, malformed, truncados, duplicados y fragmentados.
4. Comparar cada capa con al menos un dissector independiente fijado por versión; divergencias se resuelven contra especificación, no por mayoría.
5. Ejecutar tests de bounds/fuzz/round-trip sin instalar ni ejecutar firmware durante la fase documental.

### 13.2 Seguridad

1. Vectores known-answer por security level/key identifier/MIC length soportado.
2. Casos negativos obligatorios: key, source EUI, frame counter, security control, AAD y MIC incorrectos.
3. Plaintext sólo tras MIC válido; de otro modo output ciphertext/MIC + razón.
4. Manejo seguro de claves y borrado/logging; ninguna captura implica autorización para obtener claves.
5. Demostrar por separado cualquier uso de AES-CCM del CC1352P7 y su equivalencia con CCM* requerido.

### 13.3 Construcción y TX

1. Parser(builder(x)) debe ser equivalente por campo y bytes.
2. Sniffer independiente debe observar MPDU y FCS esperados.
3. Distinguir `accepted`, `RF completed`, `ACK observed` y `application response`.
4. Validar errores de longitud, RX activo, canal inválido, overflow y pérdida UART.
5. No afirmar interoperabilidad Zigbee a partir de una trama observada.

### 13.4 Primitivas MAC embedded

Antes de considerar integración deben demostrarse:

- exactitud RAT, wrap y late-trigger;
- CCA/CSMA status y backoff medidos;
- turnaround y ACK dentro del tiempo requerido con instrumentación independiente;
- retry/cancel/completion exactamente una vez y correlacionados;
- comportamiento con RX queue llena, UART cargado y callbacks concurrentes;
- restauración de captura pasiva tras cada operación;
- regresión BLE, IEEE raw y sub-GHz;
- consumo de flash/RAM/stack a partir de map y high-water marks;
- funcionamiento de antenna switch/power en hardware CatSniffer.

Sin esas demostraciones, el estado correcto es **mecanismo SDK disponible, integración no verificada**.

## 14. Preguntas no resueltas

1. ¿Qué revisión exacta de IEEE 802.15.4, Zigbee Core y ZCL exige el objetivo académico?
2. ¿Qué subset de clusters ZDO/ZCL y manufacturer extensions debe declararse soportado?
3. ¿Cuál es el evento RF exacto del timestamp, cómo se extiende RAT tras wrap y qué pérdida existe bajo carga?
4. ¿Debe el API conservar siempre FCS o transportar una bandera versionada? El código actual lo conserva en RX y lo excluye en TX, pero el contrato público no lo explicita.
5. ¿Cuál es el margen real de flash, heap y stack? El source/linker no sustituye map y high-water measurements.
6. ¿Qué variante CCM* y security levels se requieren, y puede la primitiva AES-CCM actual representar todos? Tag cero no está soportado actualmente.
7. ¿Qué corpus autorizado y qué versiones de Wireshark/Scapy u otro dissector servirán de oráculo comparativo?
8. ¿Qué política de provenance/expiry evita presentar topología o attributes inferidos como estado actual?
9. ¿Existe un requisito académico concreto para timed TX, CCA o ACK que justifique tocar `RadioIF`?
10. ¿Qué licencia y política de redistribución se aplicará si se adapta código TI en vez de reimplementar desde definiciones oficiales? Debe revisarse archivo por archivo.
11. ¿Cómo se corregirá la semántica de completion antes de cualquier claim MAC activo?

## 15. Decisiones técnicas que pueden aprobarse ahora

1. **Preservar FeralRF como único firmware y propietario RF.** No introducir una persona Zigbee separada.
2. **Conservar el MPDU raw como fuente de verdad.** Los resultados estructurados son derivados y deben retener offsets/raw payload.
3. **Ubicar análisis pasivo en Python.** Es la frontera de menor riesgo y mayor verificabilidad.
4. **Reusar selectivamente algoritmos, no translation units.** ZCL y ZDO pequeños se adaptan con bounds o sirven de referencia; NWK/APS se implementan independientemente porque su parser TI no es visible.
5. **Separar cinco claims:** decode pasivo, construcción, inyección raw, servicio MAC y participación Zigbee. Ninguno implica automáticamente el siguiente.
6. **Mantener crypto como servicio genérico.** No etiquetarlo Zigbee hasta que el host construya y valide contexto/CCM*.
7. **Diferir MAC activo.** Timed TX es investigable; auto-ACK/CSMA/ACK/retries no se promueven a integración hasta satisfacer la validación de la sección 13.4.

## 16. Respuesta a la pregunta final de investigación

**¿Qué capacidades Zigbee estrechas pueden justificarse para integrarse en FeralRF existente, con la implementación real y el source visible del SDK fijado, sin reemplazar FeralRF ni inventar comportamiento?**

Se justifican, con trabajo y validación aún pendientes:

- un decoder host bounded de MAC IEEE 802.15.4, NWK y APS (**C**), usando `Packet.data` y definiciones versionadas;
- parsers ZDO y ZCL por comando/schema, adaptando de forma endurecida los algoritmos y field order visibles de `zcl.c`/`zd_object.c` o implementándolos independientemente (**B/C**);
- interpretación de security headers, conservación de ciphertext/MIC y estado pasivo con provenance (**C**);
- descifrado autenticado opcional con claves autorizadas y contexto completo (**C condicional**), sin confundir AES-CCM genérico con seguridad Zigbee completa;
- builders sintácticos MAC/NWK/APS/ZDO/ZCL y raw TX mediante la interfaz existente (**C + A**), siempre sin claim de asociación o aceptación de red;
- PCAP/PCAPNG y comparación independiente como parte de la validación host (**C**).

No se justifica hoy afirmar o incorporar como capacidad funcional:

- `NLDE_ParseMsg()` o `APSDE_ParseMsg()` TI, porque no hay definición fuente visible;
- ZMAC/TIMAC, association, MLME/MCPS, join, routing, commissioning, BDB o seguridad/NV activos, porque requieren servicios operacionales y binarios/configuración ausentes (**D/E**);
- auto-ACK, CSMA, espera de ACK, retries o RX→TX turnaround sólo porque `rf_ieee_cmd.h` contiene comandos. Esas funciones son **D** hasta diseñar estado/completion y demostrar timing;
- cualquier claim de conformance o interoperabilidad Zigbee basado en RX/TX raw histórico.

En consecuencia, el **máximo alcance defendible actual** es un analizador/constructor Zigbee nativo dentro del proyecto FeralRF, con captura y transmisión IEEE raw existentes, más crypto autenticado condicional. FeralRF seguiría siendo un instrumento raw protocol-aware, no un nodo Zigbee operacional. Cualquier expansión hacia MAC activo debe aprobarse como rama de investigación separada y basada en evidencia temporal, no como consecuencia implícita de los headers del SDK.
