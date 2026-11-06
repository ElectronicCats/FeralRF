# Procedimientos Sub-GHz y experimentos condicionados

## Alcance y estado

Esta guía permite configurar y probar los presets Sub-GHz históricamente relevantes sin deducir comandos desde la API. El operador todavía **no ha ejecutado esta revisión de 07**. Los flujos nuevos de dos terminales son reconstruidos aunque sus presets tengan resultados históricos positivos.

Distinciones obligatorias:

- un ACK acredita control, no emisión;
- un marcador CRC-good en el peer acredita entrega OTA host-visible;
- ninguna de esas observaciones mide frecuencia, potencia o modulación física;
- Wi-SUN, Sidewalk y Wireless M-Bus en un nombre no aportan sus tramas, MAC, seguridad o interoperabilidad;
- `mod_type=4/5/6` está reservado en el comando TI auditado: no demuestra MSK/4-FSK/4-GFSK.

La ejecución RF-03 del 21-oct se bloqueó en su etapa IEEE inicial; no aporta evidencia Sub-GHz nueva. Véase [[08 - Validación reciente del manual frente a evidencia histórica]].

## Preparación común obligatoria

### Directorio y entorno

En **cada terminal**:

```powershell
Set-Location 'C:\Users\Support\Documents\ec-projects\catsniffer-feralrf\FeralRF\python'
$env:PYTHONPATH = (Resolve-Path '.').Path
python --version
python -c "import feralrf; print(feralrf.__file__)"
```

La ruta impresa debe terminar en este checkout bajo `FeralRF\python\feralrf`. Puertos históricos:

```powershell
$TxBridge = 'COM33'; $TxShell = 'COM35'
$RxBridge = 'COM88'; $RxShell = 'COM87'
```

No calcule el Shell desde el Bridge. Identifique físicamente los pares mediante OP-01 de [[05 - Manual práctico de uso de FeralRF]].

### Cat-Shell, Cat-Bridge y banda

| Interfaz | Acción | Qué demuestra |
|---|---|---|
| Cat-Shell, 115200 | `band2`, `status`, `boot`, `exit` | Estado lógico/control RP2040 |
| Cat-Bridge, 921600 | PHY, preset, RX/TX | Operación aceptada por FeralRF |
| Peer RX | marcador con `crc_ok=True` | Entrega OTA entre CatSniffer |
| Instrumento | frecuencia/espectro/potencia | Magnitud física medida |

Antes de una prueba ordinaria seleccione y consulte Sub-GHz en ambos Shell:

```powershell
python -c "import serial,time; s=serial.Serial('COM35',115200,timeout=1); s.reset_input_buffer(); s.write(b'band2\r\n'); time.sleep(.3); print(s.read_all().decode(errors='replace')); s.write(b'status\r\n'); time.sleep(.5); print(s.read_all().decode(errors='replace')); s.close()"
python -c "import serial,time; s=serial.Serial('COM87',115200,timeout=1); s.reset_input_buffer(); s.write(b'band2\r\n'); time.sleep(.3); print(s.read_all().decode(errors='replace')); s.write(b'status\r\n'); time.sleep(.5); print(s.read_all().decode(errors='replace')); s.close()"
```

Con RP2040 `v3.1.0.1` se observó `SUB-GHz Band` y `Band: 1`. Esto no identifica 868/902/915 MHz ni verifica eléctricamente el switch. Los checkpoints 3D/3E/4A entregaron paquetes incluso con `Band: 0`; el mecanismo físico quedó inconcluso, pero una repetición ordinaria debe seguir seleccionando `band2`.

### Antena, parámetros y framing

- Use antena/carga compatible y cumpla regulación local.
- `wisun_915_fsk_*` solicita realmente `902200000` Hz.
- No sobrescriba parámetros individuales si pretende reproducir un preset histórico.
- La ruta API es `PHY.PROPRIETARY_GFSK` incluso si el diccionario pide FSK (`mod_type=0`).
- Los presets elegibles usan por defecto preámbulo 4 B, sync 32 bits, MSB-first, longitud variable, CRC del RF core y sin whitening/FEC.
- `rx_bw` y `deviation` son valores solicitados, no mediciones.

## Patrones operativos

