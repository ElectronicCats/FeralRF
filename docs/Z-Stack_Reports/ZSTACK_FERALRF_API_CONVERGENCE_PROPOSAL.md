# Propuesta de convergencia de APIs FeralRF + TI Z-Stack

**Fecha de evaluación:** 2026-11-02

**Repositorio FeralRF examinado:** `bfe056cc323911b6fc4c6f224d1f4f626ee15942` (`feature/hardware-validation`)

**Revisión auditada de referencia:** `8a4a8d8194da7f4b2b5c5a4a221b749cc2fa882b`

**Submódulo TI fijado:** `5b31d0a4903351e544546e23ef3330eaa4291ceb` (`lpf2-8.30.01.01`)

**SDK:** SimpleLink CC13xx/CC26xx SDK `8.30.01.01`; Z-Stack User's Guide `8.30.00`

**Procesador objetivo:** CC1352P7

**Naturaleza del documento:** propuesta técnica y priorización; no es diseño de implementación, plan de trabajo ni evidencia de interoperabilidad.

El registro de fuentes y la correspondencia de identificadores usados aquí se encuentran en [ZSTACK_FERALRF_API_CONVERGENCE_SOURCES.md](ZSTACK_FERALRF_API_CONVERGENCE_SOURCES.md).

## 1. Resumen ejecutivo

La máxima convergencia defendible con la evidencia actual no es unir FeralRF y Z-Stack dentro del mismo runtime. Es presentar, mediante una API de host coherente pero honesta, tres capacidades distintas:

1. **FeralRF actual como backend IEEE 802.15.4 raw:** captura de MPDU con FCS, metadatos y transmisión de MPDU sin FCS, conservando además las demás PHY y funciones raw de FeralRF.
2. **Análisis Zigbee pasivo en Python:** decapsulación MAC/NWK/APS, ZDO/ZCL visible, análisis de cabeceras de seguridad, estado de observación y exportación PCAP/PCAPNG; no requiere modificar el firmware para recibir las tramas CRC-válidas que ya entrega.
3. **Z-Stack operacional como firmware separado:** preferentemente una persona ZNP si el objetivo dominante es controlar Zigbee desde Python; alternativamente una persona ZED si se busca demostrar primero un dispositivo final autónomo y de alcance cerrado.

Esta separación maximiza el valor académico y reduce afirmaciones indebidas. La captura raw demostrada entre dos CatSniffer no acredita interoperabilidad Zigbee. La disponibilidad del código y de librerías Z-Stack tampoco acredita que puedan enlazarse con el target GNU actual. [EXP-01][SDK-04]

La prioridad recomendada es:

1. fijar como contrato estable la frontera IEEE 802.15.4 ya existente;
2. construir y validar conceptualmente la capacidad pasiva en host, incluyendo interoperabilidad de archivos;
3. reutilizar sólo algoritmos/definiciones ZCL y ZDO pequeños, endurecidos y con revisión de licencia, o emplearlos como referencia para una implementación independiente;
4. definir una abstracción de host con descubrimiento de capacidades y selección explícita de backend/persona;
5. evaluar una persona ZNP separada para Zigbee operacional; usar ZED si la pregunta experimental es más estrecha;
6. mantener imagen única, conmutación in-process y DMM como ramas condicionadas, no como arquitectura base.

La arquitectura recomendada preserva FeralRF completo cuando se ejecuta la persona FeralRF y aísla la propiedad del RF cuando se ejecuta la persona ZNP/ZED. No promete simultaneidad. El precio es una selección o reflasheo explícito de firmware, o hardware separado, a cambio de una frontera verificable y mantenible.

## 2. Lenguaje de evidencia

Las conclusiones usan estas clasificaciones:

| Etiqueta | Significado |
|---|---|
| **CONFIRMADO-CÓDIGO** | Visible en el código FeralRF del HEAD actual. |
| **CONFIRMADO-SDK** | Visible en el submódulo fijado o en documentación oficial de la misma versión. |
| **EVIDENCIA-HISTÓRICA** | Observado en una campaña retenida, con sus limitaciones originales. |
| **FACTIBLE-CONDICIONAL** | La arquitectura es técnicamente viable si se satisfacen condiciones identificadas; no está implementada ni validada. |
| **PLAUSIBLE-NO-VERIFICADO** | Compatible con el contrato conocido, pero faltan implementación, corpus o experimento. |
| **BLOQUEADO** | Una dependencia o incompatibilidad demostrada impide sostener la opción en el target actual. |
| **NO-SOPORTADO** | No existe evidencia suficiente para afirmar la capacidad. |

“Implementado”, “compilable”, “ejecutado”, “validado” e “interoperable” se consideran estados diferentes.

## 3. Línea base arquitectónica real

### 3.1 Repositorio, cambios y SDK

| Comprobación | Resultado | Efecto sobre esta propuesta |
|---|---|---|
| HEAD actual | `bfe056cc323911b6fc4c6f224d1f4f626ee15942` | Es posterior a `8a4a8d8`; debe citarse como línea base actual. |
| Diferencia de implementación desde `8a4a8d8` | Sólo se añadió `docs/ZSTACK_PARTIAL_REUSE_AND_ZIGBEE_DECODER_AUDIT.md`; no cambió `firmware/`, `python/`, CMake, configuración generada ni puntero del SDK | Las conclusiones de código de las auditorías siguen aplicando al HEAD, salvo correcciones documentales. **CONFIRMADO-CÓDIGO**. |
| Estado del árbol antes de esta propuesta | `docs/ZSTACK_PARTIAL_REUSE_AND_ZIGBEE_DECODER_AUDIT.md` modificado localmente | Cambio preexistente preservado; no se alteró en esta investigación. |
| SDK | `5b31d0a4903351e544546e23ef3330eaa4291ceb` | Coincide con el baseline solicitado. |
| Target activo | `CC1352P7`, `DeviceFamily_CC13X2X7`, Cortex-M4F hard-float | Coincide con el dispositivo evaluado. [FW-14] |
| Composición del target | Fuentes FeralRF + TI Drivers/DriverLib/RF/TI-RTOS; no incluye Z-Stack, TIMAC, OSAL Port, BDB, NVOCMP ni configuración Z-Stack | No hay Z-Stack oculto detrás de la API FeralRF. [FW-14] |

### 3.2 Estado de las correcciones del informe del 30 de octubre

La copia de trabajo del informe contiene una sección explícita `Audit Corrections and Evidence Reconciliation` y corrige materialmente seis grupos: nombres reales de `Packet`, terminología de heaps, pinning de fuentes externas, separación capacidad/validación, tratamiento de cifrado/MIC y rutas de evidencia. **Las correcciones A–F están incorporadas en el contenido local**, pero el archivo continúa modificado y no confirmado en Git; además conserva incertidumbres expresas. Por tanto:

