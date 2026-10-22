# Referencia independiente de evidencia experimental

**Corte de evidencia:** 2026-10-22  
**Propósito:** conservar dentro de FeralRF la procedencia, configuración, criterios y resultados esenciales usados por los informes canónicos, sin exigir el repositorio de investigación externo.

## 1. Alcance y clases de evidencia

Este apéndice no es un log primario nuevo y no reproduce el archivo de investigación histórico. Resume evidencia inspeccionada durante la auditoría y señala qué material sí está incluido en FeralRF.

| Clase | Significado y uso permitido |
|---|---|
| Registro experimental primario | Output, captura, log, CSV/JSONL o medición contemporánea directamente inspeccionable |
| Reporte experimental documentado | Narrativa técnica con condiciones, procedimiento, observaciones o conteos. Es evidencia válida de lo que reporta aunque no incluya stdout completo |
| Resumen experimental consolidado | Documento posterior que agrupa experimentos identificables; conserva el alcance y las limitaciones de sus fuentes |
| Claim histórico de implementación | README, commit o comentario con resultado declarado pero sin contexto experimental suficiente para tratarlo como reporte equivalente |
| Evidencia estática | Conclusión derivada del source/SDK; demuestra presencia, ausencia o contrato, no ejecución física |
| Medición física independiente | Instrumento o peer externo apropiado que mide frecuencia, potencia, espectro, modulación, ruta o interoperabilidad |

Una cifra conserva su alcance original. Un ACK no se convierte en transmisión RF; un marker recibido no demuestra frecuencia o potencia; un enlace entre dos CatSniffer no demuestra interoperabilidad con una implementación independiente. La falta de output crudo reduce inspección independiente, reproducibilidad y análisis causal, pero **no invalida por sí sola una observación consignada en un reporte técnico**.

Cada conclusión debe separar cuatro dimensiones:

- **observación reportada:** qué afirma el reporte que ocurrió;
- **inspectabilidad independiente:** qué artefactos completos puede examinar otro revisor;
- **reproducibilidad:** si están preservadas condiciones suficientes para repetir;
- **interpretación causal:** si la evidencia establece por qué ocurrió.

## 2. Instantánea y limitaciones comunes

- Source FeralRF usado para la evaluación final: `feature/hardware-validation` @ `fbff1b7f5077964418fb63a3fdf346a27c35a003`.
- SDK: `simplelink_cc13xx_cc26xx_sdk_8_30_01_01` @ `5b31d0a4903351e544546e23ef3330eaa4291ceb`.
- Los binarios CC1352P7 instalados en las campañas no quedaron vinculados a un SHA/hash. `1.0.0` y `FERALRF1` no son identificadores suficientes.
- Los reportes recientes registraron RP2040 `v3.1.0.1`, pero no conservaron el hash del UF2 instalado.
- No se preservó instrumentación independiente de frecuencia, potencia, espectro, modulación o posición eléctrica del RF switch.
- Las unidades se describieron como CatSniffer v3 con CC1352P7; la revisión eléctrica exacta y la equivalencia entre placas/campañas permanecen abiertas.

## 3. Evidencia incluida en FeralRF

### 3.1 Campaña del 6–12 de octubre de 2026

El directorio [OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-12](OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-12/README.md) está versionado e incluye procedimiento, matriz Markdown/CSV, auditoría de comandos y cinco reportes de experimento.

Fuentes principales:

- [Matriz de 2026-10-12](OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-12/validation_matrix_20261012_113056.md).
- [Informe de ejecución](OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-12/execution_report.md).
- [IEEE RAW, FRAME y BURST](OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-12/experiments/01_ieee_raw_frame_burst.md).
- [IEEE CONTINUOUS y STOP](OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-12/experiments/02_ieee_continuous_stop.md).
- [Proprietary GFSK 868/915](OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-12/experiments/03_proprietary_gfsk_868_915.md).
- [Campaña proprietary ampliada](OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-12/experiments/04_proprietary_extended_campaign.md).
- [CONTINUOUS/STOP dirigido](OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-12/experiments/05_ieee_continuous_stop_directed.md).

Resultados esenciales:

