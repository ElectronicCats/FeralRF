# Informe técnico del estado actual de FeralRF

**Documento canónico de estado — corte 2026-10-22**  
**Revisión principal:** `feature/hardware-validation` @ `fbff1b7f5077964418fb63a3fdf346a27c35a003`

## 1. Síntesis técnica

FeralRF es actualmente una herramienta multipropósito de control RF para el CC1352P7 de CatSniffer. Combina una biblioteca Python, un protocolo binario COBS/CRC transportado por Cat-Bridge, firmware TI-RTOS en el CC1352P7 y el firmware RP2040 stock que actúa como puente y controlador de reset/banda. Su fortaleza demostrada es el control PHY y el intercambio de paquetes raw: existen recepciones IEEE 802.15.4, TX RAW/FRAME entre dos CatSniffer y entregas propietarias FSK/GFSK en varias configuraciones 868–915/902.2 MHz.

El proyecto no es hoy una colección de pilas completas. IEEE se implementa como PHY y transporte de bytes MAC; BLE cubre PHY/advertising limitado; los nombres Wireless M-Bus, Wi-SUN, Sidewalk y MIOTY corresponden a parámetros nominales o helpers incompletos, no a interoperabilidad protocolaria. OOK dispone de una ruta de implementación y un reporte histórico declara 10/10 raw a 868 MHz; la repetición reciente RF-05 no se ejecutó. MSK, 4-FSK y 4-GFSK están nombrados en presets con valores que el comando TI activo reserva.

La madurez puede calificarse como **prototipo de investigación funcional para PHY/raw, con deuda relevante de contrato, lifecycle, observabilidad y reproducibilidad**. La base sirve para continuar desarrollo, pero no justifica promesas de soporte protocolario, exactitud de metadatos o configuración RF efectiva sin las correcciones y mediciones indicadas en [FUNCTIONAL_GAPS.md](FUNCTIONAL_GAPS.md).

## 2. Alcance, inventario y revisiones auditadas

### 2.1 Instantánea de fuentes

| Elemento | Estado observado | Papel en esta evaluación |
|---|---|---|
| FeralRF | rama `feature/hardware-validation`; HEAD `fbff1b7f5077964418fb63a3fdf346a27c35a003` | Autoridad primaria de implementación |
| Working tree FeralRF al corte | `python/examples/killerbee_sniff.py` modificado; documentos de planificación, `python/op13_rx_temp.py`, `python/op13_tx_temp.py` y `python/validation_artifacts/` sin seguimiento | Contexto local, no atribuido a HEAD; `experimental_artifacts/` y `validation_runs/` no existen en esta instantánea |
| SDK TI submodule | `5b31d0a4903351e544546e23ef3330eaa4291ceb`, `lpf2-8.30.01.01` | Contrato exacto de comandos/drivers |
| CatSniffer-Firmware | `v3.x` @ `c0cd5a45e019dbd14ed11d039aacb13f300e5731`, tag/describe `v3.1.0.1` | Fuente RP2040 disponible |
| CatSniffer hardware checkout | `master` @ `6da5050fefde59138fc11d9e2ecf7c0de36f3a22`, tag `v3.3` | Contexto actual; el análisis de la placa evaluada conserva el tag histórico v3.1 |
| Hardware histórico evaluado | tag `v3.1` @ `5b99d984933a8b8c36e789853c08d3a2a785bed4`; U8 físico reportado `CC1352P74` | Topología publicada con discrepancia P1/P7 |
| Consolidación histórica recibida | Auditorías, matrices y addendum usados como insumo de revisión | Sus conclusiones y límites necesarios se incorporan en [`EXPERIMENTAL_EVIDENCE_REFERENCE.md`](EXPERIMENTAL_EVIDENCE_REFERENCE.md); esta documentación no requiere otro repositorio |