| Patrón | Uso | Terminales | Procedencia |
|---|---|---:|---|
| A | Control de una placa | 1 | Script actual ejecutado históricamente |
| B | RX y TX independientes | 2 | **PROCEDIMIENTO RECONSTRUIDO — NO VALIDADO EN HARDWARE** |
| C | Script coordinado RF-02/helper archivado | 1 | Exacto histórico o equivalente preservado |

RX debe quedar listo antes de TX.

## OP-12 — Control de `gfsk_868_50k` en una placa

**VALIDADO POR CONTROL, no OTA.** EV-11 obtuvo ACK y cero paquetes locales.

Desde `FeralRF\python`:

```powershell
python .\examples\smoke_prop_phase1.py --port COM88 --preset gfsk_868_50k --power 0
$LASTEXITCODE
```

Histórico: `SET_PROP_CONFIG ACK`, `RX_START ACK`, `packets=0`, `RX_STOP ACK`, `TX_RAW ACK` y PASS. Exit 0 no demuestra 868 MHz, GFSK, potencia o entrega. No use `--auto-reset` en FSK/GFSK; para OOK su descubrimiento de Shell tampoco es seguro con COM88/COM87. Si `RX_STOP` falla, no continúe al TX/preset siguiente.

## OP-13 — Flujo OTA reutilizable de dos terminales

> [!warning]
> **PROCEDIMIENTO RECONSTRUIDO — NO VALIDADO EN HARDWARE.** Reproduce `init → set_phy → configure_prop → RX antes de TX → diez TX → stop`, pero estos bloques PowerShell no son los comandos históricos que produjeron los PASS.

Valores de referencia: TX COM33, RX COM88, 0 dBm solicitados, diez TX separados 100 ms, marcador de cuatro bytes, RX 12 s y umbral 10.

### Terminal 1 — receptor

```powershell
$env:FERALRF_PORT = 'COM88'
$env:FERALRF_PRESET = 'wisun_915_fsk_50k'
$env:FERALRF_MARKER = 'DEADBEEF'
$env:FERALRF_DURATION = '12'
$env:FERALRF_MIN_HITS = '10'
@'
import os
from feralrf import PHY, PROP_PRESETS, Radio, RxStreamError

port = os.environ["FERALRF_PORT"]
preset_name = os.environ["FERALRF_PRESET"]
marker = bytes.fromhex(os.environ["FERALRF_MARKER"])
duration = float(os.environ["FERALRF_DURATION"])
minimum = int(os.environ["FERALRF_MIN_HITS"])
if preset_name not in PROP_PRESETS:
    raise SystemExit(f"Unknown preset: {preset_name}")

radio = Radio(port=port, baudrate=921600)
rx_started = False
hits = packets = stream_errors = 0
cleanup_error = None
try:
    print("RX INIT:", radio.init())
    radio.set_phy(PHY.PROPRIETARY_GFSK, channel=0)
    radio.configure_prop(**PROP_PRESETS[preset_name])
    radio.start_rx(); rx_started = True
    print(f"RX READY preset={preset_name} marker={marker.hex().upper()}", flush=True)
    for item in radio.read_packets(timeout=duration):
        if isinstance(item, RxStreamError):
            stream_errors += 1
            print("RX ASYNC ERROR:", item, flush=True)
            continue
        packets += 1
        matched = item.crc_ok and marker in item.data
        hits += int(matched)
        print(f"RX PACKET crc_ok={item.crc_ok} marker={matched} data={item.data.hex()}", flush=True)
    radio.stop_rx(retries=1, timeout=2.0); rx_started = False
    print("RX_STOP ACK")
finally:
    if rx_started:
        try:
            radio.stop_rx(retries=1, timeout=2.0)
        except Exception as exc:
            cleanup_error = exc
            print("RX_STOP CLEANUP ERROR:", repr(exc))
    radio.disconnect()

print(f"RX RESULT packets={packets} hits={hits} errors={stream_errors}")
if cleanup_error is not None:
    raise cleanup_error
raise SystemExit(0 if hits >= minimum and stream_errors == 0 else 2)
'@ | python -
$LASTEXITCODE
```

Espere `RX READY` antes de TX. No use `channel`, RSSI o LQI propietarios como medidas calibradas.

### Terminal 2 — transmisor

Use el mismo preset y marcador:

