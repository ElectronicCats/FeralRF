# Procedimientos propietarios Sub-GHz

Estos procedimientos separan configuración, control, entrega raw y propiedad física. Un preset llamado Wi-SUN, Sidewalk o Wireless M-Bus no implementa ese protocolo. Consulte la [matriz canónica de presets](../../CURRENT_VALIDATION_MATRIX.md#6-matriz-de-presets-y-familias-protocolarias).

Antes de ejecutar: complete [OP-01/02](common.md), seleccione `band2` por cada Shell explícito, consulte `status`, cierre los Shell y use antenas/cargas apropiadas. `Band: 1`, frecuencia solicitada, ACK o marker peer no miden switch, frecuencia, potencia ni modulación.

<a id="op-12"></a>
## OP-12 — Control del preset `gfsk_868_50k`

**Estado:** `CURRENT_VALIDATED`
**Objetivo:** ejercer en una placa la configuración/control de un preset conocido.
**Capacidad relacionada:** [`configure_prop()` y P09](../../CURRENT_VALIDATION_MATRIX.md#6-matriz-de-presets-y-familias-protocolarias).
**Demuestra:** aceptación de `SET_PHY`, `SET_PROP_CONFIG`, RX start/stop y TX_RAW en esa sesión.
**No demuestra:** entrega OTA, GFSK físico, 868 MHz efectivo, potencia ni interoperabilidad.

**Hardware/software:** una CatSniffer, band2, [`smoke_prop_phase1.py`](../../../python/examples/smoke_prop_phase1.py).
**Precondiciones:** estado limpio, Bridge/Shell verificados, Shell cerrado; no haber entrado en OOK.
**Entrada/working directory:** `examples/smoke_prop_phase1.py`, `FeralRF/python`.

```powershell
python .\examples\smoke_prop_phase1.py --port COM_DUT --preset gfsk_868_50k --power 0
$LASTEXITCODE
```

**Parámetros/procedimiento:** sustituya COM; no use `--auto-reset`; no cambie valores del preset si pretende reproducir este control.
**Observaciones esperadas:** ACK de config/RX stop/TX, stats y exit 0. Cero paquetes locales es compatible con este control.
**Aceptación:** `PASS` sólo del camino de control requiere todos los ACK y cleanup; no se exige peer. `BLOCKED` si `RX_STOP` falla.
**Cleanup/recovery:** el helper desconecta; ante stop fallido detenga la campaña y aplique [recovery general](common.md#cleanup-y-recovery-general).
**Evidencia:** preset exacto, parámetros impresos, COM, ACK, stats, exit y cleanup.
**Limitaciones:** el helper transmite un payload raw después de cerrar RX, pero sin peer no acredita entrega ni propiedades físicas.
**Historia:** EV-11 y [`User-Manual/07`, OP-12](<../../User-Manual/07 - Procedimientos Sub-GHz y experimentos condicionados.md>).

<a id="op-13"></a>
## OP-13 — OTA Sub-GHz reutilizable

**Estado:** `RECONSTRUCTED_UNVALIDATED`
**Objetivo:** configurar el mismo preset en dos unidades, dejar RX listo, emitir diez markers raw y evaluar entrega/cleanup.
**Capacidad relacionada:** [entrega raw propietaria A→B](../../CURRENT_VALIDATION_MATRIX.md#53-propietaria-sub-ghz-y-modos-tx).
**Demuestra si se revalida:** ACK de configuración/TX y entrega raw host-visible para el preset exacto.
**No demuestra:** frecuencia, modulación, potencia, timing, posición del switch, fiabilidad universal ni el protocolo sugerido por el nombre.

**Hardware/software:** dos CatSniffer, band2, API pública `Radio`, `PROP_PRESETS` y `RxStreamError`; no existe hoy un helper OTA Sub-GHz current dedicado.
**Precondiciones:** OP-12 en ambas unidades, estado limpio, mismo preset/marker, RX antes de TX, marker nuevo. El bloque no fue reejecutado en hardware.
**Entrada/working directory:** dos bloques inline desde `FeralRF/python`.

**Terminal 1 — RX:**

```powershell
$env:FERALRF_PORT = 'COM_RX'
$env:FERALRF_PRESET = 'gfsk_868_50k'
$env:FERALRF_MARKER = 'A51300000101'
@'
import os
from feralrf import PHY, PROP_PRESETS, Radio, RxStreamError
port = os.environ["FERALRF_PORT"]
name = os.environ["FERALRF_PRESET"]
marker = bytes.fromhex(os.environ["FERALRF_MARKER"])
if name not in PROP_PRESETS: raise SystemExit(f"Unknown preset: {name}")
r = Radio(port=port); active = False; hits = packets = errors = 0; cleanup = None
try:
    print("RX INIT:", r.init())
    r.set_phy(PHY.PROPRIETARY_GFSK, channel=0)
    r.configure_prop(**PROP_PRESETS[name])
    r.start_rx(); active = True
    print(f"RX READY preset={name} marker={marker.hex()}", flush=True)
    for item in r.read_packets(timeout=12):
        if isinstance(item, RxStreamError):
            errors += 1; print("RX ASYNC ERROR:", item); continue
        packets += 1
        matched = marker in item.data
        hits += int(matched)
        print(
            f"RX PACKET reported_crc_ok={item.crc_ok} "
            f"marker={matched} data={item.data.hex()}"
        )
    r.stop_rx(retries=1, timeout=2.0); active = False; print("RX_STOP ACK")
finally:
    if active:
        try: r.stop_rx(retries=1, timeout=2.0)
        except Exception as exc: cleanup = exc; print("RX_STOP CLEANUP ERROR:", repr(exc))
    r.disconnect()
print(f"RX RESULT packets={packets} hits={hits} errors={errors}")
if cleanup is not None: raise cleanup
if errors > 0: raise SystemExit(3)
raise SystemExit(0 if hits >= 10 else 2)
'@ | python -
$LASTEXITCODE
```

**Terminal 2 — TX, sólo después de `RX READY`:**

```powershell
$env:FERALRF_PORT = 'COM_TX'
$env:FERALRF_PRESET = 'gfsk_868_50k'
$env:FERALRF_MARKER = 'A51300000101'
@'
import os, time
from feralrf import PHY, PROP_PRESETS, Radio
port = os.environ["FERALRF_PORT"]
name = os.environ["FERALRF_PRESET"]
marker = bytes.fromhex(os.environ["FERALRF_MARKER"])
if name not in PROP_PRESETS: raise SystemExit(f"Unknown preset: {name}")
r = Radio(port=port); accepted = 0
try:
    print("TX INIT:", r.init())
    r.set_phy(PHY.PROPRIETARY_GFSK, channel=0)
    r.configure_prop(**PROP_PRESETS[name]); r.set_power(0)
    for index in range(1, 11):
        r.transmit(marker, power_dbm=0); accepted += 1
        print(f"TX_RAW ACK {index}/10"); time.sleep(.1)
finally:
    r.disconnect()
print(f"TX RESULT accepted={accepted} requested=10")
raise SystemExit(0 if accepted == 10 else 2)
'@ | python -
$LASTEXITCODE
```

**Parámetros/procedimiento:** use el mismo nombre y marker en ambas terminales. El conjunto con evidencia raw positiva incluye `gfsk_868_50k`, `gfsk_868_100k`, `gfsk_902_50k`, `gfsk_915_50k`, `wisun_915_fsk_50k/100k/150k/200k/300k`, `sidewalk_915_fsk_50k/250k` y `wireless_mbus_s/t/c_868`; sus nombres sólo describen presets raw. Cambiar de preset es otra ejecución.
**Observaciones esperadas:** diez ACK TX, hits detectados por contenido del payload, resumen y `RX_STOP ACK`. `reported_crc_ok` se conserva sólo como metadata diagnóstica reportada por el parser propietario; no participa en el conteo ni en la aceptación.
**Aceptación:** tras revalidación: diez marker hits basados en payload + solapamiento válido + ambos lados completos + cleanup = `PASS` del criterio de entrega raw; 1–9 hits = `PARTIAL`; cero hits con ventana solapada válida = `FAIL` de entrega raw; error de setup, evento asíncrono o fallo de cleanup = `BLOCKED`. Hasta entonces el estado sigue `RECONSTRUCTED_UNVALIDATED`.
**Cleanup/recovery:** ambos objetos desconectan; ante timeout/async error preserve salida y no lance otro preset antes del recovery.
**Evidencia:** preset dict exacto, marker con secuencia si es posible, roles, transcripts, hits/ACK, exits y cleanup.
**Limitaciones:** `crc_ok`, RSSI, LQI, timestamp y channel propietarios son metadata no autoritativa para este procedimiento. Un marker hallado en el payload sólo demuestra entrega raw host-visible; no demuestra corrección física del CRC, modulación, frecuencia, potencia, timing ni semántica protocolaria. Estos bloques no son los comandos históricos que produjeron los positivos y ningún reporte reciente revalidó esta reconstrucción.
**Historia:** [`User-Manual/07`, OP-13](<../../User-Manual/07 - Procedimientos Sub-GHz y experimentos condicionados.md>) y [contradicciones Sub-GHz](../../CURRENT_VALIDATION_MATRIX.md#7-contradicciones-preservadas).

<a id="op-13c"></a>
## OP-13C — Helper coordinado archivado

**Estado:** `HISTORICAL_ONLY`
**Objetivo:** localizar el equivalente histórico coordinado, no recomendar su ejecución.
**Capacidad relacionada:** [entrega raw propietaria](../../CURRENT_VALIDATION_MATRIX.md#53-propietaria-sub-ghz-y-modos-tx).
**Demuestra:** únicamente procedencia de campañas anteriores.
**No demuestra:** compatibilidad actual o validez de puertos/entorno.

**Hardware/software:** el archivo preservado [`manual_preset_runner.py`](<../../HARDWARE_VALIDATION_RESULTS/OTA-SUBGHZ-EXPERIMENT_REPORT_2026-10-16/experimental_artifacts/manual_preset_runner.py>) existe dentro de evidencia histórica.
**Precondiciones/entrada/working directory/comando:** no existe entrada current. El helper tiene puertos hard-coded y `sys.path` legado; **no lo ejecute como runbook canónico**.
**Procedimiento/observaciones/aceptación:** consulte el archivo sólo para trazabilidad de `TX_ATTEMPT_ACK`, `RX_PACKET`, `RX_STOP_ACK` y `PASS-RF`; no asigne un resultado nuevo.
**Cleanup/recovery:** no aplica a consulta estática.
**Evidencia:** path/hash del artefacto y campaña original.
**Limitaciones:** no parametriza puertos, conteo o potencia; pertenece a evidencia inmutable.
**Historia:** [`User-Manual/07`, OP-13C](<../../User-Manual/07 - Procedimientos Sub-GHz y experimentos condicionados.md>).

<a id="op-14"></a>
## OP-14 — RF-02 Sub-GHz en dirección inversa

**Estado:** `BLOCKED`
**Objetivo:** reproducir B→A con `wisun_915_fsk_50k` raw y cleanup coordinado.
**Capacidad relacionada:** [entrega raw Sub-GHz B→A](../../CURRENT_VALIDATION_MATRIX.md#53-propietaria-sub-ghz-y-modos-tx).
**Demuestra históricamente:** 10/10 para parámetros solicitados ~902,2 MHz en RF-02.
**No demuestra:** que la receta sea hoy ejecutable, 902,2 MHz físico, Wi-SUN o fiabilidad universal.

**Hardware/software:** dos CatSniffer; se requieren `preflight.py` y `rf02_subghz_reverse.py`, ambos ausentes.
**Precondiciones:** restaurar/revisar tooling sin sustituirlo por otro helper; mapping y recovery verificados.
**Entrada/working directory/comando:** **ninguno actual**. El comando histórico está en [`User-Manual/07`, OP-14](<../../User-Manual/07 - Procedimientos Sub-GHz y experimentos condicionados.md>) y no debe ejecutarse porque su path no existe.

**Parámetros/procedimiento esperado:** TX y RX invertidos, preset exacto, marker nuevo, preflight, RX antes de TX y cleanup de ambas unidades.
**Observaciones/aceptación:** un futuro PASS exige peer + cleanup, no sólo ACK. Mientras falten runners es `BLOCKED`, aunque la capacidad permanezca demostrada en la matriz.
**Cleanup/recovery:** power-cycle/control fresco según el runner restaurado.
**Evidencia:** versión/hash de runners, JSONL, roles, marker, exits y cleanup.
**Limitaciones:** el éxito histórico no convierte una entrada ausente en procedimiento current.
**Historia:** RF-02, 2026-10-20, inventariado en [`User-Manual/02`](<../../User-Manual/02 - Matriz histórica de experimentos y ejecuciones.md>).

## Recovery Sub-GHz

Ante timeout, `RxStreamError`, `RX_STOP` no confirmado o pérdida de COM:

1. Detenga la secuencia y preserve stdout, comando, preset, marker, roles y exit.
2. Cierre procesos y libere Bridge/Shell.
3. No asuma que `boot`→`exit`, `band2` y power-cycle son equivalentes.
4. Si el procedimiento lo exige, retire USB y alimentación externa ≥15 s.
5. Reenumere, ejecute OP-01/02 y un único control OP-12. Si vuelve a bloquearse, detenga la campaña.

Las campañas 70/70, 0/70, 0/240 y recuperaciones positivas se conservan como contradicción abierta; no establecen ni fiabilidad universal ni regresión permanente.

<a id="op-16"></a>
## OP-16 — OOK 868 aislado

**Estado:** `BLOCKED`
**Objetivo:** preservar las condiciones mínimas para una futura validación OOK aislada sin presentarla como receta utilizable hoy.
**Capacidad relacionada:** [OOK raw](../../CURRENT_VALIDATION_MATRIX.md#55-integraciones-protocolos-nombrados-y-funciones-experimentales).
**Demuestra actualmente:** sólo que la configuración/patch path existe parcialmente y que CP7 quedó bloqueado.
**No demuestra:** lifecycle robusto, OOK físico, frecuencia/potencia, entrega actual o recovery mediante API.

> [!CAUTION]
> `mod_type=2` carga patches genOOK. Para usar otro modo de forma segura el backend requiere reset completo del CC1352, power-cycle o reflash; `RADIO_INIT` no basta. La recuperación demostrada fue power-cycle físico. `Radio.reset_device()` no es universalmente seguro porque infiere Shell como Bridge+2.

**Hardware/software:** dos unidades con antenas/cargas 868, capacidad de cortar toda alimentación y aprobación inmediata. El runner `rf05_ook_868.py` y su `preflight.py` están ausentes.
**Precondiciones:** tooling restaurado/revisado, mapping Shell verificado, power-cycle antes/después, OOK como última operación y control posterior definido.
**Entrada/working directory/comando:** **ninguno actual ejecutable**. No use `smoke_prop_phase1.py --auto-reset` ni invente un sustituto. El comando histórico gated nunca se ejecutó y se conserva en [`User-Manual/07`, OP-16](<../../User-Manual/07 - Procedimientos Sub-GHz y experimentos condicionados.md>).

**Parámetros/procedimiento futuro:** power-cycle inicial; preflight; aprobación; runner aislado; sin otro comando RF; power-cycle ≥15 s; reenumeración; preflight/control IEEE único.
**Observaciones/aceptación:** una etapa OTA satisfactoria seguiría sin validar OOK físico. PASS operacional exigiría entrega, cleanup, power-cycle y control posterior; cualquier gate faltante es `BLOCKED`.
**Cleanup/recovery:** no confíe en `RADIO_INIT` o `reset_device()` sin mapping; corte alimentación completa y no reutilice la sesión.
**Evidencia:** aprobaciones, identities, power-cycle antes/después, runner/hash, raw, cleanup y control posterior.
**Limitaciones:** CP7 obtuvo primer ACK, luego timeouts/errores, 0/10 y cleanup fallido; CP8 sólo demostró recuperación posterior de FSK/GFSK mediante power-cycle.
**Historia:** [contradicción OOK](../../CURRENT_VALIDATION_MATRIX.md#7-contradicciones-preservadas) y [`Presets_Reports/EXPERIMENTAL_EVIDENCE.md`](../../Presets_Reports/EXPERIMENTAL_EVIDENCE.md).