La versión del paquete Python es `0.3.0` (`python/pyproject.toml`). El CC responde `1.0.0`, capabilities `0x07` y serial constante `FERALRF1` (`ControlTask_getInfoPayload()`), datos insuficientes para identificar el artefacto. Los reportes recientes registran RP2040 `v3.1.0.1`, pero no preservan hash del UF2. Por ello, **HEAD describe la implementación fuente auditada; no identifica automáticamente los binarios de cada campaña histórica**.

### 2.2 Corpus inspeccionado

Se revisaron:

- documentación existente bajo `FeralRF/docs/`, incluidas arquitectura, API, protocolo, matriz histórica, runbook y reportes OTA del 12 y 16 de octubre;
- código `firmware/cc1352/`, `python/feralrf/`, ejemplos y helpers relevantes;
- SDK TI incluido y definiciones `rf_prop_cmd.h`;
- firmware RP2040, overlay y Cat-Shell en `CatSniffer-Firmware/RP2040/catsniffer/`;
- diseño KiCad/PDF y notas de hardware, preservando la incertidumbre de revisión;
- consolidaciones históricas de Fase I y auditorías II-A, II-B y II-C, incorporadas aquí sin crear una dependencia de navegación externa;
- reportes originales y addendum del 21 de octubre, incluidos RF-01…RF-05 y los JSONL referenciados por la consolidación.

Los documentos previos conservan roles diferentes: `VALIDATION_MATRIX.md` es una línea histórica agregada; los reportes OTA preservan campañas; los runbooks son planes; las auditorías históricas son análisis. Esta serie nueva es la vista canónica actual. La [referencia experimental autocontenida](EXPERIMENTAL_EVIDENCE_REFERENCE.md) preserva contabilidad, resultados, procedencia y ausencias sin exigir acceso a otro checkout.

## 3. Jerarquía de evidencia

| Nivel | Uso permitido |
|---|---|
| Fuente actual | Afirmar presencia, ruta ejecutable, contrato o defecto determinista en la revisión auditada |
| Log/reporte primario | Afirmar lo observado en esa ejecución y configuración |
| Resumen consolidado | Orientar y localizar evidencia, manteniendo su procedencia |
| Documentación/README | Describir intención o contrato anunciado; no sustituye el source |
| Verificación estática | Establecer compatibilidad o incoherencia sin afirmar resultado físico |
| Hipótesis | Explicación plausible, marcada como no confirmada |

Un ACK acredita el hito exacto en que lo emite el handler. No acredita finalización TX, recepción OTA, frecuencia, potencia, modulación, antena ni interoperabilidad.

La falta de stdout o JSONL no elimina una observación consignada en un reporte técnico. En esta evaluación, **validez de la observación reportada**, **inspectabilidad de los artefactos**, **reproducibilidad** e **interpretación causal** son dimensiones separadas. Por ejemplo, el baseline de abril–mayo sostiene los conteos que documenta para BLE raw, OOK, crypto y emulación, pero no permite inspeccionar cada paquete ni atribuir frecuencia/modulación físicas. F29, en cambio, permanece como claim de commit/script porque no se recuperó un reporte experimental con contexto equivalente. La taxonomía completa está en [EXPERIMENTAL_EVIDENCE_REFERENCE.md](EXPERIMENTAL_EVIDENCE_REFERENCE.md).

## 4. Arquitectura de hardware y firmware

La ruta principal es:

`Python ↔ Cat-Bridge/USB ↔ RP2040 ↔ UART0 921600 ↔ CC1352P7/TI-RTOS ↔ TI RF Driver/RF core ↔ front-end/U2 ↔ antena`.

El RP2040 transporta bytes, controla reset/boot y las líneas externas de U2; el CC1352P7 interpreta FeralRF y controla el radio. Cat-Shell es un CDC textual independiente. La arquitectura, pines, rutas, compatibilidad por banda y límites físicos se detallan en [HARDWARE_INTEGRATION.md](HARDWARE_INTEGRATION.md).