```powershell
$env:FERALRF_PORT = 'COM33'
$env:FERALRF_PRESET = 'wisun_915_fsk_50k'
$env:FERALRF_MARKER = 'DEADBEEF'
$env:FERALRF_TX_COUNT = '10'
$env:FERALRF_POWER_DBM = '0'
@'
import os
import time
from feralrf import PHY, PROP_PRESETS, Radio

port = os.environ["FERALRF_PORT"]
preset_name = os.environ["FERALRF_PRESET"]
marker = bytes.fromhex(os.environ["FERALRF_MARKER"])
count = int(os.environ["FERALRF_TX_COUNT"])
power = int(os.environ["FERALRF_POWER_DBM"])
if preset_name not in PROP_PRESETS:
    raise SystemExit(f"Unknown preset: {preset_name}")

radio = Radio(port=port, baudrate=921600)
accepted = 0
try:
    print("TX INIT:", radio.init())
    radio.set_phy(PHY.PROPRIETARY_GFSK, channel=0)
    radio.configure_prop(**PROP_PRESETS[preset_name])
    radio.set_power(power)
    for index in range(1, count + 1):
        radio.transmit(marker, power_dbm=power)
        accepted += 1
        print(f"TX_RAW ACK {index}/{count}", flush=True)
        time.sleep(0.1)
finally:
    radio.disconnect()

print(f"TX RESULT accepted={accepted} requested={count}")
raise SystemExit(0 if accepted == count else 2)
'@ | python -
$LASTEXITCODE
```

### Criterio, cleanup y variaciones

- TX 10/10 ACK no significa diez emisiones físicas.
- RX 10 hits con CRC-good sustenta diez entregas host-visible.
- Ambos exits 0, `RX_STOP ACK` y procesos cerrados constituyen PASS de este procedimiento reconstruido.
- 1–9 hits: PARTIAL; 0 hits: FAIL de entrega; timeout/cleanup: BLOCKED.
- Use un marcador nuevo al repetir. Invertir roles es otra ejecución.
- Cambie preset y marker en **ambas** terminales usando sólo la tabla elegible. No cambie frecuencia/tasa/desviación por separado.

## OP-13C — Helper coordinado archivado

**EQUIVALENTE DEL PROCEDIMIENTO HISTÓRICO; no revalidado ahora.** Abre TX/RX en una terminal, fija TX=`COM33`, RX=`COM88`, diez llamadas y 0 dBm. Su `sys.path` archivado apunta a una ubicación antigua; la preparación mediante `PYTHONPATH` es necesaria.

```powershell
Test-Path '..\docs\OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-16\experimental_artifacts\manual_preset_runner.py'
python -u '..\docs\OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-16\experimental_artifacts\manual_preset_runner.py' wisun_915_fsk_50k DEADBEEF manual-replay-wisun-50k
$LASTEXITCODE
```

No acepta `--tx-port`, `--rx-port`, `--count` o `--power`. No lo use si los COM cambiaron. Salida histórica equivalente: diez `TX_ATTEMPT_ACK`, diez `RX_PACKET`, `RX_STOP_ACK`, `PASS-RF`, exit 0.

### Recuperación 3A–4A

| Checkpoint | Cambio | Resultado |
|---|---|---|
| 3A | Reset explícito + `band2` | 10/10 |
| 3A-R | Repetición inmediata | 10/10 |
| 3C | Sin nuevo reset, `Band: 1` | 10/10 |
| 3D | `band1`, `Band: 0` | 10/10 |
| 3E | Ventanas 3 s/5 s, sin reset/band | 10/10 |
| 4A | Power-cycle ≥15 s, `Band: 0` inicial | 10/10 |

El agregado 60/60 de `wisun_915_fsk_50k` no explica los 0/70 y 0/240 anteriores ni demuestra posición física de banda.

## Catálogo operacional

### Control + 13 configuraciones elegibles para OP-13

Todas tuvieron 10/10 raw #2→#3 en la campaña positiva; el control tuvo además 60/60 y RF-02 inverso 10/10. Los markers siguientes son etiquetas de campaña, no tramas de protocolo.

