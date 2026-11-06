# Evidencia, procedencia y preguntas abiertas

## Índice de fuentes

### Inventario primario completo al inicio de la revisión

| Directorio | Archivos revisados | Contenido |
|---|---:|---|
| `05 - Evaluación/` | 18 Markdown | Auditoría EV; EV-03/04/05/06/10; tres documentos EV-11; dos EV-12; EV-13/14; guía, matriz, wiki, guía OTA y registro. |
| `06 - Experimentos/` raíz | 8 Markdown | Experimentos 01–05, auditoría final, checklist OTA y seguimiento Sub-GHz. |
| `06 - Experimentos/EXPERIMENT_REPORT_2026-10-12/` | 10 Markdown + 1 CSV | README, procedure, execution report, historical audit, matriz Markdown/CSV y cinco copias de los experimentos 01–05. |
| `06 - Experimentos/EXPERIMENT_REPORT_2026-10-20/` | 9 Markdown | RF-01 (dos documentos), RF-02 (dos), RF-03 (dos) y RF-04 (tres). |
| `06 - Experimentos/Pruebas OTA SubGHz/` | 4 Markdown | Registro extenso y tres auditorías metodológica/final/causal. |
| **Total** | **49 Markdown + 1 CSV** | **50 artefactos primarios**; seis pares tienen contenido idéntico y no suman evidencia independiente. |

### Vault — evaluación EV

- [Auditoría técnica EV](<../../05 - Evaluación/Auditoría técnica de validación FeralRF - EV ejecutadas.md>): reconstrucción EV-00…EV-14 y correcciones metodológicas.
- [EV-05](<../../05 - Evaluación/EV-05 — Primera observación RF IEEE.md>): tres ventanas formales IEEE.
- [EV-10](<../../05 - Evaluación/EV-10 — Matriz de PHY por control.md>) y los tres documentos EV-11: control multi-PHY/presets.
- [EV-12 OTA](<../../05 - Evaluación/EV-12 — TX RAW FRAME BURST CONTINUOUS por aire.md>): marcadores, conteos y stop.
- [EV-13](<../../05 - Evaluación/EV-13 — CW PRBS y TX_TEST_STOP por control.md>) y [EV-14](<../../05 - Evaluación/EV-14 — Eventos RF asíncronos y firma RX.md>).

### Vault — campañas OTA

- Documentos 01–05 bajo `06 - Experimentos/` y copias exactas bajo `EXPERIMENT_REPORT_2026-10-12/experiments/`.
- [CSV de 47 filas](<../EXPERIMENT_REPORT_2026-10-12/validation_matrix_20261012_113056.csv>): 44 ejecutadas y 3 planificadas/no ejecutadas.
- [Auditoría de comandos](<../EXPERIMENT_REPORT_2026-10-12/historical_command_audit.md>): diferencia entre procedimiento exacto, variante, error e incompletitud.
- [Auditoría causal Sub-GHz](<../Pruebas OTA SubGHz/Auditoría causal Codex - comparación histórica OTA SubGHz.md>): 70/70 vs 0/70/0/240 y hipótesis.
- Reportes RF-01…RF-04 del 20-oct y sus JSONL locales en `FeralRF/validation_runs/next_rf_20261020_091040/`.

Los nueve JSONL locales son `next_rf_20261020_090857/00_preflight.jsonl` y, bajo `next_rf_20261020_091040/`, `00_preflight.jsonl`, `01_rf01_ieee_channel_switch.jsonl`, `01b_preflight_recovery.jsonl`, `01c_rf01_recovery_control.jsonl`, `02a_preflight_rf02.jsonl`, `02_rf02_subghz_reverse.jsonl`, `03a_preflight_rf03.jsonl` y `03_rf03_cross_phy_switch.jsonl`.

### FeralRF y hardware

- `FeralRF@fbff1b7`: `docs/ARCHITECTURE.md`, `docs/PYTHON_API.md`, `docs/VALIDATION_MATRIX.md`.
- `docs/OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-16/`: auditorías, recovery record y matriz E01–E24.
- `python/feralrf/{radio.py,presets.py,enums.py,commands.py,__init__.py}` y scripts citados.
- `CatSniffer-Firmware/RP2040/catsniffer/src/{main.c,shell_commands.c}` y `boards/rpi_pico.overlay`.
- Notas del Vault `01 - Hardware/Arquitectura CatSniffer v3.1.md` y `02 - Firmware/Firmware CC1352P7.md`.

## Procedencia Git relevante

### Vault

- `51c2702` (02-oct): primeros reportes de evaluación.
- `e3cbe2e`, `bce7a55` (07-oct): normalización, contraste y correcciones EV.
- `eb81190`, `aa117b5`, `43fc4cd` (08–12-oct): presets, campañas OTA y auditoría final.
- `a62bd87` (12-oct): paquete de experimento y matriz.
- `19cca8c`, `2a4b002` (16-oct): auditorías y segunda campaña Sub-GHz.
- `c07aeeb` (20-oct): documentos RF recientes.

### FeralRF