En el CC1352P7 existen dos tareas principales. `HostIFTask` procesa UART, un comando pendiente y la cola de salida. `DataTask` ejecuta operaciones RF diferidas y entrega eventos. `ControlTask` posee la intención de estado; `RadioIF` administra handles, estructuras TI, colas RX y métricas. Esta división hace que varias órdenes respondan antes de completar el trabajo RF.

## 5. Comunicación con el PC

### 5.1 Protocolo wire

Formato antes de COBS:

```text
CMD_ID:u8 | SEQ:u8 | LEN:u16-le | PAYLOAD:N | CRC16-CCITT:u16-le
```

El frame COBS termina en `0x00`. El límite C declarado es 255 bytes de payload. No existe negociación de versión, ventana, retransmisión general, manifest de build ni operación diferida identificada.

### 5.2 API Python

`Radio.connect()` abre Cat-Bridge; `init()` envía `RADIO_INIT` y consulta `GET_INFO`. La API expone configuración, RX/TX, modos de prueba, crypto, jamming e integraciones. La superficie estable/experimental/pending no sigue aún un criterio uniforme.

La principal brecha de correlación está en `_read_response()`: los eventos RX usan una secuencia propia 0…254, mientras el host preserva durante esperas síncronas sobre todo `seq=0`; otros eventos con secuencia distinta a `_last_seq` pueden descartarse. Las excepciones de parseo también se consumen silenciosamente. Esto es un defecto contractual confirmado en fuente, pero no establece por sí solo la causa completa del timeout `RX_STOP`.

El criterio estático correcto es estricto: todo `RSP_RX_PACKET` válido que compita con una orden síncrona debe entregarse exactamente una vez, independientemente de su `seq`, y la respuesta de la orden debe conservar su correlación. El código actual viola ese criterio. Sin embargo, un timeout de `RX_STOP` también puede involucrar cola de salida, timing de tareas, drenado, bridge o estado del backend; por eso el arreglo del dispatcher y la investigación de stop son trabajos relacionados, pero separables.

## 6. Subsistemas y capacidades implementadas

| Área | Implementación actual | Estado experimental |
|---|---|---|
| Descubrimiento/conexión | PySerial, filtros USB/descripcion y apertura de bridge | Usado repetidamente; asociación Bridge/Shell frágil |
| Init/info/stats | Presente; identidad genérica y métricas parciales | Demostrado por control |
| PHY/canal/frecuencia/potencia | Presente; valores solicitados y configuración TI | Parcial; sin readback ni medición independiente |
| RX | BLE/IEEE/PROP, cola TI + software, metadata por PHY | IEEE y PROP raw demostrados; metadata PROP defectuosa |
| TX RAW/FRAME | Ruta diferida única; FRAME es alias de bytes, no constructor MAC | IEEE y PROP raw demostrados |
| BURST | Scheduler cooperativo por count/intervalo | Actividad, pero 1/5 y 1/40 frente al criterio; completion ausente |
| CONTINUOUS | Scheduler cooperativo hasta stop | Actividad fuerte con intervalo 0; un hit con 250 ms; conteo no establecido |
| TX_STOP | Limpia estado TX/jam y responde ACK | ACK demostrado; cese físico no medido |
| CW/PRBS | Comandos TI de prueba | ACK demostrado; RF física no medida |
| Proprietary FSK/GFSK | Packet mode TI simple con preámbulo/sync/length/CRC | Entrega bilateral en subconjunto 868–915/902.2 |
| OOK | Modo TI y patches `genook`; lifecycle exige power-cycle para salir con seguridad | Reporte histórico 10/10 raw a 868; RF-05 reciente NOT RUN; recuperación actual no demostrada |
| MSK/4-FSK/4-GFSK | Presets copian 4/5/6 al campo `modType` | No soportado por el comando actual; ACK no valida modulación |
| IEEE 802.15.4 | RF core genera PHR/FCS; host aporta PSDU/MAC; sin MAC completo | RX ambiental y OTA raw/frame demostrados |
| BLE | Advertising RX/TX 1M/Coded y construcción especial 2M; parseo LL superficial | Reporte histórico raw: 1M 10/10, 2M 8/10, Coded S8/S2 10/10; sin campaña externa reciente ni pila completa |
| Crypto | TRNG, AES ECB/CTR/CBC/CCM/GCM, SHA-256, ECDH/ECDSA | Reporte histórico 9/9 y script de vectores disponible; no repetido en el build actual; mismatch ECDSA |
| Jamming | Sesión continua BLE/IEEE, duración/cooldown | Control histórico mencionado; efecto físico no validado; requiere autorización/entorno controlado |
| Spectrum | IDs/builders residuales, sin camino firmware utilizable | Ausente |
| KillerBee | Adaptador básico para IEEE | RF-04 bloqueado por dependencia; integración parcial |
| Emulación | Tramas/patrones fijos IEEE, W-MBus y OOK | Firmas raw históricamente reportadas; no state machines ni dispositivos interoperables |