- se aceptan sus correcciones técnicas como evidencia de trabajo actual;
- no se interpreta el estado local como publicación o aprobación definitiva;
- los puntos pendientes —timestamp RF exacto, memoria libre medida, corpus Zigbee, compatibilidad ABI, decryption vectors y versiones externas— se mantienen como **requieren reconciliación/validación**, no como errores ya resueltos. [AUD-03]

### 3.3 Hardware y recursos

El CC1352P7 dispone oficialmente de 704 KiB de flash y 144 KiB de SRAM principal más 8 KiB de cache/GPRAM utilizable según configuración; la página de producto expresa 152 KiB agregados. Esto demuestra capacidad física general, no espacio libre del binario FeralRF ni suficiencia para una imagen combinada. [DOC-TI-01][DOC-TI-03]

El linker FeralRF declara 144 KiB de SRAM y 8 KiB de GPRAM y reserva un heap primario de 16 KiB; dos stacks de tarea de 4096 bytes son asignaciones separadas. El `app_release.cfg` Z-Stack configura un heap OSAL de 0x1800 bytes, pero no está activo en FeralRF y no puede sumarse como estimación de integración. No hay mapa de enlace o medición runtime que establezca margen real. [FW-15][SDK-11][AUD-03]

La configuración generada está basada en `LP_CC1352P7_4` y controla DIO28/DIO29/DIO30 como selección 2.4 GHz/High-PA/Sub-1 GHz mediante callback global del RF Driver. Una persona ZNP/ZED/DMM debe reproducir deliberadamente esta configuración para el CatSniffer; seleccionar sólo el MCU no resuelve el front-end. [HW-01]

## 4. Qué significa “convergencia de APIs”

### 4.1 Tres objetivos distintos

| Objetivo | Frontera real | Puede coexistir con | No implica |
|---|---|---|---|
| **A. Integración de capacidades** | `Packet.data`/metadatos y TX raw de FeralRF alimentan captura, construcción y análisis Zigbee en host | B; C mediante backends separados | Z-Stack en firmware, asociación, ACK, joining ni conformidad |
| **B. Reuso selectivo** | Algoritmos/definiciones ZCL/ZDO delimitados se adaptan o sirven de referencia | A; C | Compilar `zcl.c` o `zd_object.c` completos; obtener NWK/APS/TIMAC |
| **C. Integración operacional** | Runtime completo Z-Stack posee MAC/NWK/APS/ZDO/BDB/seguridad y se expone por app local o ZNP | A/B sólo como personas/backends separados; DMM únicamente si se demuestra | Compatibilidad wire con COBS FeralRF; preservación simultánea de todas las PHY |

A y B son acumulativos. C es acumulable en la experiencia de host, pero en el estado actual es una alternativa de firmware y de propiedad RF, no un módulo que se inserta en FeralRF.

### 4.2 Principio rector

Una API unificada debe unificar **semántica de uso**, no ocultar diferencias de backend. Debe informar si una operación es:

- captura raw;
- decodificación pasiva;
- construcción de bytes;
- transmisión raw sin garantías de MAC;
- operación de red ejecutada por Z-Stack.

La igualdad del nombre de una operación no vuelve iguales sus garantías. Un ACK del protocolo COBS actual confirma aceptación/encolado en varios caminos, no necesariamente terminación RF; una SRSP/AREQ MT tiene otra correlación y otro ciclo de vida. [FW-03][SDK-10]

## 5. Mapa de APIs y responsabilidades actuales

### 5.1 Contrato Python y transporte FeralRF

| Elemento existente | Runtime | Entrada/salida real | Dependencias | Estado para Zigbee |
|---|---|---|---|---|
| `Radio.set_phy()` / `set_channel()` | Python → CC1352P7 | Selecciona `PHY.IEEE_802_15_4=4` y canal; respuesta ACK/ERROR | Serial, COBS, CRC16, `CommandProcessor`, `PhyManager` | Reutilizable para raw; no selecciona un stack Zigbee. [FW-01][FW-02] |
| `Radio.start_rx()` / `stop_rx()` | Python → CC1352P7 | Solicita inicio/parada; ACK se emite antes de la ejecución diferida | `ControlTask`, `DataTask`, RF Driver | Reutilizable con semántica documentada; no es commissioning. [FW-01][FW-03] |
| `Radio.read_packets()` | Python | Produce `Packet` o `RxStreamError` | `RSP_RX_PACKET`, parser de longitud, metadatos | Frontera adecuada para decoder host. [FW-01] |
| `Packet` | Python | `timestamp_us`, `channel`, `rssi_dbm`, `lqi`, `crc_ok`, `data`, metadatos LL opcionales | Contrato de respuesta | `data` es MPDU con FCS, sin PHR en IEEE; no contiene árbol Zigbee. [FW-01][FW-09] |
| `Radio.transmit()` | Python → CC1352P7 | MPDU caller-supplied; ACK de aceptación/encolado | `TX_RAW`, `ControlTask`, `RadioIF` | Inyección raw; no equivale a entrega Zigbee. [FW-01][FW-03] |
| `Radio.transmit_frame()` | Python → CC1352P7 | En código actual delega al mismo raw path | `TX_FRAME`, `ControlTask_onTxFrame()` | El nombre sobreexpresa capacidad; no añade framing Zigbee. [FW-04] |
| COBS + CRC16 | Python/RP2040/CC1352 | `CMD|SEQ|LEN|PAYLOAD|CRC16`, COBS y delimitador 0 | RP2040 como puente, UART 921600 | Transporte propio; no wire-compatible con NPI/MT. [FW-12][HW-02] |

### 5.2 Componentes firmware FeralRF

| Componente existente | Controla | Contrato relevante | Propiedad/estado | Reuso o adaptación |
|---|---|---|---|---|
| `CommandProcessor_processEncodedFrame()` | Validación y despacho de comandos | COBS/CRC ya decodificado; ACK/ERROR y respuestas | Ejecuta en contexto atendido por `DataTask` | Reusar para persona FeralRF; no envolver MT sin un gateway explícito. [FW-03] |
| `ControlTask` | Estado de PHY/canal/potencia y solicitudes RX/TX | Eventos y buffers estáticos; `TX_FRAME→TX_RAW` | No implementa MAC | Mantener en raw; reemplazado por app/ZNP en persona Z-Stack. [FW-04] |
| `DataTask_poll()` | Ordena comando, control, RF poll y emisión RX | `RadioIF_RxPacket` → `RSP_RX_PACKET` | Productor de eventos asíncronos | Frontera de captura actual; decode en firmware aportaría poco y exigiría nuevo wire contract. [FW-05] |
| `PhyManager` | Tabla PHY→LL/backend | IEEE usa LL por defecto y backend RF | No estado Zigbee | Reutilizable sólo en FeralRF. [FW-06] |
| `LLManager_processRxPacket()` | Clasificación BLE o pass-through | IEEE pasa sin parsear | No NWK/APS/ZCL | Punto conceptual de inserción, pero no recomendado para decode host. [FW-07] |
| `RadioIF` | RF handles, comandos, colas, RX/TX, metadatos | MPDU raw y `RadioIF_RxPacket` | Dueño actual del RF Driver y scheduling | Debe ceder propiedad completa en persona Z-Stack o ser rediseñado bajo DMM. [FW-08][FW-09] |
| Configuración SysConfig/antena | RF Driver y front-end | Callback según `RadioSetup` y PA | DIO28–30 | Debe portarse a cualquier target Z-Stack. [HW-01] |