| Caso | Configuración/criterio | Observación | Clasificación usada |
|---|---|---|---|
| IEEE RAW A→B | Canal 25, marker dirigido | 20/20 | PASS acotado |
| IEEE RAW B→A | Canal 25 | 10/10 | PASS acotado |
| IEEE FRAME | Canal 25 | 10/10 | PASS acotado; no prueba constructor MAC |
| BURST | 40×25 ms | 1 marker | FAIL del umbral; emisiones físicas inconclusas |
| BURST | 5×250 ms | 1 marker | FAIL del umbral; emisiones físicas inconclusas |
| CONTINUOUS | 250 ms | 1 marker | PARTIAL |
| CONTINUOUS | intervalo 0 | 30 hits en la matriz; otra ejecución histórica indicó 99/102 | Actividad repetida; no conteo físico universal |
| STOP dirigido | A observado antes del stop | B posterior 0/3 y 0/10×2 | Criterio posterior FAIL; causa del stop inconclusa |
| Proprietary ampliada | 24 corridas, diez `transmit()` por corrida | 240 ACK, 0 paquetes peer | FAIL del criterio receptor; causa RF INCONCLUSIVE |

### 3.2 Recuperación y campaña del 15–16 de octubre de 2026

El directorio [OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-16](OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-16/VALIDATION_MATRIX_261016) está versionado. Incluye auditorías de procedencia/recuperación, matriz E01–E24 y scripts de checkpoints. Los `.log` crudos no forman parte del conjunto versionado listado por Git; los reportes incluidos son la referencia publicable.

Fuentes:

- [Matriz 261016](OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-16/VALIDATION_MATRIX_261016).
- [Auditoría de procedencia](OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-16/SUBGHZ_PROVENANCE_AUDIT.md).
- [Auditoría de recuperación](OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-16/SUBGHZ_RECOVERY_AUDIT.md).
- [Registro de recuperación](OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-16/SUBGHZ_RECOVERY_RUN.md).

Resultados esenciales:

| Grupo | Configuración | Observación | Límite |
|---|---|---|---|
| 3A, 3A-R, 3C, 3D, 3E, 4A | `wisun_915_fsk_50k`, 902.2 MHz solicitados | 10/10 en seis checkpoints; 60/60 total | Entrega raw, no Wi-SUN ni medición de 902.2 MHz |
| E02–E24 | Trece presets FSK/GFSK entre 868 y 915/902.2 MHz solicitados | Trece configuraciones positivas con 10/10; cinco corridas BLOCKED por incidentes de puerto/reset/timeout | No heredar PASS a protocolos o presets no ejecutados |
| Estado `Band` | Checkpoints con `Band: 1` y otros con `Band: 0` | Hubo entrega en ambos estados lógicos | No prueba posición eléctrica de U2 |

## 4. Evidencia histórica esencial no completamente incluida

La consolidación auditada identificó 193 unidades de ejecución/intento. En 125 existen registros o reportes detallados directamente inspeccionables en el corpus revisado; 68 dependen de reportes agregados, resúmenes o claims con artefactos primarios no recuperados. **68 no significa 68 experimentos inexistentes o inválidos**: describe una limitación de inspectabilidad y reproducción, con fuerza distinta según la clase documental.

| Familia | Unidades | Registro/reporte detallado inspeccionado | Reporte agregado, resumen o claim sin primarios completos | Disponibilidad en FeralRF |
|---|---:|---:|---:|---|
| Baseline abril 2026 | 46 | 0 | 46 | [VALIDATION_MATRIX.md](VALIDATION_MATRIX.md) es un reporte agregado con fechas, conteos y resultados; primarios no recuperados |
| F29 mayo 2026 | 7 | 0 | 7 | Presets/source y claim histórico; transcript no incluido |
| EV-00…EV-14 | 63 | 52 | 11 | Reportes EV inspeccionados; parte del corpus no está incluido en FeralRF |
| Campañas 6–12-oct | 43 | 39 | 4 | Reportes/matriz incluidos en `docs/OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-12/` |
| Recuperación 15–16-oct | 29 | 29 | 0 | Reportes/matriz/scripts incluidos; logs crudos no todos versionados |
| Campaña RF 20-oct | 5 | 5 | 0 | Resumen auditado aquí; JSONL primarios no incluidos en el checkout actual |
| **Total** | **193** | **125** | **68** | Cobertura documental de inspectabilidad desigual |

### 4.1 Baseline y F29