En BURST y CONTINUOUS, el ACK sólo confirma que `ControlTask` aceptó el estado. El scheduler cooperativo se ejecuta después: BURST decrementa `remaining` únicamente si `RadioIF_transmitRaw()` devuelve éxito y, si devuelve fallo, limpia la operación sin evento; CONTINUOUS también puede limpiarse silenciosamente. No existen eventos de inicio, progreso o finalización. Por tanto, un marcador observado por el peer no equivale a un intento TX interno, y el conteo solicitado no puede reconciliarse con emisiones reales usando el contrato actual.

## 7. Framing, modulación y nivel de protocolo

### 7.1 Proprietary

La configuración base produce:

`preamble 4 B | sync 32 b | length | host payload | CRC`.

El RF core agrega ese framing. `sync_word` y parte de `format_conf` son configurables, pero no existe una API tipada completa para preámbulo, CRC, whitening, FEC u orden de bits. Un payload Wireless M-Bus, MIOTY o control OOK enviado por esta ruta no adquiere automáticamente el framing o timing de ese protocolo.

El SDK local define `modType`: `0=FSK`, `1=GFSK`, `2=OOK`; los restantes son reservados (`rf_prop_cmd.h`, L475–478 y L581–584). Por ello, los seis presets nominales `msk_433_50k`, `4fsk_433_50k`, `4gfsk_433_50k`, `msk_868_50k`, `4fsk_868_50k` y `4gfsk_868_50k` no implementan esas modulaciones con la ruta actual.

### 7.2 IEEE 802.15.4 y BLE

IEEE: el host entrega bytes MAC/PSDU y el RF core genera PHR/FCS. No hay auto-ACK, asociación, CSMA-CA de nivel MAC, tablas, seguridad, Zigbee ni Thread. BLE recibe datos publicitarios y el RF core genera framing LL; no hay conexiones, GAP/GATT/SMP ni pila pública completa.

### 7.3 Presets con nombres de protocolos

| Familia | Nivel real actual |
|---|---|
| Wireless M-Bus | Parámetros RF nominales; helper T1 incompleto y usa `msk_868_50k` reservado |
| Wi-SUN | FSK/tasas nominales sobre framing PROP; sin 802.15.4g/FAN, hopping, IPv6 o seguridad |
| Sidewalk | Etiquetas FSK experimentales; el preset 50k usa FSK, no GFSK; sin PHR/hopping/enlace/seguridad |
| MIOTY | FSK 396 baud; sin telegram splitting, coding/interleaving, bursts, hopping, MAC o seguridad |
| Zigbee/Thread | Base IEEE raw utilizable, sin capas superiores |

El inventario fuente contiene **31 presets**. Su existencia no hereda la validación de campañas anteriores.

### 7.4 Caso MIOTY / TS-UNB