### 5.3 Recorrido IEEE 802.15.4 exacto

```text
Python Radio.set_phy(IEEE_802_15_4, canal)
  -> COBS/CRC16 -> CommandProcessor -> ControlTask -> RadioIF_setPhy()
  -> RadioIF_startIeee154RfBackend()
     RF_open / RadioSetup / FS / CMD_IEEE_RX
     sin filtering, sin auto-ACK, incluye PHR+FCS+RSSI+corr/CRC+timestamp en cola RF
  -> RadioIF_processIeee154Packets()
     usa PHR como longitud, descarta PHR, descarta frames CRC-error,
     conserva MPDU incluido FCS y encola RadioIF_RxPacket
  -> LLManager pass-through -> DataTask_emitRxPacket()
  -> RSP_RX_PACKET -> Python Packet.data
```

TX toma como máximo 125 bytes de MPDU sin FCS, configura `bIncludePhyHdr=0` y `bIncludeCrc=0`, y deja que RF Core genere PHR/FCS. RX devuelve el FCS. Esta asimetría es válida si se documenta, pero obliga a cualquier constructor/reinyector a conocer el contrato. [FW-08][FW-09][FW-10]

### 5.4 Límites Z-Stack

| Subsistema | Runtime y función | Contrato/entrada | Dependencias importantes | Uso directo por FeralRF |
|---|---|---|---|---|
| TIMAC/ZMAC | CC1352P7; MAC 802.15.4 operativo | primitivas MAC, callbacks a NWK | librería TIMAC, OSAL Port, RF Driver, timers, estado | No; sólo wrappers/callbacks visibles, implementación principal binaria. [SDK-04][SDK-05] |
| NWK | CC1352P7; routing, addressing, seguridad NWK | primitivas desde MAC y hacia APS | archivo `libZStack_nwk_<role>.a`, globals, NV, timers | No; `NLDE_ParseMsg()` sólo declarado. [SDK-04][SDK-06] |
| APS | CC1352P7; entrega, endpoint, ACK/retry/fragmentación | frames NWK y AF/ZDO/ZCL | NWK, seguridad, heap/OSAL, estado | No; `APSDE_ParseMsg()` sólo declarado. [SDK-06] |
| AF | CC1352P7; endpoints y mensajes app | registros/AF requests e indications | APS, OSAL messages, descriptors | No como codec independiente. [SDK-07] |
| ZDO | CC1352P7; descubrimiento/gestión | `zdoIncomingMsg_t`, ASDU y estado | AF/NWK, OSAL alloc, tablas | Sólo algunas reglas de campos son reutilizables con adaptación. [SDK-03] |
| ZCL | CC1352P7 app/stack; comandos y atributos | mensajes AF y buffers ZCL | AF, OSAL, config, clusters | Sólo helpers de bytes pequeños; translation unit completo no. [SDK-02] |
| BDB | CC1352P7; commissioning y políticas Zigbee 3.0 | eventos/estado/config | ZDO/NWK/seguridad/NV/timers | No; requiere runtime. |
| Seguridad/NV | CC1352P7; claves, counters, persistencia | contextos de red y almacenamiento | crypto drivers, NVOCMP, CCFG, flash | No; FeralRF crypto genérico no sustituye estado Zigbee. [SDK-08] |
| Z-Stack API | app task ↔ stack task | mensajes `zstackmsg_*` | OSAL Port, service task, heap | No wire API y no API independiente. [SDK-07] |
| NPI/MT/ZNP | host ↔ firmware ZNP | frames MT, SREQ/SRSP/AREQ y subsistemas SYS/AF/ZDO/UTIL/APP_CNF | NPI task/UART, Z-Stack completo, configuración ZNP | Útil sólo como backend separado/adaptador; incompatible con COBS actual. [SDK-09][SDK-10][DOC-TI-05] |
| TI-RTOS/OSAL/config generado | soporte de ejecución | tasks, messages, timers, heap y macros de rol | SysConfig, linker, toolchain | No está configurado en target FeralRF. [SDK-11][DOC-TI-04] |

## 6. Evaluación por nivel de capacidad

### 6.1 Nivel 0 — fundamento IEEE 802.15.4 existente

**Confirmado por código:**

- recepción monocal de frames aceptados por `CMD_IEEE_RX`;
- filtrado de frames con bit de error CRC antes de Python;
- eliminación del PHR y preservación del MPDU completo con FCS;
- exposición de canal, RSSI, LQI/correlación, `crc_ok` y timestamp RAT dividido entre cuatro;
- transmisión de MPDU caller-supplied, máximo 125 bytes, sin PHR/FCS suministrados por el host;
- selección de canales 11–26, aunque el firmware también remapea 0–15 a 11–26;
- colas finitas: tres entradas RF y ocho paquetes de aplicación; la preservación por frame no implica captura sin pérdidas. [FW-08][FW-09][FW-10]

**Evidencia histórica:** RAW bidireccional y FRAME unidireccional fueron observados OTA entre dos CatSniffer en canal 25; el informe reporta 20/20, 10/10 y repeticiones posteriores dentro de su alcance. BURST, temporización positiva, cese inmediato y estabilidad de stop tienen limitaciones. No hubo validación Zigbee de NWK/APS/ZCL, joining o seguridad. [EXP-01][EXP-02]

**Ausente:** MAC completo, CSMA-CA gestionado como servicio, address filtering, auto-ACK, asociación, retries, polling, PAN/network state y seguridad MAC/Zigbee. Por tanto, Level 0 es un servicio raw útil, no un nodo IEEE MAC ni Zigbee.

### 6.2 Nivel 1 — identificación y decodificación pasiva

| Capa | Posible sin cambiar firmware | Contexto necesario | Estado defendible |
|---|---|---|---|
| IEEE 802.15.4 MAC | Sí | versión FCF, modos de dirección, PAN compression, FCS presente | **FACTIBLE-CONDICIONAL** en Python; no implementado. |
| Zigbee NWK | Sí para cabecera visible | versión/protocolo, campos opcionales, source route | **PLAUSIBLE-NO-VERIFICADO**. |
| Zigbee APS | Sí si el payload relevante está visible/descifrado | frame type, delivery mode, extended header, fragmentación | **PLAUSIBLE-NO-VERIFICADO**. |
| ZDO/ZDP | Sí en plaintext y con schema del cluster | cluster ID y payload delimitado | **PLAUSIBLE-NO-VERIFICADO**. |
| ZCL | Sí en plaintext y con cluster/profile context | frame control, manufacturer, direction, tipos/atributos | **PLAUSIBLE-NO-VERIFICADO**. |
| Cabecera de seguridad | Sí, si está visible | capa que protege, security control, counter, source, key sequence | **PLAUSIBLE-NO-VERIFICADO**. |
| Payload cifrado | Sólo conservar/etiquetar | ninguno para conservación | Bytes ya preservados; no hay descifrado. |
| Descifrado autenticado | No por mera captura; posible en host con autorización | clave correcta, IEEE source/nonce, counter, AAD, nivel/MIC, key sequence | **PLAUSIBLE-NO-VERIFICADO** hasta verificar MIC con vectores y capturas. |

