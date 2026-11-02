---
title: "02 — OTA IEEE: CONTINUOUS y STOP (dos CatSniffer)"
date: 2026-10-08
project: CatSniffer - FeralRF
status: "Evaluación ejecutada — PARTIAL con PASS-RF en interval_us=0"
tags: [FeralRF, CatSniffer, OTA, IEEE-802-15-4, CONTINUOUS, STOP, validacion]
---

# 02 — OTA IEEE: `TX_CONTINUOUS` y `TX_STOP`

**Proyecto:** CatSniffer – FeralRF  
**Fecha de consolidación:** 2026-10-08 (fecha de ejecución exacta no registrada en stdout)  
**Tipo de documento:** registro técnico de pruebas ejecutadas, con comandos y stdout literal.  
**Resultado global:** **PARTIAL**, con **PASS-RF** de transmisión repetida para `interval_us=0`, recepción limitada para `interval_us=250000`, control post-STOP sin coincidencias en ambas variantes y un timeout transitorio de `RX_STOP`.

> **Alcance de evidencia:** se transcriben las salidas compartidas durante la sesión. No se añaden líneas de consola ni marcas de tiempo de pared no observadas. La secuencia cronológica se reconstruye del orden de los mensajes. Las afirmaciones de código/arquitectura no se consideran verificadas en el binario instalado únicamente por coincidencia de nombres/versiones.

## 1. Objetivos

**Objetivo principal.** Verificar por radio, entre dos placas físicas, que una solicitud `TX_CONTINUOUS` produce tramas IEEE 802.15.4 observables y examinar qué evidencia existe del cese posterior a `TX_STOP`.

**Objetivos específicos:**

1. Establecer un control negativo con un identificador de payload ausente antes de transmitir.
2. Observar RF durante una ventana RX que se solape con el modo CONTINUOUS del transmisor.
3. Comparar dos configuraciones de `interval_us`: `250000` (250 ms solicitados) y `0` (sin intervalo positivo solicitado).
4. Registrar bytes transmitidos solicitados, bytes RF observados, FCS aparente, CRC reportado, RSSI, timestamps y cantidad de coincidencias.
5. Separar confirmaciones de API/ACK de resultados RF, y distinguir número de tramas recibidas de número exacto de emisiones.
6. Comprobar ausencia del marcador en una nueva ventana RX posterior a la finalización de TX/STOP.
7. Registrar anomalías de transporte/control, especialmente el timeout de `RX_STOP`, sin adjudicar causas que las evidencias no demuestran.

## 2. Referencias y alcance

**Plan experimental empleado:** `Guía enfocada OTA FeralRF - TX RX con dos CatSniffer.md`, §4 «CONTINUOUS + STOP físico»; `Checklist OTA FeralRF - dos CatSniffer.md`, §«CONTINUOUS + STOP». La guía prescribe un control negativo, RX previo a TX, emisión de 3 s con intervalo de 250 ms, ventana post-STOP de 5 s y una segunda variante de 1 s con intervalo 0; exige guardar stdout literal y no confundir conteo RX con conteo emitido.

**Artefactos software involucrados (rutas observadas en comandos):**
- `python/examples/lab/ota_rx_probe.py`: control del receptor y filtrado del marcador; se observaron `INFO`, configuración, `RX_START`, lectura y `RX_STOP`.
- `python/examples/smoke_tx_continuous_phase1.py`: configuración del transmisor, solicitud `TX_CONTINUOUS`, pausa `run-seconds` y solicitud `TX_STOP`; se observaron ACK de ambas solicitudes.

**Versiones/identidad:** ambas utilidades reportaron `firmware=1.0.0`, `capabilities=0x07`, `serial=464552414c524631`; este campo no es una identificación única demostrada de cada placa. Contexto de proyecto registrado: rama `main`, commit de fuente `0178721cbd4f0d0f6f8eba5ae919ca46066d5dea`, Python `0.3.0`, TI SDK `simplelink_cc13xx_cc26xx_sdk_8_30_01_01`; **no se ha demostrado una relación binaria reproducible entre ese commit y los firmwares instalados**.

**Límites:** no hubo captura independiente con analizador de espectro, sniffer externo ni osciloscopio; no se midió el instante exacto de salida de STOP al aire ni se hizo inventario de pérdidas RF. Tampoco se conservan relojes de pared compartidos que permitan medir la latencia entre comandos de terminales distintas.

## 3. Montaje y condiciones