- [VALIDATION_MATRIX.md](VALIDATION_MATRIX.md) funciona como **reporte experimental documentado agregado** del baseline de abril–mayo: identifica fechas/fases, scripts, configuraciones, conteos y resultados. Sustenta las observaciones reportadas, entre ellas BLE raw 1M 10/10, 2M 8/10, Coded S8/S2 10/10; OOK 868 10/10; crypto 9/9; y emulación wire/raw. La falta de logs por fila impide inspeccionar cada paquete, verificar el binario o reproducir exactamente las condiciones; no autoriza causas físicas ni interoperabilidad superior.
- F29, fechado 3-may-2026 y asociado al commit histórico `f5863c3`, afirmó 70/70 para siete presets nominales Sidewalk/Wi-SUN. El script puede reconstruirse desde el historial, pero el transcript, los binarios instalados, la asociación exacta de puertos y el resultado del reset no están incluidos. Clasificación: **claim histórico / reproducción inconclusa**.
- El comentario de `mioty_868_tsunb` registra intentos 0/10 para FSK/GFSK y varios `rx_bw`. El registro primario completo no fue recuperado. Además, el objetivo fue marker raw entre dos FeralRF, no un telegrama TS-UNB frente a una base station MIOTY. No demuestra incompatibilidad del silicio ni interoperabilidad MIOTY.

Lectura por caso relevante:

| Caso histórico | Observación que sí se conserva | Lo que no puede elevarse desde ese documento |
|---|---|---|
| BLE raw | Conteos reportados por PHY | Interoperabilidad actual, exactitud RF o stack BLE completo |
| OOK 868 | Entrega raw 10/10 reportada con recuperación declarada | Descarga efectiva del patch, espectro/profundidad OOK o recuperación del build actual |
| Crypto | Nueve pruebas de hardware reportadas como PASS; el script de vectores existe | Que el binario actual sea idéntico o que el TRNG quede certificado |
| Emulación | Firmas PHY/raw reportadas | State machines, autenticación o interoperabilidad con dispositivo independiente |
| 433/2440/modos reservados | Conteos históricos reportados | La causa física, frecuencia efectiva o la modulación indicada por el nombre |

### 4.2 EV-00…EV-14

Resultados conservados porque condicionan el diagnóstico actual:

| ID | Observación | Fuerza/límite |
|---|---|---|
| EV-00 | Mapeo de puertos mostró que COM88/COM87 contradice Bridge+2 | Observación operativa; identidad USB completa pendiente |
| EV-02/05 | Cinco ventanas IEEE; tres formales 41/43/43, dos preliminares 48/100 | RX IEEE demostrado; fuente ambiental no siempre atribuida |
| EV-03 | Cinco reaperturas/init entre procesos | No probó pérdida USB o reconexión de la misma instancia |
| EV-04 | Recuperación manual `boot`/`exit` 1/1; reset automático habría usado COM90 | API de reset no validada en ese mapa |
| EV-06 | Dos rechazos TX-durante-RX con error `0x05` y recuperación | PASS de contrato de estado |
| EV-10 | PHY 0–7: 8/8 por control | No prueba RF de cada PHY |
| EV-11 | 27 presets por control; 18 stdout individuales y nueve resumidos | ACK/configuración, no modulación ni OTA |
| EV-12 | RAW/FRAME/BURST/CONTINUOUS aceptados; OTA como en §3.1 | Separa ACK, scheduling y marker peer |
| EV-13 | CW, PRBS15, PRBS32 y stop con ACK | Sin analizador, potencia o cese medidos |
| EV-14 | RX normal y transición BLE→IEEE mínima, más tests host/mock | No reprodujo de forma concluyente el error RF buscado |

## 5. Campaña RF del 20 de octubre y validación del 21 de octubre

Los reportes técnicos y JSONL de estas corridas se inspeccionaron durante la auditoría, pero **no están incluidos en el checkout FeralRF actual**. Los siguientes registros preservan las observaciones de esos reportes documentados; no se presentan como logs originales ni permiten una nueva inspección byte a byte desde este checkout.

### 5.1 RF-01 — switching IEEE

| Fecha/ejecución | Etapas observadas | Cleanup | Resultado |
|---|---|---|---|
| 20-oct, corrida inicial | 25/25: 10/10 | `RX_STOP` timeout; no llegó a 20 | BLOCKED global |
| 20-oct, recuperación | 25/25: 10/10 | Timeout reproducido | BLOCKED global |
| 21-oct, nueva ejecución | 25/25: 10/10; 25/20: 0/10; 20/20: 10/10; retorno 25/25: 9/10 | Stop final timeout | **PASS estrecho de selección de canal; BLOCKED global del experimento** |

El criterio estrecho es selectividad: recepción en canales coincidentes, ausencia en mismatch y recuperación al canal inicial. No incluye cleanup. El criterio completo sí exige cleanup; por eso los dos estados no se contradicen.

### 5.2 RF-02 — proprietary reverso