La ruta completa puede vivir en Python. Colocarla en CC1352P7 no mejora la disponibilidad de bytes y obligaría a diseñar un resultado estructurado sobre COBS, gastar flash/RAM y duplicar evolución de schemas.

Dentro de ZCL también hay grados: la cabecera puede separarse con pocos bytes; los comandos foundation contienen listas de registros que deben validarse uno por uno; el significado de un comando cluster-specific requiere cluster, dirección y, a veces, código de fabricante; y el valor de un atributo requiere su tipo y límites de longitud. Un decoder robusto debe conservar como raw todo campo desconocido en vez de adivinarlo. Del mismo modo, un cluster ID ZDO permite elegir un schema de descriptor/comando, pero una ASDU aislada sin cluster ID no basta para interpretarla.

### 6.3 Nivel 2 — reuso selectivo de Z-Stack

| Función TI | Valor real | Riesgos/condiciones | Recomendación |
|---|---|---|---|
| `zclParseHdr()` (`zcl.c:2412-2446`) | Orden y bits de cabecera ZCL | No recibe longitud; incrementa punteros bajo supuestos del caller | **Adaptar/endurecer**, no copiar sin bounds. |
| `zclBuildHdr()` (`zcl.c:2459-2490`) | Serialización compacta de cabecera | `static`, sin capacidad de buffer | Usar como referencia o extraer con writer acotado. |
| `zclGetDataTypeLength()` (`zcl.c:3195-3279`) | Tabla de tamaños fijos | Retorno de 8 bits y tipos variables/especiales | Reuso algorítmico razonable con tests de tipos. |
| `zclGetAttrDataLength()` (`zcl.c:3296-3322`) | Longitud de strings/atributos | Lee prefijos sin longitud disponible; riesgo de truncación/overflow | Reescribir con `buffer_len`, límites y error estructurado. |
| `zclSerializeData()` (`zcl.c:2961-3117`) | Endianness y representación de valores ZCL | No conoce capacidad; tipos complejos y punteros Z-Stack | Adaptar a writer acotado o implementar desde definiciones CSA. |
| `ZDO_Parse*` (`zd_object.c:2914-3764`) | Orden de campos y descriptores | `zdoIncomingMsg_t`, `asdu` sin longitud suficiente, alloc/ownership OSAL, tablas | Principalmente **referencia**; sólo parsers seleccionados tras reemplazo sustancial. |

El archivo `zcl.c` está bajo BSD-3-Clause, mientras otros archivos NWK/APS usan licencia TI limitada a dispositivos TI. La licencia debe revisarse archivo por archivo; el manifest no autoriza inferir una sola licencia para toda la extracción. Esta propuesta no ofrece dictamen legal. [SDK-01][SDK-12]

No son reutilizables de forma independiente: `zcl_ProcessMessageMSG()`, `zcl_SendCommandEx()`, handlers de clusters con estado, `zd_object.c` completo, AF, `stub_aps.c`, NWK/APS ordinarios, ZMAC/TIMAC, Z-Stack API, BDB, commissioning, seguridad activa y NV. **BLOQUEADO** por runtime, globals, librerías, configuración y propiedad RF. [SDK-02][SDK-04][SDK-06][SDK-07]

| Modalidad de reuso | Ventaja | Coste/riesgo | Decisión |
|---|---|---|---|
| Compilar fuentes TI completas sin cambios | Máxima semejanza nominal | Translation units arrastran AF/config/OSAL/globals; no hay bounds modernos ni build demostrado | **Rechazada** |
| Extraer/adaptar helpers pequeños | Conserva reglas de bytes inspeccionadas | Divergencia futura, licencia y tests propios | **Aceptable selectivamente** |
| Implementación independiente desde CSA | Contrato host natural, bounds y errores diseñables | Hay que demostrar fidelidad y mantener revisión | **Preferida para decoder host general** |
| Usar TI sólo como referencia/vectores | Menor acoplamiento y mantenimiento | No reutiliza código directamente | **Preferida para ZDO y estructuras muy acopladas** |

### 6.4 Nivel 3 — construcción y transmisión limitada

La capacidad debe describirse en cuatro escalones:

1. **MPDU arbitrario:** ya existe, con máximo 125 bytes y FCS generado por RF Core.
2. **Frame IEEE sintáctico:** factible en host si se construyen FCF, secuencia, addressing, PAN compression y payload dentro de límites.
3. **Bytes NWK/APS/ZDO/ZCL sintácticos:** factible como constructor offline/raw para casos definidos; requiere conocer todos los campos y longitudes.
4. **Frame Zigbee asegurado o comportamiento de red:** requiere clave/contexto/counters/nonce/MIC y, para operar como nodo, servicios MAC, ACK, retries, timing, addressing y state machines.

Un constructor puede producir un vector correcto y la radio puede transmitirlo, pero eso no demuestra que el emisor esté asociado, que el counter sea aceptable, que una respuesta sea atendida a tiempo o que el peer lo acepte. La inyección raw es útil para pruebas autorizadas, replay controlado y corpus, no es un nodo Zigbee.

### 6.5 Nivel 4 — pasivo avanzado

| Capacidad | Unidad mínima | Estado/contexto | Firmware nuevo |
|---|---|---|---|
| Mapeo short↔IEEE | múltiples anuncios/respuestas/tráfico | cache con procedencia y expiración | No |
| Reensamblado APS | secuencia de fragmentos | key por source/destination/APS counter, límites/timeouts | No |
| Correlación | request/response | transaction sequence, endpoints, cluster, ventana | No |
| Topología | múltiples frames | neighbor/LQI/route/announcements observados; nunca confundir observación con verdad global | No |
| Interpretación de atributos | ZCL + schema | revisión ZCL, cluster, fabricante y tipo | No |
| Seguridad visible | una trama | parse de aux header, ciphertext y MIC | No |
| Descifrado autorizado | trama + contexto externo/histórico | clave, nonce, AAD, counter, MIC; éxito sólo si MIC verifica | No, puede ser host |
| Comparación independiente | misma captura | decoder propio vs Wireshark/Scapy fijado | No |
| PCAP/PCAPNG | frames + timestamps/metadata | DLT correcto; PCAPNG preferible para metadatos y resolución | No |

El ejemplo actual escribe PCAP classic con DLT 195, adecuado para IEEE 802.15.4 con FCS, pero pierde metadatos FeralRF y usa limitaciones de timestamp documentadas. El flujo KillerBee no está validado contra una versión instalable fijada. [HOST-01][AUD-03]

### 6.6 Nivel 5 — Zigbee operacional