| Campo | Configuración utilizada |
| --- | --- |
| Dispositivos | Dos CatSniffer V3 físicos |
| CatSniffer #1 | **Transmisor**: Bridge `COM33`; LoRa `COM34`; Shell `COM35` |
| CatSniffer #2 | **Observador RX**: Bridge `COM88`; LoRa `COM86`; Shell `COM87` |
| Protocolo/PHY | IEEE 802.15.4, parámetro FeralRF `--phy 4` |
| Canal | `--channel 25` (centro nominal IEEE 802.15.4: 2475 MHz) |
| Potencia solicitada TX | `--power 0` (0 dBm solicitados; no medidos) |
| Ruta RF | 2.4 GHz seleccionada previamente en preflight (no repetida ni verificada eléctricamente en estas salidas) |
| Interfaz | Bridge serie a 921600 baudios, vía Python |
| Sistema de comandos | Windows PowerShell, directorio raíz de `FeralRF` |
| Variante 1 | Marker `a40133008804`, `interval_us=250000`, TX 3 s, RX 10 s, umbral 5 hits |
| Variante 2 | Marker `a40233008804`, `interval_us=0`, TX 1 s, RX 7 s, umbral 20 hits |
| Filtrado | `--match-mode contains`, CRC fallido no permitido por defecto |

La selección `--phy 4` se trata como interfaz IEEE 802.15.4 a nivel PHY/raw; estas pruebas **no demuestran** interoperabilidad con stacks Zigbee, Thread u otros protocolos de red.

### 3.1 Preparación de PowerShell

La sesión contenía la siguiente ruta de trabajo y variable necesarias para reproducir los comandos:

```powershell
Set-Location 'C:\Users\Support\Documents\ec-projects\catsniffer-feralrf\FeralRF'
$env:PYTHONPATH = (Resolve-Path '.\python').Path
```

Preparar **cada una de las dos terminales** con ambos comandos, ya que las variables de entorno son locales al proceso. El ambiente Python instalado, sus dependencias exactas y cualquier activación previa de `venv` no quedan acreditados por stdout y no se inventan aquí.

## 4. Criterios predefinidos y lectura de evidencia

- **Negativo inicial:** cero coincidencias del marcador durante 5 s antes de la solicitud de TX.
- **Variante 1:** RX durante 10 s, al menos **5 coincidencias** del marcador `a40133008804`, con TX en paralelo durante 3 s y `interval_us=250000`.
- **Variante 2:** RX durante 7 s, al menos **20 coincidencias** de `a40233008804`, con TX en paralelo durante 1 s y `interval_us=0`.
- **STOP/control posterior:** ACK a `TX_STOP`, y **cero coincidencias** del mismo marcador durante la siguiente ventana RX de 5 s. El segundo criterio demuestra ausencia de observación en esa ventana; no prueba por sí solo la causalidad ni inmediatez del cese.
- **Finalización limpia RX:** `RX_STOP ACK`. Un timeout se registra como fallo del cierre del receptor incluso si se alcanzó el umbral de hits.

**Niveles:** A = recepción física atribuible por la segunda placa; B = comportamiento observado del dispositivo/receptor; C = control/ACK; D = fuente documental/código; E = inferencia; F = no evaluado. Un `SMOKE PASS` de TX no equivale a PASS-RF. El número de hits no equivale al número exacto emitido.

## 5. Cronología completa con evidencia literal

### E1 — Control negativo previo a la variante de 250 ms

**Por qué:** establecer que el observador no detectaba ya el marcador por ruido/tráfico de fondo antes de iniciar la emisión programada.  
**Esperado:** `marker_hits=0`, receptor iniciando y deteniendo correctamente.  
**Comando (Terminal A, COM88):**

```powershell
python .\python\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 5 --marker-hex a40133008804 --match-mode contains --min-hits 0 --print-limit 30
```

**Salida literal:**

```text
PS C:\Users\Support\Documents\ec-projects\catsniffer-feralrf\FeralRF> python .\python\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 5 --marker-hex a40133008804 --match-mode contains --min-hits 0 --print-limit 30
FeralRF OTA RX Probe
====================
port=COM88 baudrate=921600 phy=4 channel=25 duration=5.0s min_hits=0 marker=a40133008804 match_mode=contains allow_crc_fail=False

[STEP] Connect + RADIO_INIT + GET_INFO
[ OK ] INFO firmware=1.0.0 capabilities=0x07 serial=464552414c524631
[STEP] SET_PHY + SET_CHANNEL
[ OK ] Config ACK
[STEP] RX_START
[ OK ] RX_START ACK
[STEP] Read packets
[ OK ] packets_total=6 crc_ok=6
[ OK ] marker_hits=0
[STEP] RX_STOP
[ OK ] RX_STOP ACK

[ OK ] RX PROBE PASS hits=0 min_hits=0
```