- Dirección: CatSniffer #3 → #2.
- Preset: `wisun_915_fsk_50k`; frecuencia solicitada 902.2 MHz.
- Resultado: 10 ACK, 10 markers, stop y cleanup correctos.
- Clasificación: PASS funcional de entrega raw.
- Anomalías: eventos con `channel=25` y RSSI positivos/negativos incoherentes. No se midieron frecuencia, potencia o modulación. Estas observaciones apoyan el defecto de metadata, no invalidan el payload recibido.

### 5.3 RF-03 — transición IEEE↔Sub-GHz

- 20-oct: primera etapa IEEE 10/10; `RX_STOP` timeout; ninguna etapa Sub-GHz completada.
- 21-oct: primera etapa IEEE 7/10; timeout equivalente; tampoco alcanzó Sub-GHz.
- Clasificación: BLOCKED. No demuestra fallo ni éxito de la transición cross-PHY.

### 5.4 RF-04 y RF-05

- RF-04 KillerBee: el entorno no resolvió `killerbee>=3.0.0`; no se abrió hardware ni se generó PCAP. BLOCKED por packaging/dependencia, no fallo RF.
- RF-05 OOK 868: planificado con criterio ≥9/10, power-cycle y control IEEE posterior; no fue autorizado/ejecutado. NOT RUN.

### 5.5 Validaciones manuales del 21 de octubre

| Procedimiento | Observación | Interpretación |
|---|---|---|
| OP-04 RX IEEE | 54 paquetes en 30 s, primer paquete `crc_ok=True` | Reproduce RX bajo condiciones parcialmente comparables |
| OP-05 exclusión TX/RX | `error_code=0x05`; stop y stats correctos | Confirma código de rechazo |
| OP-06 RAW | 9/10 en una corrida, 10/10 en otra; negativo 0 | Entrega reproducida; ventana mayor no se declara causa |
| OP-08 BURST | count solicitado 5, marker 1/5 | Limitación reproducida |
| OP-09A | CONTINUOUS 250000 µs, un hit | PARTIAL |
| OP-09B | intervalo 0, 37 hits CRC-good; TX_STOP ACK; RX_STOP timeout con último `0x90` | Actividad repetida + síntoma lifecycle; causalidad completa abierta |
| OP-10 | Cat-Shell posterior: `Band:0`, counters cero | No describe retrospectivamente la banda durante CW/PRBS |

## 6. Contradicciones que deben preservarse

### 6.1 Sub-GHz

F29 70/70, repetición 0/70, campaña 0/240, recuperación 60/60, trece presets 10/10 y RF-02 10/10 no son resultados equivalentes. Difieren en procedencia, binarios no identificados, estado inicial, reset, geometría y documentación. Juntos demuestran que la entrega raw es posible y que la fiabilidad histórica no está explicada. No establecen una causa raíz ni una regresión confirmada.

### 6.2 `RX_STOP`

El source confirma que ciertos eventos RX pueden descartarse por secuencia durante una espera síncrona. Los timeouts de `RX_STOP` también están confirmados experimentalmente. Sin una traza correlacionada de host, UART, cola CC y bridge, no está probado que el descarte host sea la causa única de cada ACK faltante.

### 6.3 Estado de banda

Cat-Shell prueba el estado lógico solicitado al RP2040. Los enlaces positivos con valores distintos de `Band` impiden usar ese texto como explicación causal suficiente. No se midieron CTF1/2/3 ni U2.

## 7. Disponibilidad y publicación futura

En el checkout actual no están presentes `experimental_artifacts/` ni `validation_runs/`; los JSONL de RF-01…RF-03 citados en la investigación no son una dependencia navegable. `python/validation_artifacts/` y los scripts temporales OP-13 existen localmente sin seguimiento, pero no son fuente necesaria de los informes canónicos.

Para una publicación futura deberían incluirse, después de revisión de privacidad/tamaño y sin reescribirlos:

1. manifest de cada corrida RF-01…RF-05;
2. JSONL primarios y exit codes;
3. hashes de scripts/binarios;
4. identificación de placa/puertos;
5. reporte de cleanup y recuperación.

Hasta entonces, las observaciones recientes se conservan como **reportes documentados/resúmenes auditados no acompañados por sus primarios en este checkout**. Siguen siendo evidencia de lo reportado, pero no evidencia primaria incluida.

La evaluación de qué pruebas aportan información nueva, cuáles están bloqueadas y qué procedimientos actuales existen se encuentra en [PENDING_VALIDATION_ASSESSMENT.md](PENDING_VALIDATION_ASSESSMENT.md).