Network discovery, association, join/rejoin, ACK MAC oportuno, polling de end device, NWK routing/addressing, APS delivery/retries/acks, ZDO, endpoints ZCL, BDB commissioning, keys, counters persistentes y aplicación requieren un stack completo o una reimplementación equivalente. En este proyecto la opción técnicamente razonable es Z-Stack completo, no reconstruirlo sobre TX raw.

El rol no es un detalle de la API. Un ZED no forma la red ni enruta; un ZR añade routing y gestión de hijos; un ZC añade formación y normalmente responsabilidades de Trust Center, claves y más estado persistente. El SDK genera/liga configuraciones por rol (`nwk_zed`, `nwk_zr` y `nwk_all` para ZNP). Que ZNP enlace `nwk_all` amplía el potencial del backend, pero no demuestra que una configuración CatSniffer concreta ya pueda asumir cualquiera de esos roles: el rol, commissioning, políticas de seguridad y límites NV deben definirse y validarse. [SDK-04][DOC-TI-04]

#### A. Target ZED dedicado

- **Obtiene:** join/rejoin, sleepy/non-sleepy end device según configuración, ZDO/BDB/ZCL application endpoint.
- **Ventaja:** menor alcance de rol y app acotada; buena opción para una demostración mínima de participación real.
- **Desventaja para convergencia:** la lógica de aplicación reside principalmente en el MCU y la interfaz host debe diseñarse; no ofrece por sí sola el control general AF/ZDO que busca una API Python amplia.
- **Clasificación:** **FACTIBLE-CONDICIONAL**, no verificado en CatSniffer ni en este checkout.

#### B. Target ZNP dedicado

- **Obtiene:** stack operacional con host como application processor mediante MT_AF/MT_ZDO/MT_UTIL/MT_APP_CNF; permite que Python sea el punto de convergencia.
- **Ventaja:** mejor correspondencia con una API multi-backend y con evolución de herramientas de host.
- **Desventaja:** NPI/MT, UART, async callbacks, NV, reset y configuración son un protocolo distinto; TI advierte además que ZNP es una base y una aplicación Zigbee 3.0 completa requiere capas de host adicionales.
- **Clasificación:** **FACTIBLE-CONDICIONAL y recomendada para la rama operacional**, sujeta a target/board/toolchain y validación.

#### C. Imagen única con propiedad exclusiva Z-Stack

- Z-Stack sustituiría el runtime radio principal; funciones FeralRF raw podrían existir sólo si se reconstruyen como servicios dentro de esa aplicación y sin interferir con el stack.
- No preserva automáticamente BLE raw, proprietary Sub-1 GHz, crypto commands, COBS ni semántica de `Radio`.
- **Clasificación:** **PLAUSIBLE-NO-VERIFICADO**, valor menor que una persona separada y riesgo de acoplamiento alto.

#### D. DMM

- El SDK contiene ejemplos ZC/ZR/ZED + BLE y exige `DMMPolicy`/`DMMSch`, registro de dos clientes, tablas de prioridad, estado de aplicación e interceptación/scheduling RF.
- El target actual no usa DMM, usa `RF_postCmd()`/`RF_runCmd()` directamente y conserva handles con reglas locales. Tener un RF Driver de dos clientes no equivale a integración DMM.
- TI caracteriza sólo subconjuntos de combinaciones; no hay ejemplo ni evidencia para “Z-Stack + runtime raw universal FeralRF”.
- **Clasificación:** **BLOQUEADO como propuesta actual; rama de investigación futura**. [SDK-13][DOC-TI-07]

#### E. Conmutación in-process FeralRF↔Z-Stack

- Requeriría detener tareas, comandos, timers y callbacks; cerrar/transferir RF; reconciliar OSAL/TI-RTOS, heap, crypto, NV, ISR, configuración y state machines; y definir qué significa pausar una red sin perder timings/counters.
- No existe API oficial demostrada que “desmonte” Z-Stack y devuelva el RF a FeralRF, ni camino inverso validado.
- **Clasificación:** **NO-SOPORTADO/BLOQUEADO**.

## 7. Convergencia práctica de la API de host

### 7.1 Arquitectura recomendada

```text
                         API Python convergente
                 (capacidades, errores, eventos, modelos)
                                  |
                +-----------------+-----------------+
                |                                   |
        backend FeralRF raw                 backend Z-Stack operacional
        COBS/CRC, 921600                     NPI/MT, parámetros propios
                |                                   |
      persona firmware FeralRF              persona firmware ZNP
      RX/TX IEEE + otras PHY                TIMAC/NWK/APS/ZDO/BDB
                |                                   |
          CC1352P7/RF Core                    CC1352P7/RF Core
                |
       decoder pasivo en host
       MAC/NWK/APS/ZDO/ZCL/security
```

Las dos personas pueden ejecutarse en momentos distintos sobre el mismo CatSniffer o simultáneamente en dispositivos distintos. La figura no afirma hot switching.

### 7.2 Candidatos conceptuales de API

Los siguientes son **CANDIDATOS CONCEPTUALES — APIs NO EXISTENTES**:

- descriptor de backend/persona y versión;
- descubrimiento de capacidades (`raw_capture`, `raw_tx`, `passive_decode`, `network_join`, `zdo`, `zcl_endpoint`, `coordinator`, etc.);
- selección explícita de transporte/backend;
- resultado raw que preserve bytes, FCS, canal, timestamp y calidad;
- resultado decodificado que preserve offsets/raw bytes, warnings y contexto usado;
- operación de red con identificador, aceptación, finalización y eventos asíncronos separados;
- errores tipados por dominio y backend, sin convertir un fallo MT en un ACK COBS genérico;
- consulta de persona requerida y procedimiento externo de activación/reflash.

No se proponen nombres públicos definitivos ni firmas. El contrato debe diseñarse después de fijar corpus, versiones Zigbee y requisitos de rol.

### 7.3 Matriz de compatibilidad de interfaz

| Capacidad | API FeralRF existente | Tratamiento recomendado | Backend operacional |
|---|---|---|---|
| Captura IEEE raw | Reusar `Radio`/`Packet` | Documentar FCS/PHR, pérdidas, canal y timestamp | No exigir ZNP |
| TX raw | Reusar con etiqueta explícita raw | Mantener separado de “send Zigbee” | No mapear automáticamente a AF send |
| Decode pasivo | No existe | Wrapper/servicio Python sobre `Packet`; conservar raw | Independiente del firmware |
| Resultado estructurado | No existe | Nuevo modelo host versionado | Puede consumir capturas o indications ZNP, pero con procedencia distinta |
| Constructor protocol-aware | No existe; `TX_FRAME` no lo es | Servicio host opcional; salida bytes + precondiciones | No sustituye AF/ZDO/ZCL operativo |
| Join/commissioning | No existe | API separada con capability gate | ZNP/ZED solamente |
| ZDO/ZCL operativo | No existe | API de alto nivel sobre MT/AF/ZDO | ZNP; o app específica ZED |
| Descubrimiento de capabilities | No existe | Obligatorio para no ocultar personas | Ambos backends |
| Errores/completions | ACK/ERROR + async RX error | Normalizar envelope, preservar semántica backend | SRSP/AREQ/callbacks MT no se traducen 1:1 |
| Selección de persona | No existe | Explícita; no fingir runtime combinado | FeralRF vs ZNP/ZED |