**Resultado E1:** **PASS** del negativo (0/6 paquetes coincidentes). No prueba emisión ni STOP. Se recibieron seis tramas ambientales con CRC válido según el script.

### E2 — Primera ventana RX con umbral 5, sin salida TX asociada

**Por qué:** disponer de RX antes de la transmisión, para no perder el inicio de CONTINUOUS.  
**Esperado si hubiera TX solapado:** al menos 5 coincidencias con el marcador.  
**Observación de procedimiento:** se compartió **sólo** la salida RX; no consta ejecución TX simultánea para esta primera ventana. Por tanto, no es válido atribuir sus cero hits a un fallo del modo CONTINUOUS.

**Comando (Terminal A):**

```powershell
python .\python\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 10 --marker-hex a40133008804 --match-mode contains --min-hits 5 --print-limit 120
```

**Salida literal:**

```text
PS C:\Users\Support\Documents\ec-projects\catsniffer-feralrf\FeralRF> python .\python\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 10 --marker-hex a40133008804 --match-mode contains --min-hits 5 --print-limit 120
FeralRF OTA RX Probe
====================
port=COM88 baudrate=921600 phy=4 channel=25 duration=10.0s min_hits=5 marker=a40133008804 match_mode=contains allow_crc_fail=False

[STEP] Connect + RADIO_INIT + GET_INFO
[ OK ] INFO firmware=1.0.0 capabilities=0x07 serial=464552414c524631
[STEP] SET_PHY + SET_CHANNEL
[ OK ] Config ACK
[STEP] RX_START
[ OK ] RX_START ACK
[STEP] Read packets
[ OK ] packets_total=19 crc_ok=19
[ OK ] marker_hits=0
[STEP] RX_STOP
[ OK ] RX_STOP ACK

[FAIL] RX PROBE FAIL hits=0 min_hits=5
```

**Resultado E2:** **INCONCLUSIVE** respecto a TX/CONTINUOUS. El script falló su umbral (`0 < 5`), pero sí completó el ciclo RX y no existe constancia de una TX coetánea.

### E3 — Variante 1 coordinada: `interval_us=250000`, duración TX 3 s

**Por qué:** ejecutar simultáneamente las dos terminales, subsanando el solapamiento no acreditado en E2. Se inicia Terminal A hasta `[STEP] Read packets` y a continuación Terminal B, sin esperar a que RX termine.  
**Esperado:** ACK de `TX_CONTINUOUS` y de `TX_STOP`; cinco o más hits RF durante los 10 s RX; ACK de `RX_STOP`. Un período solicitado de 250 ms durante 3 s sugiere varias oportunidades de emisión, pero el conteo exacto esperado **no** queda probado por el comando.

**Comando RX (Terminal A):**

```powershell
python .\python\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 10 --marker-hex a40133008804 --match-mode contains --min-hits 5 --print-limit 120
```

**Comando TX (Terminal B):**

```powershell
python .\python\examples\smoke_tx_continuous_phase1.py --port COM33 --phy 4 --channel 25 --power 0 --packet-hex a40133008804 --interval-us 250000 --run-seconds 3 --tx-timeout 2
```

**Salida RX literal:**

```text
PS C:\Users\Support\Documents\ec-projects\catsniffer-feralrf\FeralRF> python .\python\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 10 --marker-hex a40133008804 --match-mode contains --min-hits 5 --print-limit 120
FeralRF OTA RX Probe
====================
port=COM88 baudrate=921600 phy=4 channel=25 duration=10.0s min_hits=5 marker=a40133008804 match_mode=contains allow_crc_fail=False

[STEP] Connect + RADIO_INIT + GET_INFO
[ OK ] INFO firmware=1.0.0 capabilities=0x07 serial=464552414c524631
[STEP] SET_PHY + SET_CHANNEL
[ OK ] Config ACK
[STEP] RX_START
[ OK ] RX_START ACK
[STEP] Read packets
[ OK ] packets_total=11 crc_ok=11
[HIT] ts=1002528444us ch=25 rssi=-57 crc_ok=True len=8 data=a401330088043df6
[ OK ] marker_hits=1
[STEP] RX_STOP
[ OK ] RX_STOP ACK

[FAIL] RX PROBE FAIL hits=1 min_hits=5
```