| Preset | Marker | Hz | mod | rate | dev | BW | sync | Evidencia |
|---|---|---:|---|---:|---:|---:|---|---|
| `wisun_915_fsk_50k` | `DEADBEEF` | 902200000 | 0 | 50000 | 100 | `52` | `930B51DE` | 60/60; inverso 10/10 |
| `gfsk_902_50k` | `A5010001` | 902200000 | 1 | 50000 | 100 | `52` | `930B51DE` | 10/10 |
| `wisun_915_fsk_100k` | `A5020001` | 902200000 | 0 | 100000 | 150 | `56` | `930B51DE` | 10/10 |
| `wisun_915_fsk_150k` | `A5030001` | 902200000 | 0 | 150000 | 200 | `5A` | `930B51DE` | 10/10 |
| `wisun_915_fsk_200k` | `A5040001` | 902200000 | 0 | 200000 | 200 | `5A` | `930B51DE` | 10/10 |
| `wisun_915_fsk_300k` | `A5050001` | 902200000 | 0 | 300000 | 200 | `5E` | `930B51DE` | 10/10 |
| `gfsk_915_50k` | `A5060001` | 915000000 | 1 | 50000 | 100 | `52` | `930B51DE` | 10/10 |
| `sidewalk_915_fsk_50k` | `A5070001` | 915000000 | 0 | 50000 | 100 | `52` | `930B51DE` | 10/10 raw |
| `sidewalk_915_fsk_250k` | `A5080001` | 915000000 | 0 | 250000 | 200 | `5A` | `930B51DE` | 10/10 raw |
| `gfsk_868_50k` | `A5090001` | 868000000 | 1 | 50000 | 100 | `52` | `930B51DE` | 10/10 |
| `gfsk_868_100k` | `A50A0001` | 868000000 | 1 | 100000 | 150 | `56` | `930B51DE` | 10/10 |
| `wireless_mbus_s_868` | `A50B0001` | 868300000 | 1 | 32768 | 75 | `52` | `543D0000` | 10/10 raw |
| `wireless_mbus_t_868` | `A50C0001` | 868950000 | 1 | 100000 | 200 | `57` | `543D0000` | 10/10 raw |
| `wireless_mbus_c_868` | `A50D0001` | 868950000 | 1 | 100000 | 180 | `57` | `543D0000` | 10/10 raw |

### Intentados, condicionados o excluidos

Cada fila reproduce la definición vigente de `PROP_PRESETS`; todos solicitan `PHY.PROPRIETARY_GFSK`. Salvo indicación contraria del firmware, comparten el framing descrito arriba: preámbulo 4 B, longitud variable, MSB-first, CRC del RF core, sin whitening/FEC y `format_conf=0`. `mod` es el entero solicitado, no una medición de modulación.

| Preset | Hz / mod / rate / dev / BW / sync | Cobertura histórica | Procedimiento/restricción actual |
|---|---|---|---|
| `gfsk_433_50k` | 433920000 / 1 / 50000 / 100 / `52` / `930B51DE` | Control; claim agregado GFSK 433 de 6–10/10, sin artefacto primario por preset | No OTA sin ruta y antena 433 verificadas |
| `gfsk_433_10k` | 433920000 / 1 / 10000 / 50 / `52` / `930B51DE` | Control; el claim agregado 6–10/10 no permite asignarle un conteo individual | No OTA sin ruta y antena 433 verificadas |
| `fsk_433_50k` | 433920000 / 0 / 50000 / 100 / `52` / `930B51DE` | Control; claim marginal 1/10 | No OTA sin ruta y antena 433 verificadas |
| `ook_433_4k8` | 433920000 / 2 / 4800 / 0 / `4C` / `930B51DE` | Control; claim familiar 0/10 | No ejecutar: OOK y hardware 433 no demostrados |
| `ook_433_2k4` | 433920000 / 2 / 2400 / 0 / `4C` / `930B51DE` | Control; sin conteo individual recuperable (claim familiar 0/10) | No ejecutar: OOK y hardware 433 no demostrados |
| `msk_433_50k` | 433920000 / 4 / 50000 / 100 / `52` / `930B51DE` | Control; claim 1/10 sin verificación física | `mod_type=4` reservado; no existe procedimiento OTA válido |
| `4fsk_433_50k` | 433920000 / 5 / 50000 / 100 / `52` / `930B51DE` | Control/claim sin verificación física | `mod_type=5` reservado; no existe procedimiento OTA válido |
| `4gfsk_433_50k` | 433920000 / 6 / 50000 / 100 / `52` / `930B51DE` | Control/claim sin verificación física | `mod_type=6` reservado; no existe procedimiento OTA válido |
| `ook_868_4k8` | 868000000 / 2 / 4800 / 0 / `4C` / `930B51DE` | Claim 10/10 sin logs; RF-05 NOT RUN | Sólo OP-16 gated; requiere power-cycle y control posterior |
| `msk_868_50k` | 868000000 / 4 / 50000 / 100 / `52` / `930B51DE` | Claim 10/10, sin verificación física ni artefacto primario completo | `mod_type=4` reservado; no existe procedimiento OTA válido |
| `4fsk_868_50k` | 868000000 / 5 / 50000 / 100 / `52` / `930B51DE` | Claim 10/10, sin verificación física ni artefacto primario completo | `mod_type=5` reservado; no existe procedimiento OTA válido |
| `4gfsk_868_50k` | 868000000 / 6 / 50000 / 100 / `52` / `930B51DE` | Claim 10/10, sin verificación física ni artefacto primario completo | `mod_type=6` reservado; no existe procedimiento OTA válido |
| `wireless_mbus_n_169_2k4` | 169450000 / 1 / 2400 / 154 / `44` / `543D0000` | Control solamente | No ejecutar: frecuencia, ruta y setup no defendibles |
| `wireless_mbus_n_169_4k8` | 169450000 / 1 / 4800 / 154 / `44` / `543D0000` | Control solamente | No ejecutar: frecuencia, ruta y setup no defendibles |
| `mioty_868_tsunb` | 868000000 / 0 / 396 / 1 / `4A` / `930B51DE` | 0/10; preset marcado pending | No ejecutar; no demuestra TS-UNB ni patch nativo |
| `gfsk_2440_50k` | 2440000000 / 1 / 50000 / 100 / `52` / `930B51DE` | Aceptación y 0 hits | No es Sub-GHz; fuera de esta guía |
| `gfsk_2440_250k` | 2440000000 / 1 / 250000 / 320 / `59` / `930B51DE` | Aceptación y 0 hits, incluso concurrente | No es Sub-GHz; fuera de esta guía |