### 7.4 Ventajas y costes de una abstracción unificada

**Ventajas:** misma ergonomía Python; modelos de captura compartidos; capability negotiation; validación cruzada; aplicaciones que eligen el mínimo backend; aislamiento del transporte.

**Costes:** riesgo de “mínimo común denominador”; diferencias de temporalidad y errores; necesidad de versionar schemas; pérdida de detalles MT si se fuerza una fachada demasiado estrecha; confusión si “send” oculta raw vs APS delivery.

La abstracción debe ser un façade con subinterfaces especializadas y datos de procedencia, no una clase que afirme que todos los backends implementan todo.

## 8. Matriz de decisión arquitectónica

Escala de complejidad relativa: baja/media/alta/muy alta. “Preserva FeralRF” significa preservación al usar la misma imagen, no disponibilidad simultánea.

| Alternativa | Funcionalidad realista | Reuso | Cambios requeridos | Toolchain/memoria/RF | HW, NV, seguridad | Validación previa | Preserva FeralRF | Incertidumbre / complejidad | Clasificación |
|---|---|---|---|---|---|---|---|---|---|
| Host pasivo sobre FeralRF | L0–L4 pasivo; no join | Alto: `Packet`, RF raw, PCAP | Python y fixtures; firmware no necesario para CRC-good | Python; RF actual | Sin NV de red; keys sólo autorizadas en host | golden captures, malformed, decoder independiente | Sí | Media / media | **Alta factibilidad** |
| Reuso ZCL/ZDO selectivo | Codecs/definiciones, no stack | Bajo-medio y localizado | Adaptación bounds/ownership/licencia | Host preferible; C opcional | Sin NV/seguridad activa | vectores, fuzz, round-trip | Sí si host | Media / media | **Factible condicionado** |
| ZED dedicado | Nodo final real, endpoints limitados | Z-Stack completo como producto TI | Nuevo target, app, SysConfig, board, host opcional | TI ARM Clang/IAR según libs; mapa real | NVOCMP, keys, counters, antena | boot, join, rejoin, peer, persistence | Sí, aislado en otra persona | Alta / alta | **Factible condicionado** |
| ZNP dedicado | Operación Zigbee amplia controlada por host | Z-Stack + NPI/MT | Nuevo target y adaptador Python; RP2040/UART | toolchain/librerías de rol; stack/NPI RAM/flash | NV, keys, counters, reset, board | SYS ping/version, join/form, AF/ZDO, restart, peer | Sí, aislado en otra persona | Alta / alta | **Preferido para rama operacional** |
| Imagen única, Z-Stack exclusivo | Z-Stack + servicios raw reconstruidos | Parcial; alto acoplamiento | Sustituir arquitectura FeralRF o portarla dentro de app Z-Stack | mapa conjunto; Z-Stack dueño RF | Todo lo anterior | equivalencia FeralRF + Zigbee | Parcial; riesgo de pérdida | Muy alta / muy alta | **Diferir** |
| DMM Z-Stack + FeralRF | Simultaneidad arbitrada, si fuese posible | Ejemplos DMM sólo como referencia | Integración de scheduler/policies/hooks de ambos clientes | dos clientes, scheduling, timing, gran presión de recursos | board y estados de ambos runtimes | build/map, RF interception, stress, timing y coexistencia | Potencialmente parcial | Muy alta / muy alta | **BLOQUEADO/experimental** |
| Switching in-process | Alternancia sin reflash | Escaso | lifecycle completo y transferencia de recursos | no hay teardown probado | estado NV/counters y red sensible a pausa | pruebas exhaustivas de quiescence/recovery | Incierto | Extrema / extrema | **Rechazar hoy** |
| Dos dispositivos físicos | FeralRF y ZNP simultáneos y aislados | Máximo de cada imagen | API host y coordinación | memoria independiente; RF independiente | board por dispositivo | pruebas cruzadas y clocks | Sí, totalmente | Baja-media / media | **Alternativa de menor riesgo para investigación** |

La siguiente tabla desagrega los criterios que la matriz compacta agrupa:

| Alternativa | Componentes y modificaciones firmware | Modificaciones Python | Toolchain, memoria y runtime | RF, hardware y antena | Seguridad/NV | Licencia y mantenimiento | Prerrequisitos de validación |
|---|---|---|---|---|---|---|---|
| Host pasivo | Ninguna para CRC-good; opcional futura para bad-FCS/timestamp | Decoder, modelos, schemas, PCAPNG, fixtures | Python; sin presión MCU adicional | RF/board actual | Keys/contexto sólo si se autoriza decryption | Mantener revisiones y dependencias opcionales | corpus, dissector fijado, bounds, MIC vectors |
| Reuso selectivo | Ninguna si se ubica en host; codec C acotado si se justifica MCU | Bindings o port independiente | Sin librerías de rol; evitar translation units completas | Sin cambio RF | No provee seguridad activa ni NV | Revisión archivo por archivo y seguimiento de upstream | unit/round-trip/fuzz/licencia |
| ZED separado | Target/app/endpoints/SysConfig/board nuevos | Sólo control/telemetría que se decida | Toolchain TI soportado, librería `nwk_zed`, TI-RTOS/OSAL, mapa | Z-Stack dueño; portar DIO28–30/PA | NVOCMP, keys y counters del nodo | Mantener app + SDK fijado | init, join/rejoin, ZDO/ZCL, reset/persistencia, peer |
| ZNP separado | Target ZNP, NPI/MT, SysConfig/board | Backend MT completo, callbacks y façade | Toolchain TI, `nwk_all`, tasks/heap NPI/stack, mapa | ZNP dueño; validar UART/RP2040 y antena | NV/keys/counters + políticas host | Versionar MT y firmware; límites de licencia TI | SYS ping/version, commission, AF/ZDO, roles, restart, peer |
| Imagen única exclusiva | Portar servicios FeralRF dentro de app Z-Stack o renunciar a ellos | Backend híbrido | Mapa conjunto, un RTOS/config, alto riesgo de colisión | Único dueño Z-Stack; raw sólo como servicio diseñado | Estado Z-Stack completo | Máximo acoplamiento a internals | todo ZNP/ZED + regresión FeralRF operación por operación |
| DMM | Convertir FeralRF en cliente DMM; policies, scheduler, hooks, estados | Exponer concurrencia y preemption | Dos stacks/clientes, peor caso de RAM/flash/latencia | RF arbitrado; compatibilidad de patches/front-end | Estado de ambos runtimes y power | Alto coste por SDK/policy changes | build/map, scheduling, ACK/poll latency, stress/coexistence |
| Switching in-process | Teardown/startup completo de ambos runtimes | API de transición y recuperación | Dos runtimes no simultáneos pero residentes; lifecycle no soportado | Transferencia RF y antena determinista | No romper counters, parent/child ni NV | Arquitectura ad hoc difícil de mantener | quiescence, repeated switches, power loss, network recovery |