**Salida TX literal:**

```text
PS C:\Users\Support\Documents\ec-projects\catsniffer-feralrf\FeralRF> python .\python\examples\smoke_tx_continuous_phase1.py --port COM33 --phy 4 --channel 25 --power 0 --packet-hex a40133008804 --interval-us 250000 --run-seconds 3 --tx-timeout 2
FeralRF TX Continuous Smoke Test (Phase 1)
===========================================
port=COM33 baudrate=921600 phy=4 channel=25 power=0 len=6 interval_us=250000 run_seconds=3.0

[STEP] Connect + RADIO_INIT + GET_INFO
[ OK ] INFO firmware=1.0.0 capabilities=0x07 serial=464552414c524631
[STEP] SET_PHY + SET_CHANNEL + SET_POWER
[ OK ] Config ACK
[STEP] TX_CONTINUOUS
[ OK ] TX_CONTINUOUS ACK
[STEP] TX_STOP
[ OK ] TX_STOP ACK

[ OK ] TX CONTINUOUS SMOKE PASS
```

**Resultado E3:** **PARTIAL**. Existe al menos una recepción OTA atribuible con payload `a40133008804`, bytes completos reportados `a401330088043df6` (8 bytes, 6 del marcador + 2 bytes finales compatibles con FCS), `crc_ok=True`, RSSI −57 dBm. `TX_CONTINUOUS` y `TX_STOP` recibieron ACK; no se alcanzó el umbral RX (1/5). **No afirmar** que sólo se emitió una trama ni que el intervalo RF real fue 250 ms.

### E4 — Ventana post-STOP de variante 1

**Por qué:** comprobar si seguían apareciendo tramas con el marcador después de que la utilidad TX hubiera informado `TX_STOP ACK`.  
**Esperado:** cero hits, RX_INIT/START/STOP funcionales.  
**Observación temporal:** la ventana se ejecutó en una invocación posterior, tras una pausa entre respuestas. No mide cese inmediato.

**Comando (Terminal A; Terminal B inactiva):**

```powershell
python .\python\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 5 --marker-hex a40133008804 --match-mode contains --min-hits 0 --print-limit 40
```

**Salida literal:**

```text
PS C:\Users\Support\Documents\ec-projects\catsniffer-feralrf\FeralRF> python .\python\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 5 --marker-hex a40133008804 --match-mode contains --min-hits 0 --print-limit 40
FeralRF OTA RX Probe
====================
port=COM88 baudrate=921600 phy=4 channel=25 duration=5.0s min_hits=0 marker=a40133008804 match_mode=contains allow_crc_fail=False

[STEP] Connect + RADIO_INIT + GET_INFO
[ OK ] INFO firmware=1.0.0 capabilities=0x07 serial=464552414c524631
[STEP] SET_PHY + SET_CHANNEL
[ OK ] Config ACK
[STEP] RX_START
[ OK ] RX_START ACK
[STEP] Read packets
[ OK ] packets_total=9 crc_ok=9
[ OK ] marker_hits=0
[STEP] RX_STOP
[ OK ] RX_STOP ACK

[ OK ] RX PROBE PASS hits=0 min_hits=0
```

**Resultado E4:** **PASS** del criterio de ausencia observada post-STOP durante 5 s, no demostración de tiempo de apagado ni de causalidad exclusiva de STOP.

### E5 — Variante 2 coordinada: `interval_us=0`, duración TX 1 s

**Por qué:** comprobar si la ausencia de pausa positiva cambia el comportamiento observable de CONTINUOUS y permite evidenciar repetición RF. Se preparan los dos comandos, se arranca RX primero y se ejecuta TX mientras RX está leyendo.  
**Esperado:** ≥20 hits con marcador nuevo, ACK de `TX_CONTINUOUS`, ACK de `TX_STOP` y cierre limpio de RX.

**Comando RX (Terminal A):**

```powershell
python .\python\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 7 --marker-hex a40233008804 --match-mode contains --min-hits 20 --print-limit 160
```

**Comando TX (Terminal B):**

```powershell
python .\python\examples\smoke_tx_continuous_phase1.py --port COM33 --phy 4 --channel 25 --power 0 --packet-hex a40233008804 --interval-us 0 --run-seconds 1 --tx-timeout 2
```