- `68ed18f`/`04caba9`/`f5863c3` (03-may): smoke F29 y claim 70/70.
- `2595276` y commits próximos: presets Wi-SUN; `6e34ef2`: MIOTY pending.
- `7ad0a1a`, corregido por `cd958c3` (22-jul): matriz e invariantes RF.
- `ba27fbe`, `0f893f4` (12-oct): reporte de validación.
- `fbff1b7` (16-oct): reporte de segunda campaña Sub-GHz.

La rama local del 20-oct contiene scripts y logs sin seguimiento posteriores a HEAD. Ningún SHA debe atribuirles identidad implícita.

## Duplicación documental

Seis contenidos son copias exactas: la guía/procedure y los cinco documentos de experimento 01–05. Se conservaron como rutas distintas por procedencia, pero cada par representa una sola fuente experimental.

## Contradicciones no resueltas

### Sub-GHz: 70/70, 0/70 y éxito posterior

F29 afirma 70/70; octubre registra 0/70 para los mismos siete nombres; recuperación posterior produce 60/60 y trece presets 10/10. No se conocen binarios CC exactos, estado físico inicial, geometría/antenas ni equivalencia del reset. La evidencia contradice una incapacidad permanente y contradice también una fiabilidad universal. Causa raíz: **no establecida**.

### `band2` y ruta física

Firmware/esquema vinculan `band2` a CTF `0/0/1`, pero no hubo sondeo eléctrico. 3D/3E/4A funcionaron reportando `Band:0`. Esto invalida usar el estado textual como condición causal necesaria en enlace cercano, no invalida la topología documentada ni demuestra qué throw estaba activo.

### Canal propietario y RSSI

RF-02 pidió canal 0/902,2 MHz y recibió diez eventos con `channel:25`; RSSI incluyó valores positivos e intervalos extremos. Los payloads prueban entrega host-visible, pero esos metadatos no son fiables hasta auditar serialización, campo residual y conversión signed.

### Reset y pinout

`FeralRF/hardware/PINOUT.md` menciona RESET_CC GPIO15; el overlay RP2040 ejecutable usa GPIO3 para reset y GPIO2 para boot. Puede ser diferencia de nomenclatura/revisión. Para procedimientos, usar Cat-Shell y puertos explícitos; no afirmar un pin físico sin reconciliar placa/firmware.

### Presets con nombres de protocolo/modulación

Wi-SUN/Sidewalk/W-MBus son etiquetas de parámetros, no pilas. Los valores 4/5/6 están reservados en la estructura TI auditada, pese a nombres MSK/4-FSK/4-GFSK. La evidencia de ACK no resuelve esta discrepancia.

## Evidencia importante no disponible

- Transcript crudo F29 70/70 y baseline completo de abril.
- Hashes/artefactos instalados de CC1352P7 y UF2 de RP2040 por campaña.
- Logs individuales de nueve presets EV-11 902/915.
- Capturas RF instrumentales de frecuencia, potencia, modulación y cese.
- Estado eléctrico CTF/U2 y pérdida de ruta.
- PCAP KillerBee de RF-04; no fue generado.
- Ejecución RF-05 OOK; no ocurrió.
- Dos stdout completos de las tres transiciones narradas en EV-14.

## Preguntas priorizadas para Fase II

1. ¿Por qué `stop_rx()` ignora respuestas `0x90` y agota timeout después de recibir tráfico? Auditar secuencia, cola, `expected`, SEQ y retry sin modificar comportamiento todavía.
2. ¿Qué significan y cómo se serializan `channel`, RSSI, LQI y timestamp en cada PHY? Explicar `channel=25` y RSSI positivos de RF-02.
3. ¿Qué confirma realmente `SET_PROP_CONFIG` y qué errores backend pueden quedar asíncronos o silenciosos?
4. ¿Cómo se programan y completan BURST/CONTINUOUS/TX_STOP, y existe un evento/contador de TX efectivo?
5. ¿Qué implementación real corresponde a `mod_type=4/5/6` frente al header TI? Corregir sólo después de la auditoría.
6. ¿Qué nombres de protocolo tienen framing/stack real y cuáles son sólo presets PHY?
7. ¿Cómo debe descubrirse Cat-Shell por identidad USB/MI en lugar de `Bridge + 2`? ¿Qué API pública es segura?
8. ¿Qué revisiones/pines describen correctamente reset/boot y front-end de la placa física P7?
9. ¿Cómo debe distribuirse KillerBee y qué combinaciones OS/Python están soportadas?
10. ¿Cómo registrar build ID/hash de firmware CC y RP para unir source → artifact → dispositivo?
11. ¿Cuál es el alcance estable real de OOK y su recuperación obligatoria?
12. ¿Qué claims de `VALIDATION_MATRIX.md` pueden conservarse tras recuperar o no recuperar evidencia cruda?

## Criterio para resolver futuras contradicciones

Una nueva ejecución debe fijar repositorio y diff, hash del script, Python/dependencias, IDs USB, firmware/build IDs, antenas/geometría, comando exacto, stdout/JSONL y cleanup. Para claims físicos se añade instrumento independiente. Un resultado nuevo no reescribe el antiguo: se relaciona como repetición, contradicción o supersesión con condiciones explícitas.