### 8.1 Toolchain y binarios

SysConfig selecciona `maclib_nosecure_<device>_2_4g[_tirtos7].a` y `libZStack_nwk_<role>.a` por toolchain/ISA. En el submódulo inspeccionado se encuentran variantes TI ARM Clang/IAR y no se demostró una variante GCC compatible con el target FeralRF. No debe asumirse ABI-link compatibility. Un target ZNP/ZED debe partir de un proyecto/toolchain soportado por TI, no añadir archivos al CMake GNU actual por ensayo. [SDK-04]

### 8.2 Mantenimiento y licencias

- El decoder host independiente reduce acoplamiento a internals TI, pero debe rastrear revisiones de Zigbee/ZCL y manufacturer clusters.
- Copiar helpers TI aumenta obligación de rastrear parches y licencias por archivo.
- ZNP mantiene una frontera oficial MT, pero el host debe fijar SDK/firmware y command set.
- Integraciones GPL en proceso pueden afectar distribución; un flujo externo Wireshark/PCAP reduce acoplamiento, sin resolver automáticamente todos los términos. Se requiere revisión legal separada.

## 9. Prioridad técnica propuesta

No es un calendario. Son capacidades ordenadas por evidencia, valor y reducción de riesgo.

| Prioridad | Capacidad | Justificación | Dependencias | Rama |
|---:|---|---|---|---|
| 1 | Contrato IEEE raw verificable | Ya implementado y con evidencia OTA limitada; sustenta todo lo pasivo y evita ambigüedad FCS/PHR | Ninguna nueva; requiere especificación y corpus | Común |
| 2 | Decodificación pasiva MAC→NWK→APS→ZDO/ZCL | Máximo valor sin tocar firmware; verificable contra capturas y dissector independiente | P1, revisión Zigbee objetivo | Acumulativa |
| 3 | Exportación PCAPNG + comparación independiente | Produce evidencia reproducible y desacopla la validación del decoder propio | P1; deseable junto con P2 | Acumulativa |
| 4 | Estado pasivo y seguridad visible; decryption autorizado opcional | Amplía análisis sin pretender participación; requiere contexto explícito | P2, corpus, vectores CCM* | Acumulativa |
| 5 | Codecs/constructores acotados con reuso selectivo | Útiles para round-trip, fixtures y TX raw controlado; no bloquean decoder básico | P2; revisión licencia/bounds | Acumulativa |
| 6 | Façade multi-backend y capability discovery | Evita que futuras personas rompan semántica; se diseña con dos contratos reales | P1 y especificación mínima ZNP | Común |
| 7 | Persona ZNP dedicada | Mejor ajuste para API Python operacional y extensibilidad | target TI soportado, board, NPI/MT, NV, seguridad, peer | Alternativa operacional preferida |
| 7b | Persona ZED dedicada | Mejor si la meta es un endpoint autónomo mínimo y no control general | target TI soportado, app/endpoints, board, peer | Alternativa a ZNP, no prerequisito |
| 8 | Imagen única con Z-Stack exclusivo | Sólo si selección de persona es inaceptable y se justifica pérdida/acoplamiento | ZNP/ZED demostrados, mapa de recursos, requisitos raw | Condicional |
| 9 | DMM | Sólo tras demostrar stacks soportados y diseñar FeralRF como cliente DMM | todo lo anterior + scheduling/policies/performance | Investigación independiente |

Relaciones:

```text
P1 contrato raw ──> P2 decoder ──> P4 estado/seguridad
       |                |
       └──> P3 PCAPNG <-┘
                        └──> P5 codecs/constructores

P1 + contrato ZNP ──> P6 façade ──> P7 ZNP
                                   └─X─ P7b ZED (alternativa según objetivo)

P7 demostrado + necesidad explícita ──> P8 o P9 (ramas alternativas, no secuencia obligatoria)
```

## 10. Arquitectura recomendada y razones

### 10.1 Recomendación

Adoptar una **arquitectura de backends/personas separadas con convergencia en Python**:

- FeralRF sigue siendo la persona raw multiprotocolo y dueña exclusiva del RF durante esa sesión.
- El decoder pasivo opera sobre el contrato `Packet` sin cambiar firmware.
- Los pequeños algoritmos ZCL/ZDO se usan sólo cuando ganan precisión frente a una implementación independiente, después de hardening y revisión de licencia.
- ZNP es la persona operacional preferida si el supervisor aprueba ampliar el alcance a participación Zigbee real controlada desde Python.
- ZED es una alternativa si la pregunta académica se limita a demostrar un endpoint funcional y se acepta lógica de aplicación en MCU.
- La API de host descubre capabilities y expone la persona; nunca reporta “Zigbee operativo” por disponer sólo de RX/TX raw.

### 10.2 Por qué es preferible

1. reutiliza la única capacidad RF ya demostrada sin alterar su runtime;
2. permite validación independiente con corpus, PCAP y dissectors;
3. usa Z-Stack donde su valor es irreemplazable: MAC/state machines/security/NV;
4. evita enlazar librerías de rol TI con un target/toolchain no demostrados;
5. conserva la posibilidad de usar todas las PHY FeralRF, aunque no simultáneamente con ZNP en el mismo MCU;
6. reduce el riesgo de presentar inyección raw como interoperabilidad;
7. deja DMM como investigación con criterios verificables, no como supuesto.

## 11. Funcionalidad preservada, perdida o aislada

| Al usar persona | Preservada | Aislada/no disponible durante esa sesión | Observación |
|---|---|---|---|
| FeralRF | BLE raw actual, IEEE raw, proprietary Sub-1 GHz, TX tests, crypto host commands, COBS API | join/ZDO/BDB/ZCL operativo | Decoder pasivo sigue disponible en host. |
| ZNP | Zigbee MAC/NWK/APS/ZDO/BDB/security/NV y MT | APIs raw FeralRF, otras PHY, semántica COBS, helpers específicos | ZNP es dueño del RF; raw capture depende sólo de lo que ZNP/MT exponga, no de `Packet`. |
| ZED | Stack y endpoint/app seleccionados | API host general, FeralRF raw multiprotocolo | Puede añadirse telemetría host, pero no se presupone. |
| DMM hipotético | Subconjunto planificado de ambos | Operaciones que pierdan arbitraje/timing | La preservación debe medirse por operación, no declararse globalmente. |

## 12. Condiciones antes de justificar implementación

### 12.1 Para pasivo host

- fijar revisiones IEEE/Zigbee/ZCL y features en alcance;
- definir un corpus legal y reproducible: plaintext, security headers, cifrado con claves autorizadas, fragmentos, source routing, ZDO/ZCL, manufacturer-specific, truncados/malformados;
- verificar FCS-present y límites con captures independientes;
- comparar al menos con un dissector versionado;
- comprobar bounds, errores y round-trip; ningún crash ante input arbitrario;
- para decryption: known-answer CCM*, MIC negativas y rechazo obligatorio si no autentica.