**Salida RX literal íntegra:**

```text
PS C:\Users\Support\Documents\ec-projects\catsniffer-feralrf\FeralRF> python .\python\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 7 --marker-hex a40233008804 --match-mode contains --min-hits 20 --print-limit 160
FeralRF OTA RX Probe
====================
port=COM88 baudrate=921600 phy=4 channel=25 duration=7.0s min_hits=20 marker=a40233008804 match_mode=contains allow_crc_fail=False

[STEP] Connect + RADIO_INIT + GET_INFO
[ OK ] INFO firmware=1.0.0 capabilities=0x07 serial=464552414c524631
[STEP] SET_PHY + SET_CHANNEL
[ OK ] Config ACK
[STEP] RX_START
[ OK ] RX_START ACK
[STEP] Read packets
[ OK ] packets_total=38 crc_ok=38
[HIT] ts=158793830us ch=25 rssi=-57 crc_ok=True len=8 data=a40233008804f1eb
[HIT] ts=158794952us ch=25 rssi=-57 crc_ok=True len=8 data=a40233008804f1eb
[HIT] ts=158796072us ch=25 rssi=-57 crc_ok=True len=8 data=a40233008804f1eb
[HIT] ts=158797192us ch=25 rssi=-57 crc_ok=True len=8 data=a40233008804f1eb
[HIT] ts=158798311us ch=25 rssi=-57 crc_ok=True len=8 data=a40233008804f1eb
[HIT] ts=158799430us ch=25 rssi=-56 crc_ok=True len=8 data=a40233008804f1eb
[HIT] ts=158800547us ch=25 rssi=-57 crc_ok=True len=8 data=a40233008804f1eb
[HIT] ts=158801666us ch=25 rssi=-57 crc_ok=True len=8 data=a40233008804f1eb
[HIT] ts=158802785us ch=25 rssi=-56 crc_ok=True len=8 data=a40233008804f1eb
[HIT] ts=158803903us ch=25 rssi=-57 crc_ok=True len=8 data=a40233008804f1eb
[HIT] ts=158805021us ch=25 rssi=-57 crc_ok=True len=8 data=a40233008804f1eb
[HIT] ts=158806140us ch=25 rssi=-57 crc_ok=True len=8 data=a40233008804f1eb
[HIT] ts=158807259us ch=25 rssi=-57 crc_ok=True len=8 data=a40233008804f1eb
[HIT] ts=158808378us ch=25 rssi=-57 crc_ok=True len=8 data=a40233008804f1eb
[HIT] ts=158809495us ch=25 rssi=-57 crc_ok=True len=8 data=a40233008804f1eb
[HIT] ts=158810593us ch=25 rssi=-57 crc_ok=True len=8 data=a40233008804f1eb
[HIT] ts=158811712us ch=25 rssi=-57 crc_ok=True len=8 data=a40233008804f1eb
[HIT] ts=158812830us ch=25 rssi=-57 crc_ok=True len=8 data=a40233008804f1eb
[HIT] ts=158813949us ch=25 rssi=-57 crc_ok=True len=8 data=a40233008804f1eb
[HIT] ts=158815068us ch=25 rssi=-57 crc_ok=True len=8 data=a40233008804f1eb
[HIT] ts=158816187us ch=25 rssi=-57 crc_ok=True len=8 data=a40233008804f1eb
[HIT] ts=158817305us ch=25 rssi=-57 crc_ok=True len=8 data=a40233008804f1eb
[HIT] ts=158818422us ch=25 rssi=-57 crc_ok=True len=8 data=a40233008804f1eb
[HIT] ts=158819540us ch=25 rssi=-57 crc_ok=True len=8 data=a40233008804f1eb
[HIT] ts=158820658us ch=25 rssi=-57 crc_ok=True len=8 data=a40233008804f1eb
[HIT] ts=158821776us ch=25 rssi=-57 crc_ok=True len=8 data=a40233008804f1eb
[HIT] ts=158822895us ch=25 rssi=-57 crc_ok=True len=8 data=a40233008804f1eb
[HIT] ts=158824013us ch=25 rssi=-57 crc_ok=True len=8 data=a40233008804f1eb
[HIT] ts=158825131us ch=25 rssi=-57 crc_ok=True len=8 data=a40233008804f1eb
[HIT] ts=158826250us ch=25 rssi=-57 crc_ok=True len=8 data=a40233008804f1eb
[ OK ] marker_hits=30
[STEP] RX_STOP
[FAIL] Timeout waiting for response: Response timeout (ignored 9 unexpected response(s), last=0x90)
```