`mioty_868_tsunb` solicita `868000000 Hz`, `mod_type=0`, `symbol_rate=396`, `deviation=1`, `rx_bw=0x4A` y `sync_word=0x930B51DE`. La cadena es directa: `presets.py` entrega esos campos, `radio.py` llama `configure_prop()`, `commands.py` serializa `<IBIHBIH`, `command_processor.c` los parsea y responde ACK después de una función `RadioIF_setPropConfig()` sin resultado ni readback.

En `RadioIF_setPropConfig()` se fija `pre_scale=15` y se calcula `rate_word=floor(symbol_rate×1048576×pre_scale/24000000)`. Para 396 resulta `rate_word=259`, equivalente teóricamente a **395.20263671875 baud**. `mod_type=0` selecciona FSK. Con `deviation=1` y `deviationStepSz=0` de la configuración base, la desviación nominal teórica es **250 Hz** según el SDK; no es una medición física. `rx_bw=0x4A` permanece una codificación de registro sin readback efectivo y `0x930B51DE` es el sync genérico de FeralRF, no una prueba de framing MIOTY.

Eso configura un paquete FSK proprietary genérico. No implementa el modelo [ETSI TS 103 357-2](https://www.etsi.org/deliver/etsi_ts/103300_103399/10335702/02.01.01_60/ts_10335702v020101p.pdf): división del telegrama, codificación/interleaving, distribución temporal y frecuencial de sub-ráfagas, recepción/reensamble, procedimientos ni interoperabilidad. No existe una ruta MIOTY/TS-UNB alternativa en el source auditado. El claim histórico `0/10`, además de carecer de evidencia primaria conservada, transmitía un marcador raw y por tanto no valida ni refuta MIOTY.

Una evolución real exige primero un estudio go/no-go: rol y modalidad, factibilidad del RF core/patch, prototipo offline de encoder/decoder, scheduler/backend de sub-ráfagas, reensamble, procedimientos y peer/base station independiente. Integrar un modem/stack externo o retirar el nombre del catálogo son alternativas legítimas.

## 8. Historia de validación

La consolidación histórica identifica 193 unidades de ejecución o intento: 125 con registro o reporte detallado inspeccionado y 68 sustentadas por reporte agregado, resumen o claim sin primarios completos. El segundo grupo no representa experimentos automáticamente inválidos: indica menor inspectabilidad y reproducibilidad, y contiene clases documentales distintas. Esta contabilidad no convierte cada unidad en PASS y no duplica copias.

La derivación completa de 193/125/68, los subtotales por campaña y la distinción entre primarios preservados y resúmenes sin primarios están en [`EXPERIMENTAL_EVIDENCE_REFERENCE.md`](EXPERIMENTAL_EVIDENCE_REFERENCE.md).

### 8.1 Resultados demostrados

- Reapertura entre procesos, init, info y stats en múltiples campañas.
- RX IEEE canal 25 ambiental y de peer.
- IEEE RAW: 20/20 y 10/10 en direcciones documentadas; validación reciente 9/10 y 10/10.
- IEEE FRAME: 10/10 en el caso documentado.
- Rechazo de TX durante RX con `ERR_INVALID_STATE=0x05`, seguido de recuperación.
- Switching IEEE 25→20→25 observado una vez el 21-oct: 10/10, control negativo 0/10, 10/10 y retorno 9/10; global BLOCKED por timeout final de `RX_STOP`.
- Proprietary FSK/GFSK: recuperación 60/60 y campaña positiva de trece presets con 10/10 por preset; RF-02 reverso 10/10.

### 8.2 Resultados parciales o fallidos

- BURST 5 y 40 entregó un marcador en los casos exigentes; no cumplió el conteo.
- CONTINUOUS 250 ms produjo un hit; intervalo 0 produjo actividad repetida (30, 37 o 99/102 según corrida), sin equivalencia uno-a-uno de emisiones.
- `RX_STOP` presenta timeouts recurrentes después de tráfico IEEE; se observan respuestas `0x90` inesperadas, pero la causa end-to-end no está demostrada.
- Una campaña propietaria registró 240 ACK y 0 paquetes peer; FAIL del criterio receptor, causa física inconclusa.
- RF-03 se bloqueó antes de llegar a Sub-GHz tanto el 20 como el 21 de octubre. No valida ni refuta switching IEEE↔Sub-GHz.
- RF-04 KillerBee se bloqueó al no resolver `killerbee>=3.0.0`; no abrió hardware ni generó PCAP.
- RF-05 OOK no se ejecutó.

### 8.3 Contradicciones preservadas

F29 afirma 70/70 sin transcript; la campaña posterior produjo 0/70 para el mismo subconjunto y 0/240 en 24 corridas; después hubo 60/60, trece presets 10/10 y RF-02 10/10 inverso. No se conoce la identidad exacta de binarios, estado inicial, equivalencia de reset, antenas/geometría ni RF medida. La conclusión defendible es que la entrega Sub-GHz raw **es posible y fue reproducida**, pero no es universalmente confiable ni tiene causa raíz cerrada para los fallos.

### 8.4 Cobertura pendiente: lectura correcta

| Familia | Cobertura histórica | Cobertura reciente | Brecha real / readiness |
|---|---|---|---|
| OOK 868 | Reporte 10/10 raw | RF-05 NOT RUN porque no se cumplieron gates/autorización | Repetición actual y recuperación post-power-cycle; CONDITIONAL |
| BLE | Reporte raw por 1M/2M/Coded | Sin campaña externa reciente; razón específica no establecida | Raw repetible; peer externo condicional; stack completo fuera de alcance actual |
| Crypto | Reporte 9/9 con vectores descritos | No repetido recientemente; razón no establecida | Script actual permite repetición si el entorno satisface la dependencia declarada |
| Emulación | Reporte de firmas raw | Sin peer protocolario reciente; razón de no repetición raw no establecida | Raw estrecho posible; interoperabilidad bloqueada por implementación |
| Wi-SUN/Sidewalk/W-MBus | Raw reciente positivo para presets aplicables | Sin peer protocolario | No necesitan otro marker para probar protocolo; faltan framing/stack/peer |
| MIOTY | Claim 0/10 de marker raw | Sin TS-UNB | Test raw FSK opcional y estrecho; TS-UNB/interoperabilidad bloqueados |
| KillerBee | Tests host/código | RF-04 bloqueado por dependencia antes del hardware | Corregir packaging antes de PCAP HIL |
| CW/PRBS/potencia/switch | ACK o control lógico | Sin instrumento independiente | CONDITIONAL a analizador/power meter/probing |
| Spectrum, 2.4 proprietary, 169, modos 4/5/6 | Claims/control estático desigual | No ejecución válida actual | Bloqueados por implementación, hardware o contrato; no transmitir |

La matriz detallada de prerrequisitos, scripts, contribución esperada y riesgos está en [PENDING_VALIDATION_ASSESSMENT.md](PENDING_VALIDATION_ASSESSMENT.md).

## 9. Limitaciones determinantes

1. **Metadata proprietary:** `RadioIF_startSub1ghzRfBackend()` habilita `bAppendRssi`, `bAppendTimestamp` y `bAppendStatus`, cuyo layout efectivo es RSSI→timestamp→status; el parser calcula `status_idx=pdu_len`, `rssi_idx=status+1`, `timestamp_idx=rssi+1`. Ese desplazamiento determinista afecta prueba CRC/status, RSSI y timestamp. `channel` es estado residual y LQI se fija en cero.
2. **Contrato asincrónico:** eventos RX pueden perderse durante respuestas síncronas.
3. **Lifecycle:** `RX_STOP` no es estable; `RADIO_INIT` no vuelve a ejecutar `RadioIF_init()`.
4. **TX diferido:** ACK temprano sin `operation_id`, progreso, completion ni conteo ejecutado.
5. **Configuración:** `SET_PROP_CONFIG` carece de validación semántica/readback; potencia efectiva tampoco se reporta.
6. **Identidad:** `1.0.0`/`FERALRF1` no trazan source→artifact→device.
7. **RP2040:** Cat-Shell se infiere por +2; no hay API segura de banda; contadores no cubren USB→CC ni colas CC.
8. **Bandas especiales:** 169 MHz no es defendible; en proprietary 2.4 GHz `SET_PROP_CONFIG` almacena 2440 MHz y `loDivider=0`, pero `RadioIF_applySub1ghzChannelConfig()` sólo usa la frecuencia pedida si es menor que 1000 MHz y la ruta GFSK termina aplicando el fallback 868 MHz; 433 MHz requiere verificación de placa/front-end/antena.
9. **OOK:** los patches `genook` no pueden descargarse con seguridad en el lifecycle actual; `s_prop_ook_active` selecciona `Prop0_modeOok` y cambiar después de modo/frecuencia requiere power-cycle. RF-05 quedó NOT RUN.
10. **Protocolos:** nombres exceden niveles realmente implementados.
11. **Reproducibilidad:** faltan hashes de binarios y medición RF independiente.

## 10. Asuntos no resueltos

- Punto exacto donde se pierde o retrasa la respuesta `RX_STOP`.
- Efecto runtime de la cola compartida sin sincronización.
- Relación física Band/CTF/U2 en la revisión P7 disponible.
- Parámetros radiados efectivos por preset y potencia.
- Por qué BURST y CONTINUOUS con intervalo positivo entregan un solo marcador.
- Contrato de datos RX: inclusión de length/header/CRC por PHY.
- Requisitos de producto: PHY/raw, framing/parsing, emulación o pilas seleccionadas.
- Soporte distribuible de KillerBee y alcance regulatorio de jamming/high-PA.

## 11. Evaluación de madurez

| Dimensión | Evaluación |
|---|---|
| Arquitectura base | Coherente y funcional, con separación PC/RP2040/CC1352 clara |
| Control PHY/raw | Funcional en escenarios demostrados |
| Robustez lifecycle | Insuficiente para uso automatizado prolongado |
| Contrato API/wire | Utilizable, pero ambiguo para asincronía, completion, identidad y readback |
| Exactitud de observabilidad | Parcial; metadata PROP y métricas end-to-end deficientes |
| Protocolos superiores | No implementados salvo framing/constructores limitados |
| Reproducibilidad | Insuficiente por identidad de artefacto y evidencia histórica desigual |
| Preparación de producto | Investigación avanzada/prototipo; no soporte protocolario certificado |

## 12. Conclusiones

FeralRF tiene una base real y demostrada para controlar el CC1352P7 y transferir paquetes raw. Sus resultados positivos no deben minimizarse: IEEE y varias configuraciones proprietary funcionan entre dos CatSniffer. Al mismo tiempo, la evidencia exige limitar las afirmaciones a los casos medidos y al nivel PHY/raw.

El siguiente desarrollo debería priorizar reproducibilidad, exactitud de metadata, contrato de eventos/completion, lifecycle RX/TX y readback de configuración antes de ampliar protocolos. La decisión de implementar MIOTY, Wireless M-Bus, Wi-SUN, Sidewalk, emulación avanzada o stacks completos debe tomarse con el asesor a partir del rol deseado, no de los nombres existentes.

Documentos complementarios:

- [Matriz canónica de validación](VALIDATION_MATRIX_CURRENT.md)
- [Brechas funcionales](FUNCTIONAL_GAPS.md)
- [Propuesta priorizada](DEVELOPMENT_PROPOSAL.md)
- [Resumen ejecutivo](EXECUTIVE_SUMMARY.md)
- [Integración de hardware](HARDWARE_INTEGRATION.md)
- [Referencia autocontenida de evidencia experimental](EXPERIMENTAL_EVIDENCE_REFERENCE.md)
- [Evaluación de readiness de pruebas pendientes](PENDING_VALIDATION_ASSESSMENT.md)