### 12.2 Para ZNP/ZED

- obtener/generar el proyecto de ejemplo correcto para CC1352P7 y rol, ausente del checkout actual;
- demostrar toolchain y enlace soportados, con mapa de flash/RAM/heap/stacks/NV;
- portar la configuración exacta del board CatSniffer, antena y PA;
- resolver la ruta UART/RP2040: baud, framing NPI, reset/boot, flow control y coexistencia con Cat-Shell;
- definir partición NV, persistencia de counters y recuperación tras reset/power loss;
- demostrar init y, con peer independiente, discovery, join/rejoin, APS/ZDO/ZCL y persistencia;
- establecer versión del MT API y modelo de seguridad/autorización del host.

### 12.3 Para imagen única o DMM

Antes de considerarlas debe demostrarse, no suponerse:

- composición exacta de librerías por rol y ABI/toolchain;
- mapa de memoria con margen, peor caso de heap y stacks;
- único modelo de TI-RTOS/OSAL/config generado;
- propiedad RF y lifecycle de todos los handles;
- en DMM, que **cada** operación RF pase por `DMMSch` y que existan policy/state tables para el rol FeralRF;
- límites de latencia para ACK/poll/join frente a captura raw larga;
- compatibilidad de RF patches y setup commands;
- configuración de antena/PA para cada modo;
- estrategia NV/crypto/interrupts/power;
- recuperación determinista y ausencia de corrupción de red/counters;
- regresión completa de las funciones FeralRF que se pretenda preservar.

El mero máximo de dos clientes del RF Driver no satisface estos puntos. [SDK-13]

## 13. Preguntas requeridas — respuestas consolidadas

1. **¿Qué hace ya FeralRF útil para Zigbee?** Recibe frames IEEE CRC-válidos en un canal, entrega el MPDU con FCS y metadatos, y transmite MPDU raw sin FCS; hay evidencia OTA entre dos CatSniffer, no Zigbee.
2. **¿Qué se añade sin cambiar firmware?** Parsing pasivo MAC/NWK/APS/ZDO/ZCL visible, security-header analysis, estado pasivo, exportación/ingesta PCAP y decryption autorizado condicionado.
3. **¿Qué algoritmos/definiciones Z-Stack valen la pena?** Cabecera/tipos/serialización ZCL pequeños y field ordering/definitions ZDO; siempre con bounds, ownership, tests y licencia.
4. **¿Qué no se integra independientemente?** TIMAC/ZMAC, NWK, APS, AF, ZDO/ZCL operativos, BDB, seguridad, NV y Z-Stack API; requieren runtime/config/librerías/estado.
5. **¿Hasta dónde llega raw sin stack?** Captura, análisis, construcción e inyección sintáctica. No asociación, ACK oportuno, routing, commissioning, counters persistentes ni participación.
6. **¿Qué exige pasar a participación real?** Z-Stack completo o equivalente, MAC operativo, estados NWK/APS/ZDO/BDB, seguridad/NV, toolchain y board, y validación con peer.
7. **¿ZED o ZNP?** ZNP satisface mejor convergencia de API host; ZED satisface mejor una demostración autónoma mínima. La elección depende de si Python debe ser application processor.
8. **¿Qué se pierde si Z-Stack es dueño RF?** Durante esa persona se aíslan las PHY y APIs FeralRF, salvo que se reimplementen deliberadamente; se preservan al volver a la persona FeralRF o usar otro dispositivo.
9. **¿Qué demostrar antes de imagen única/DMM?** Toolchain/binarios, mapa, OSAL/tasks, todos los RF hooks/scheduling, patches, antena, NV/security, timing y regresión; véase §12.3.
10. **¿Prioridad?** Contrato raw → decoder/PCAP → estado/security/codecs → façade → ZNP o ZED → sólo después imagen única/DMM.
11. **¿Qué se decide ahora?** Separar capacidad raw, análisis pasivo y operación; host como punto de convergencia; no equiparar TX raw con nodo. Quedan condicionales rol operacional, revisiones, corpus, ZNP/ZED y forma de despliegue.
12. **¿Máximo alcance defendible?** Backend FeralRF raw + decoder pasivo host + reuso codec acotado + persona ZNP separada como objetivo condicionado. No se sostiene una imagen combinada ni DMM hoy.

## 14. Restricciones y preguntas abiertas

- No se definió qué revisión Zigbee/ZCL ni manufacturer clusters son obligatorios.
- Falta corpus representativo y autorizado.
- El edge RF exacto del timestamp y el manejo de rollover no están demostrados.
- FeralRF descarta bad-FCS y no ofrece captura multicanal/simultánea.
- Las implementaciones de `NLDE_ParseMsg()`, `APSDE_ParseMsg()` y TIMAC no son auditables en fuente visible.
- No se demostró enlace de archivos TI ARM Clang/IAR con el target GNU; no debe intentarse como premisa.
- Faltan proyectos `examples/rtos/<board>/zstack` en este submódulo checkout, aunque la documentación oficial los referencia.
- Falta mapa de memoria de cualquier persona ZNP/ZED CatSniffer.
- El RP2040 bridge no está en este repositorio; su transparencia para NPI/MT y reset debe verificarse.
- KillerBee y Wireshark no están fijados/validados end-to-end en FeralRF; Scapy/zigpy/WHAD tampoco son compatibilidad demostrada.
- No hay conclusión legal sobre redistribución de código TI o combinación con dependencias GPL.

## 15. Recomendación técnica final para revisión del supervisor

Se recomienda aprobar como alcance de convergencia el siguiente enunciado:

> **Converger FeralRF y TI Z-Stack en una API Python con backends explícitos: FeralRF proporciona captura/transmisión IEEE 802.15.4 raw y alimenta análisis Zigbee pasivo; Z-Stack proporciona operación de red únicamente mediante una persona de firmware separada, preferentemente ZNP cuando Python deba controlar la aplicación. El reuso de código Z-Stack se limita a codecs/definiciones acotados y endurecidos.**

Se recomienda no aprobar todavía una imagen única, hot switching o DMM como solución. Esas alternativas no maximizan la funcionalidad por unidad de evidencia: añaden riesgo de toolchain, memoria, RF ownership, timing, NV y mantenimiento antes de que se haya demostrado siquiera un target ZNP/ZED en el hardware CatSniffer.

La decisión inmediata que requiere aprobación del supervisor es doble:

1. confirmar que la convergencia se evaluará en la API de host y mediante personas separadas, sin exigir runtimes fusionados;
2. decidir si la rama operacional futura prioriza **ZNP controlado por Python** o **ZED autónomo mínimo**.

Todo lo demás —revisión de Zigbee, corpus, seguridad, endpoints, rol coordinador/router y posible coexistencia— debe permanecer condicional hasta aportar la evidencia indicada.