**Salida TX literal íntegra:**

```text
PS C:\Users\Support\Documents\ec-projects\catsniffer-feralrf\FeralRF> python .\python\examples\smoke_tx_continuous_phase1.py --port COM33 --phy 4 --channel 25 --power 0 --packet-hex a40233008804 --interval-us 0 --run-seconds 1 --tx-timeout 2
FeralRF TX Continuous Smoke Test (Phase 1)
===========================================
port=COM33 baudrate=921600 phy=4 channel=25 power=0 len=6 interval_us=0 run_seconds=1.0

[STEP] Connect + RADIO_INIT + GET_INFO
[ OK ] INFO firmware=1.0.0 capabilities=0x07 serial=464552414c524631
[STEP] SET_PHY + SET_CHANNEL + SET_POWER
[ OK ] Config ACK
[STEP] TX_CONTINUOUS
[ OK ] TX_CONTINUOUS ACK
[STEP] TX_STOP
[ OK ] TX_STOP ACK

[ OK ] TX CONTINUOUS SMOKE PASS
```

**Resultado E5:** **PASS-RF de repetición** (30 coincidencias ≥20), **pero PARTIAL del gate completo** debido al timeout de `RX_STOP`. En las 30 coincidencias, `data=a40233008804f1eb`, longitud 8, `crc_ok=True`, RSSI −57/−56 dBm. Los timestamps registrados por RX van de `158793830` a `158826250` µs, diferencia `32420` µs = **32.420 ms**. Son 29 separaciones entre 30 hits; la media observada es aproximadamente **1117.93 µs**. Esta cifra caracteriza el espaciado de **eventos recibidos**, no certifica la temporización interna TX, la tasa de emisión global ni la duración completa de 1 s. El campo `0x90` es sólo el último tipo de respuesta inesperada reportado por la herramienta; no se infiere su significado semántico sin revisar su definición en código/versiones.

### E6 — Control post-STOP de variante 2 y nueva inicialización RX

**Por qué:** buscar emisiones tardías con `a40233008804` después del ACK de STOP, y comprobar si la recepción podía iniciar/cerrar de nuevo tras el timeout anterior. No reiniciar ni reflashear.  
**Esperado:** cero hits en cinco segundos, secuencia de control RX con ACK.

**Comando (Terminal A; Terminal B inactiva):**

```powershell
python .\python\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 5 --marker-hex a40233008804 --match-mode contains --min-hits 0 --print-limit 40
```

**Salida literal:**

```text
PS C:\Users\Support\Documents\ec-projects\catsniffer-feralrf\FeralRF> python .\python\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 5 --marker-hex a40233008804 --match-mode contains --min-hits 0 --print-limit 40
FeralRF OTA RX Probe
====================
port=COM88 baudrate=921600 phy=4 channel=25 duration=5.0s min_hits=0 marker=a40233008804 match_mode=contains allow_crc_fail=False

[STEP] Connect + RADIO_INIT + GET_INFO
[ OK ] INFO firmware=1.0.0 capabilities=0x07 serial=464552414c524631
[STEP] SET_PHY + SET_CHANNEL
[ OK ] Config ACK
[STEP] RX_START
[ OK ] RX_START ACK
[STEP] Read packets
[ OK ] packets_total=3 crc_ok=3
[ OK ] marker_hits=0
[STEP] RX_STOP
[ OK ] RX_STOP ACK

[ OK ] RX PROBE PASS hits=0 min_hits=0
```

**Resultado E6:** **PASS** del control post-STOP. También se observa recuperación operativa del ciclo RX tras el timeout anterior, sin demostrar cuál fue la causa del timeout ni que haya existido un reset (ninguno fue solicitado en esta secuencia).

## 6. Matriz consolidada de resultados