Así se cubren los 31 presets actuales: 14 elegibles, OOK 868 condicionado, 14 sin OTA ordinaria defendible y dos proprietary 2.4 fuera de alcance.

### Procedencia de comandos históricos

- F29 Linux: `python examples/smoke_f29_subg_915.py --tx-port /dev/ttyACM1 --rx-port /dev/ttyACM2 --count 10`; claim 70/70 sin transcript/binarios suficientes. Deriva Shell por `Bridge+2`: **no ejecutarlo con el mapeo Windows actual**.
- E01–E24 usaron una función temporal `Invoke-FeralPresetOta`, no un CLI instalado. Ejemplos exactos: `gfsk_868_50k/b10133008801`, `gfsk_915_50k/b11133008801`, `gfsk_2440_250k/b32133008804`, `wireless_mbus_s_868/b50133008808`, `sidewalk_915_fsk_250k/b6213300880d`, `wisun_915_fsk_300k/b74133008812`. Acumulado: 240 ACK, cero paquetes.
- La recuperación posterior produjo 10/10 en las 13 variantes de la primera tabla, además del control 60/60. OP-13 es el equivalente actual reconstruido.

Se conservan 70/70, 0/70, 0/240 y la recuperación positiva. No se declara regresión: faltan condiciones/binarios equivalentes.

## OP-14 — RF-02 inverso #3→#2

**COMANDO HISTÓRICO EXACTO, PASS.** TX `COM88/87`; RX `COM33/35`; `wisun_915_fsk_50k`; marcador `C2028802`; resultado 10/10, `RX_STOP` ACK y exit 0.

Desde `FeralRF\python`, una terminal:

```powershell
$rfRunDir = Join-Path (Resolve-Path '..\validation_runs') ('manual_' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
New-Item -ItemType Directory -Path $rfRunDir | Out-Null
python ..\experimental_artifacts\next_rf_validation\preflight.py --tx-port COM88 --rx-port COM33 --tx-shell COM87 --rx-shell COM35 --antenna-confirmed --recovery-confirmed 2>&1 | Tee-Object -FilePath (Join-Path $rfRunDir '02a_preflight_rf02.jsonl')
$LASTEXITCODE
```

Sólo tras `RESULT PASS`/exit 0:

```powershell
python ..\experimental_artifacts\next_rf_validation\rf02_subghz_reverse.py `
    --tx-port COM88 `
    --rx-port COM33 `
    --tx-shell COM87 `
    --rx-shell COM35 `
    --marker-hex C2028802 `
    --min-received 9 `
    2>&1 | Tee-Object -FilePath (Join-Path $rfRunDir '02_rf02_subghz_reverse.jsonl')