| Evento | Intervalo | TX solicitada | Ventana RX | Total RX / CRC OK | Hits del marcador | ACK de TX | Cierre RX | Veredicto del evento |
| --- | ---: | ---: | ---: | ---: | ---: | --- | --- | --- |
| E1. Negativo previo | — | No TX | 5 s | 6 / 6 | 0 | N/A | ACK | **PASS** |
| E2. RX sin TX acreditada | 250000 µs previsto | No acreditada | 10 s | 19 / 19 | 0 | Sin evidencia | ACK | **INCONCLUSIVE para TX** (script FAIL umbral) |
| E3. CONTINUOUS coordinado | 250000 µs | 3 s | 10 s | 11 / 11 | **1** | CONTINUOUS ACK; STOP ACK | ACK | **PARTIAL** (script RX FAIL umbral) |
| E4. Post-STOP de E3 | — | TX detenida según ACK previo | 5 s | 9 / 9 | **0** | ACK previo | ACK | **PASS del control** |
| E5. CONTINUOUS coordinado | 0 µs | 1 s | 7 s | 38 / 38 | **30** | CONTINUOUS ACK; STOP ACK | **Timeout**, 9 respuestas inesperadas | **PASS-RF / PARTIAL global** |
| E6. Post-STOP de E5 | — | TX detenida según ACK previo | 5 s | 3 / 3 | **0** | ACK previo | ACK | **PASS del control** |

**Nota sobre denominadores:** `packets_total` incluye tráfico no coincidente; `crc_ok` indica validación informada por la herramienta para ese total, no que todos los paquetes procedan del transmisor bajo prueba. Los marcadores se diferenciaron para reducir atribución cruzada entre variantes. En E5 no se efectuó un **control negativo específico inmediatamente antes** con `a40233008804`; el negativo formal E1 pertenece al marcador de E3. Esta ausencia debe quedar visible en el expediente, aunque los 30 hits sincronizados con TX sean evidencia fuerte de recepción atribuible.

## 7. Análisis técnico, por afirmación y nivel de prueba

### 7.1 Repetición RF del modo CONTINUOUS

- **Observado [A]:** E3 produjo 1 trama coincidente válida; E5 produjo 30 tramas coincidentes válidas. La segunda variante demuestra actividad repetida durante una porción de la ventana RX.
- **Solicitado [C]:** E3 fijó `interval_us=250000`, TX durante 3 s; E5 fijó `interval_us=0`, TX durante 1 s. Ambos scripts recibieron `TX_CONTINUOUS ACK`.
- **No demostrado [F]:** número exacto de tramas enviadas, tasa TX media durante toda la duración, garantía de periodicidad a 250 ms, pérdidas RF y cronología entre TX y RX con reloj común.
- **Inferencia [E]:** la diferencia de 1 y 30 hits es compatible con comportamiento distinto según intervalo; no permite localizar si la causa está en scheduler RF, firmware, host o condiciones de observación.

### 7.2 Contenido recibido y alcance del CRC

- **E3 [A/B]:** `a401330088043df6`, longitud 8, CRC válido según RX; prefijo de 6 bytes coincide con la solicitud.
- **E5 [A/B]:** `a40233008804f1eb`, longitud 8 en 30 coincidencias, CRC válido según RX; prefijo coincide con la solicitud.
- **Límite:** los dos bytes finales son **compatibles con FCS IEEE 802.15.4**; su procedencia/tratamiento exacto por firmware y host no fue verificado en esta campaña. No se debe presentar la concatenación como contrato de payload documentado sin revisar el backend.

### 7.3 STOP: ACK frente a cese físico

- **Observado [C]:** `TX_STOP ACK` en E3 y E5.
- **Observado [A/B]:** cero coincidencias del marcador durante E4 y E6, cada uno por 5 s, con RX operativo.
- **Conclusión acotada:** no se encontró actividad OTA atribuible en esas ventanas posteriores, resultado **compatible** con el cese de emisiones.
- **No demostrado [F]:** latencia de STOP, ausencia de emisiones entre el ACK y el inicio posterior del control, ni que STOP sea la única causa del cese. Un apagado espontáneo anterior produciría también ventanas post-STOP sin hits.

### 7.4 Timeout de `RX_STOP` al final de E5

- **Hecho observado [B/C]:** `Response timeout (ignored 9 unexpected response(s), last=0x90)`. No llegó al host la respuesta esperada dentro del plazo de la herramienta; no se puede asegurar si el firmware procesó o no `RX_STOP`.
- **Contraprueba [B/C]:** E6 ejecutó `RADIO_INIT`, `GET_INFO`, `SET_PHY`, `SET_CHANNEL`, `RX_START`, lectura y `RX_STOP` con ACK, sin reinicio explícito entre ambas pruebas.
- **Conclusión:** anomalía **transitoria, reproducibilidad no medida**; no hay evidencia de bloqueo persistente. La interpretación del `0x90` queda pendiente de inspección de definiciones del protocolo, procesamiento de eventos asíncronos y filtros del cliente Python; no afirmar una causa raíz.

## 8. Conclusiones defendibles

**C-01 [A].** El conjunto transmisor/receptor demuestra que FeralRF es capaz de producir **múltiples recepciones OTA atribuibles** bajo `TX_CONTINUOUS` con `interval_us=0` y PHY IEEE 802.15.4: 30 observaciones con CRC válido y el marcador esperado.

**C-02 [A/C].** Con `interval_us=250000` se recibió al menos una trama correspondiente a TX, pero **no** se alcanzó el umbral de 5; no quedó validada la repetición a 250 ms. Resultado **PARTIAL**, no fallo de emisión demostrado.

**C-03 [C].** La interfaz de control aceptó `TX_CONTINUOUS` y `TX_STOP` en ambas variantes. Estos ACK no validan por sí solos comportamiento en antena.

**C-04 [A/B].** Los controles posteriores de 5 s no detectaron marcadores de las variantes, con recepción operativa; esto es **compatible con cese**, pero insuficiente para declarar «STOP físico e inmediato demostrado».

**C-05 [B/C].** Hubo un timeout de `RX_STOP` con nueve respuestas inesperadas tras la prueba de alta actividad. Un ciclo posterior funcionó normalmente; existe anomalía de control a investigar, sin bloqueo persistente observado.

**Veredicto final del bloque:** **PARTIAL / PASS-RF condicionado**. Cumplimos la demostración de repetición física en la variante de intervalo cero, pero no la caracterización completa de periodicidad, conteo y cese inmediato, y queda pendiente el diagnóstico del timeout del receptor. El cierre de este bloque experimental **no convierte** las preguntas pendientes en resultados validados.

## 9. Limitaciones y trabajo posterior propuesto (no ejecutado aquí)

1. **Periodicidad positiva:** revisar primero la implementación que procesa `TX_CONTINUOUS` (procesador de ejecución, controlador RF, interfaz interna, versión/commit exactos), su conversión de `interval_us` y el lifecycle de las operaciones. Después diseñar un ensayo dirigido con host/observador sincronizados y, si hace falta, instrumentación RF independiente.
2. **STOP real:** evaluar la transición TX→STOP con ventana RX continua antes y después del comando y referencia temporal externa si se exige tiempo de reacción; evitar pausas manuales entre ambas observaciones.
3. **`RX_STOP`/`0x90`:** localizar definición de respuesta `0x90`, cómo el cliente Python consume/filtra eventos asíncronos, límites de cola, ACK y tiempo de espera; repetir caso rápido con una matriz acotada antes de adjudicar bug.
4. **Trazabilidad:** registrar hash del binario de CC1352 instalado y su relación con el commit del repositorio, revisión PCB, antenas y firmware RP2040. Ninguna de esas asociaciones fue demostrada por los stdout aquí reunidos.
5. **Métricas:** conservar relojes y logs de ambas terminales con archivo de sesión, configuración RF física y entorno Python para trazabilidad intersesiones.

**No se proponen más repeticiones de BURST** como parte de este informe: esa operación pertenece a otra evaluación y no es condición para interpretar la evidencia CONTINUOUS aquí archivada.

## 10. Fuentes y custodia de evidencia

- **Fuente primaria S1 — Consola de usuario:** las seis fases E1–E6 transcritas literalmente en §5, incluyendo ambos comandos y stdout de las fases coordinadas E3/E5.
- **Fuente de procedimiento S2 — Archivo de proyecto:** `Guía enfocada OTA FeralRF - TX RX con dos CatSniffer.md`, §4, comandos de negativo, variantes y control posterior. La guía contiene también resultados de sesiones anteriores, **no fusionados numéricamente** con estas pruebas.
- **Fuente de criterios S3 — Archivo de proyecto:** `Checklist OTA FeralRF - dos CatSniffer.md`, §«CONTINUOUS + STOP».
- **Fuente de contexto S4 — Identificación del proyecto en la conversación:** rama/commit y versión SDK indicados en el registro de trabajo; no equivalen a provenance del binario instalado.

### 10.1 Regla de actualización

Si se reevalúa CONTINUOUS, **crear otra entrada de prueba fechada** con comandos y stdout propios. No reemplazar silenciosamente estos resultados ni mezclar hits de distintas campañas o variantes.