$LASTEXITCODE
```

El script selecciona `band2`, inicia RX antes de TX y limpia ambos. PASS requiere peer y cleanup, no sólo ACK. Metadatos propietarios históricos fueron anómalos.

## Recuperación

Ante timeout, `RxStreamError`, `RX_STOP` no confirmado o pérdida de COM:

1. No lance otro preset; conserve stdout/JSONL, comando, marker, roles y exit.
2. Cierre procesos y libere Bridge/Shell.
3. Sólo si el runbook/caso lo exige, retire USB y alimentación externa ≥15 s.
4. Reenumere, ejecute OP-01 y un preflight fresco.
5. Ejecute un único control conocido; si bloquea, detenga la campaña.

`boot`→`exit`, `band2` y power-cycle no son equivalentes ni universalmente obligatorios. 4A obtuvo PASS tras cold start sin reset ni comando de banda.

## OP-15 — KillerBee RF-04

RF-04 es IEEE 802.15.4, no Sub-GHz. Se conserva aquí sólo para mantener OP-15; operativamente pertenece a [[06 - Procedimientos IEEE, TX y modos de prueba]].

**BLOCKED antes de hardware.** Desde el directorio normalizado, el equivalente de instalación sería `python -m pip install -e '.[killerbee]'`, pero no se recomienda ni ejecuta: históricamente pip no encontró `killerbee>=3.0.0`, no se abrió hardware y no hubo PCAP.

## OP-16 — OOK 868 aislado RF-05

**CONDICIONADO — NO EJECUTADO.** No pertenece al loop FSK/GFSK. Requiere antenas 868, power-cycle disponible, aprobación inmediata y gates previos.

Desde `FeralRF\python`, sólo en campaña autorizada:

```powershell
$rfRunDir = Join-Path (Resolve-Path '..\validation_runs') ('manual_' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
New-Item -ItemType Directory -Path $rfRunDir | Out-Null
python ..\experimental_artifacts\next_rf_validation\preflight.py --tx-port COM33 --rx-port COM88 --tx-shell COM35 --rx-shell COM87 --antenna-confirmed --recovery-confirmed 2>&1 | Tee-Object -FilePath (Join-Path $rfRunDir '05a_preflight_rf05.jsonl')
$LASTEXITCODE
```

Tras power-cycle inicial, preflight PASS y aprobación:

```powershell
python ..\experimental_artifacts\next_rf_validation\rf05_ook_868.py --tx-port COM33 --rx-port COM88 --tx-shell COM35 --rx-shell COM87 --marker-hex C5050001 --min-received 9 --operator-approved --initial-power-cycle-confirmed 2>&1 | Tee-Object -FilePath (Join-Path $rfRunDir '05_rf05_ook_868.jsonl')
$LASTEXITCODE
```

Después, sin otro comando RF: power-cycle ≥15 s, reenumeración, preflight y único control IEEE:

```powershell
python ..\experimental_artifacts\next_rf_validation\rf01_ieee_channel_switch.py --only-control --tx-port COM33 --rx-port COM88 --tx-shell COM35 --rx-shell COM87 --min-received 9 2>&1 | Tee-Object -FilePath (Join-Path $rfRunDir '05b_rf05_recovery_ieee_control.jsonl')
$LASTEXITCODE
```

No existe stdout RF-05. Ni siquiera una etapa OTA satisfactoria sería validación física OOK o recuperación sin el control posterior.

## Fuentes

- [[../Pruebas OTA SubGHz/Experimentos FeralRF OTA SubGHz]]
- [[../EXPERIMENT_REPORT_2026-10-20/RF-02 — Validación OTA Sub-GHz inversa]]
- `FeralRF/docs/OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-12/experiments/03_proprietary_gfsk_868_915.md`
- `FeralRF/docs/OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-12/experiments/04_proprietary_extended_campaign.md`
- `FeralRF/docs/OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-16/SUBGHZ_RECOVERY_RUN.md`
- `FeralRF/docs/OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-16/SUBGHZ_PROVENANCE_AUDIT.md`
- `FeralRF/python/feralrf/{presets.py,radio.py}`, HEAD `fbff1b7f5077964418fb63a3fdf346a27c35a003`
- [[../../04 - FeralRF/01 - Auditoría de completitud funcional y protocolos/03 - Matriz de capacidades de protocolos y presets|auditoría II-B de presets]]
