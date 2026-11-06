# Procedimientos IEEE 802.15.4 y modos TX

Ejecute primero la [preparación común](common.md), OP-01 y OP-02. Seleccione `band1` en los Shell explícitos, cierre esos puertos y confirme antenas/cargas de 2,4 GHz. Todos los comandos parten de `FeralRF/python`.

<a id="op-04"></a>
## OP-04 — RX IEEE en canal 25

**Estado:** `CURRENT_VALIDATED`
**Objetivo:** configurar IEEE 802.15.4 canal 25, observar 30 s y cerrar RX.
**Capacidad relacionada:** [IEEE RX canal 25](../../CURRENT_VALIDATION_MATRIX.md#52-phy-ble-e-ieee-802154).
**Demuestra:** ruta de control y recepción host-visible bajo el tráfico presente.
**No demuestra:** interoperabilidad Zigbee/Thread, atribución del emisor, sensibilidad ni propiedades RF físicas.

**Hardware/software:** una CatSniffer, ruta 2,4 GHz, antena, Bridge identificado y [`smoke_phy4_ieee154.py`](../../../python/examples/smoke_phy4_ieee154.py).
**Precondiciones:** estado limpio, OP-02, Shell cerrado y fuente de tráfico autorizada si se pretende observar paquetes.
**Entrada/working directory:** `examples/smoke_phy4_ieee154.py`, `FeralRF/python`.

```powershell
python .\examples\smoke_phy4_ieee154.py --port COM_RX --channel 25 --duration 30
$LASTEXITCODE
```

**Parámetros/procedimiento:** sustituya `COM_RX`; deje terminar la ventana. El helper configura PHY/canal, inicia RX, lee, ejecuta `stop_rx()` y desconecta.
**Observaciones esperadas:** ACK de info/config/start/stop; en la reproducción de 2026-10-21 se observaron 54 paquetes y el primero reportó `crc_ok=True`.
**Aceptación:** `PASS` de recepción requiere al menos un paquete esperado y cleanup ACK; exit 0 del smoke sin paquetes sólo valida control. `BLOCKED` si stop/cleanup falla.
**Cleanup/recovery:** exija `RX_STOP ACK`; ante timeout conserve salida y aplique [recovery general](common.md#cleanup-y-recovery-general).
**Evidencia:** salida completa, conteo, primer paquete, fuente/condición de tráfico, COM, exit y cleanup.
**Limitaciones:** metadata RX no es instrumentación física; `RX_STOP` ha fallado en otras sesiones.
**Historia:** [`User-Manual/06`, OP-04](<../../User-Manual/06 - Procedimientos IEEE, TX y modos de prueba.md>) y [`User-Manual/08`](<../../User-Manual/08 - Validación reciente del manual frente a evidencia histórica.md>).

<a id="op-05"></a>
## OP-05 — Exclusión TX durante RX

**Estado:** `CURRENT_VALIDATED`
**Objetivo:** confirmar que TX durante RX activo se rechaza con error `0x05` y que RX puede cerrarse.
**Capacidad relacionada:** [sesión/RX y reglas de estado](../../CURRENT_VALIDATION_MATRIX.md#52-phy-ble-e-ieee-802154).
**Demuestra:** exclusión de estados/control para IEEE en esa sesión.
**No demuestra:** emisión RF; el TX esperado no debe aceptarse.

**Hardware/software:** una CatSniffer; `Radio`, `PHY` y `CommandError` actuales.
**Precondiciones:** Bridge idle, band1 seleccionado, ningún peer necesario.
**Entrada/working directory:** API pública inline, `FeralRF/python`.

```powershell
@'
from feralrf import PHY, Radio
from feralrf.exceptions import CommandError

r = Radio(port="COM_DUT")
rx_started = False
try:
    print("INIT:", r.init())
    r.set_phy(PHY.IEEE_802_15_4, channel=25)
    r.start_rx(); rx_started = True
    try:
        r.transmit(b"\x01", power_dbm=-20)
        raise RuntimeError("TX aceptado durante RX")
    except CommandError as exc:
        print(f"TX REJECTED: 0x{exc.error_code:02X}")
        if exc.error_code != 0x05:
            raise
    r.stop_rx(); rx_started = False
    print("RX_STOP ACK")
    print("STATS:", r.get_stats())
finally:
    cleanup_error = None
    if rx_started:
        try: r.stop_rx()
        except Exception as exc: cleanup_error = exc
    r.disconnect()
    if cleanup_error is not None: raise cleanup_error
'@ | python -
$LASTEXITCODE
```

**Parámetros/procedimiento:** sustituya `COM_DUT`; no altere el código esperado `0x05`.
**Observaciones esperadas:** `TX REJECTED: 0x05`, `RX_STOP ACK` y stats.
**Aceptación:** `PASS` sólo con rechazo `0x05` y cleanup; otro código o TX aceptado es `FAIL`; cleanup no confirmado es `BLOCKED`.
**Cleanup/recovery:** el `finally` desconecta y propaga fallo de stop; no continúe si ocurre.
**Evidencia:** transcript, error code, stats, exit y cleanup.
**Limitaciones:** sólo cubre el caso IEEE/estado ensayado, no todos los PHY o concurrencia multiproceso.
**Historia:** EV-06 y [`User-Manual/08`, OP-05](<../../User-Manual/08 - Validación reciente del manual frente a evidencia histórica.md>).

<a id="op-06"></a>
## OP-06 — `TX_RAW` IEEE con peer

**Estado:** `CURRENT_VALIDATED`
**Objetivo:** observar un marker `TX_RAW` en otra CatSniffer.
**Capacidad relacionada:** [IEEE `TX_RAW`](../../CURRENT_VALIDATION_MATRIX.md#52-phy-ble-e-ieee-802154).
**Demuestra:** ACK TX y entrega OTA host-visible entre dos unidades.
**No demuestra:** PDR real si se repite el mismo marker, stack IEEE/Zigbee/Thread ni potencia/frecuencia.

**Hardware/software:** dos CatSniffer con band1, [`ota_rx_probe.py`](../../../python/examples/lab/ota_rx_probe.py) y [`smoke_tx_phase1.py`](../../../python/examples/smoke_tx_phase1.py).
**Precondiciones:** OP-01/02 en ambas, roles RX/TX explícitos, marker nuevo, Shell cerrados.
**Entrada/working directory:** dos terminales, `FeralRF/python`.

**Comandos y procedimiento:** primero valide un control negativo y compruebe literalmente `marker_hits=0`:

```powershell
python .\examples\lab\ota_rx_probe.py --port COM_RX --phy 4 --channel 25 --duration 5 --marker-hex A10100000101 --match-mode contains --min-hits 0 --print-limit 30
```

Después inicie RX en Terminal 1:

```powershell
python .\examples\lab\ota_rx_probe.py --port COM_RX --phy 4 --channel 25 --duration 90 --marker-hex A10100000101 --match-mode contains --min-hits 10 --print-limit 80
```

Tras `RX_START ACK`, Terminal 2:

```powershell
1..10 | ForEach-Object {
    python .\examples\smoke_tx_phase1.py --port COM_TX --phy 4 --channel 25 --power 0 --packet-hex A10100000101 --tx-timeout 2
    if ($LASTEXITCODE -ne 0) { throw "TX_RAW fallo en $_" }
}
```

**Parámetros:** sustituya COM y marker; para PDR por solicitud use diez markers con contador único en una futura repetición.
**Observaciones esperadas:** diez `TX_RAW ACK`; el peer reporta hits CRC-good y `RX_STOP ACK`. Evidencia reciente: 9/10 y luego 10/10.
**Aceptación:** `PASS` = umbral 10 y cleanup; 1–9 = `PARTIAL`; 0 tras solapamiento confirmado = `FAIL` del criterio de entrega; stop/estado impide concluir = `BLOCKED`.
**Cleanup/recovery:** espere el resumen RX y exits; ante stop fallido no repita en la sesión.
**Evidencia:** ambos transcripts, marker/orden, ventana, conteos y cleanup.
**Limitaciones:** hits idénticos no correlacionan cada solicitud; FCS añadido no vuelve el payload una trama de stack.
**Historia:** [`User-Manual/06`, OP-06](<../../User-Manual/06 - Procedimientos IEEE, TX y modos de prueba.md>) y [`User-Manual/08`](<../../User-Manual/08 - Validación reciente del manual frente a evidencia histórica.md>).

<a id="op-07"></a>
## OP-07 — `TX_FRAME` IEEE con peer

**Estado:** `RECONSTRUCTED_UNVALIDATED`
**Objetivo:** enviar diez payloads mediante `TX_FRAME` y buscarlos en el peer.
**Capacidad relacionada:** [IEEE `TX_FRAME`](../../CURRENT_VALIDATION_MATRIX.md#52-phy-ble-e-ieee-802154).
**Demuestra si se revalida:** control `TX_FRAME` y entrega del payload/FCS.
**No demuestra:** MAC completo ni interoperabilidad; el backend comparte camino RAW.

**Hardware/software:** dos CatSniffer, [`ota_rx_probe.py`](../../../python/examples/lab/ota_rx_probe.py) y [`ota_tx_frame.py`](../../../python/examples/lab/ota_tx_frame.py).
**Precondiciones:** las de OP-06; marker nuevo; esta secuencia reconstruida no debe citarse como ejecución previa.
**Entrada/working directory:** dos terminales, `FeralRF/python`.

```powershell
# Terminal 1
python .\examples\lab\ota_rx_probe.py --port COM_RX --phy 4 --channel 25 --duration 45 --marker-hex A20100000201 --match-mode contains --min-hits 10 --print-limit 80

# Terminal 2, sólo tras RX_START ACK
python .\examples\lab\ota_tx_frame.py --port COM_TX --phy 4 --channel 25 --power 0 --payload-hex A20100000201 --count 10 --interval-ms 250 --tx-timeout 2 --progress-every 1
```

**Parámetros/procedimiento:** añada antes el control negativo de OP-06 con el marker nuevo; preserve ambos exits.
**Observaciones esperadas:** progreso `sent=n/10`, diez hits y cleanup.
**Aceptación:** al ejecutarse, use las reglas de OP-06; hasta entonces el procedimiento sigue `RECONSTRUCTED_UNVALIDATED`.
**Cleanup/recovery:** ambos helpers desconectan; timeout de stop bloquea continuidad.
**Evidencia:** comandos exactos, marker, transcripts, exit y cleanup.
**Limitaciones:** el histórico 10/10 no conservó el comando literal y esta composición no fue reejecutada.
**Historia:** [`User-Manual/06`, OP-07](<../../User-Manual/06 - Procedimientos IEEE, TX y modos de prueba.md>).

<a id="op-08"></a>
## OP-08 — `TX_BURST` con intervalo positivo

**Estado:** `CURRENT_PARTIAL`
**Objetivo:** solicitar cinco transmisiones a 250000 µs y medir hits peer.
**Capacidad relacionada:** [IEEE `TX_BURST`](../../CURRENT_VALIDATION_MATRIX.md#52-phy-ble-e-ieee-802154).
**Demuestra:** aceptación/programación y al menos una entrega si hay un hit.
**No demuestra:** cinco emisiones físicas, intervalo real o fiabilidad de burst.

**Hardware/software:** dos CatSniffer; `ota_rx_probe.py` y [`ota_tx_burst.py`](../../../python/examples/lab/ota_tx_burst.py).
**Precondiciones:** OP-06 funcional, band1, marker nuevo.
**Entrada/working directory:** dos terminales, `FeralRF/python`.

```powershell
# Terminal 1
python .\examples\lab\ota_rx_probe.py --port COM_RX --phy 4 --channel 25 --duration 12 --marker-hex A30100000301 --match-mode contains --min-hits 5 --print-limit 80

# Terminal 2 tras RX_START ACK
python .\examples\lab\ota_tx_burst.py --port COM_TX --phy 4 --channel 25 --power 0 --payload-hex A30100000301 --count 5 --interval-us 250000 --tx-timeout 2
```

**Parámetros/procedimiento:** ejecute antes control negativo; no interprete `scheduled=5` como cinco completions.
**Observaciones esperadas:** ACK/`scheduled=5`; histórico y 2026-10-21 observaron 1/5.
**Aceptación:** cinco hits únicos y cleanup = `PASS` estrecho; 1–4 = `PARTIAL`/fallo del umbral; cero con ensayo válido = `FAIL`; cleanup fallido = `BLOCKED`. ACK solo no es PASS.
**Cleanup/recovery:** espere RX y `RX_STOP ACK`; no encadene otro burst tras timeout.
**Evidencia:** requested/scheduled, marker hits, ambos exits, ventana y cleanup.
**Limitaciones:** el helper sólo recibe ACK inicial; no cuenta emisiones físicas.
**Historia:** [`User-Manual/08`, OP-08](<../../User-Manual/08 - Validación reciente del manual frente a evidencia histórica.md>).

<a id="op-09"></a>
## OP-09 — `TX_CONTINUOUS` y `TX_STOP`

**Estado:** `CURRENT_PARTIAL`
**Objetivo:** comparar intervalo positivo y cero, ejecutar STOP y observar una ventana posterior.
**Capacidad relacionada:** [`TX_CONTINUOUS`, `TX_STOP` y `RX_STOP`](../../CURRENT_VALIDATION_MATRIX.md#52-phy-ble-e-ieee-802154).
**Demuestra:** control start/stop y actividad peer durante la ventana.
**No demuestra:** cadencia física, número de emisiones ni instante exacto de cese.

**Hardware/software:** dos CatSniffer; `ota_rx_probe.py` y [`smoke_tx_continuous_phase1.py`](../../../python/examples/smoke_tx_continuous_phase1.py).
**Precondiciones:** OP-06, markers distintos por variante, recuperación disponible.
**Entrada/working directory:** dos terminales, `FeralRF/python`.

**Variante A, 250000 µs:**

```powershell
# RX primero
python .\examples\lab\ota_rx_probe.py --port COM_RX --phy 4 --channel 25 --duration 10 --marker-hex A40100000401 --match-mode contains --min-hits 5 --print-limit 120
# TX tras RX_START ACK
python .\examples\smoke_tx_continuous_phase1.py --port COM_TX --phy 4 --channel 25 --power 0 --packet-hex A40100000401 --interval-us 250000 --run-seconds 3 --tx-timeout 2
```

**Variante B, 0 µs:**

```powershell
# RX primero
python .\examples\lab\ota_rx_probe.py --port COM_RX --phy 4 --channel 25 --duration 7 --marker-hex A40200000402 --match-mode contains --min-hits 20 --print-limit 160
# TX tras RX_START ACK
python .\examples\smoke_tx_continuous_phase1.py --port COM_TX --phy 4 --channel 25 --power 0 --packet-hex A40200000402 --interval-us 0 --run-seconds 1 --tx-timeout 2
```

Después de cada variante, en una sesión RX limpia, ejecute cinco segundos con el mismo marker y `--min-hits 0`; exija literalmente `marker_hits=0`.

**Observaciones esperadas:** `TX_CONTINUOUS ACK` y `TX_STOP ACK`; evidencia reciente: un hit a 250000 µs y 37/37 CRC-good a intervalo cero. El spacing host-visible ~1118,7 µs no es timing RF.
**Aceptación:** registre cada variante por separado. Actividad + STOP + control posterior limpio puede ser `PASS` de ese criterio; umbral parcial es `PARTIAL`; stop/cleanup timeout es `BLOCKED`.
**Cleanup/recovery:** el helper intenta STOP en `finally`; RX intenta stop. Si falta cualquier ACK, aplique recovery antes de otro modo.
**Evidencia:** markers, intervalos, run/window, hits, ACK, control posterior y cleanup.
**Limitaciones:** un hit posterior sin sincronización concluyente no prueba TX después de STOP.
**Historia:** [`User-Manual/08`, OP-09](<../../User-Manual/08 - Validación reciente del manual frente a evidencia histórica.md>).

<a id="op-10"></a>
## OP-10 — CW, PRBS y `TX_TEST_STOP`

**Estado:** `RECONSTRUCTED_UNVALIDATED`
**Objetivo:** ejercer una sola familia de test TX y confirmar el stop de control.
**Capacidad relacionada:** [CW/PRBS/`TX_TEST_STOP`](../../CURRENT_VALIDATION_MATRIX.md#53-propietaria-sub-ghz-y-modos-tx).
**Demuestra si se ejecuta:** únicamente ACK de ruta de control.
**No demuestra:** portadora, frecuencia, potencia, espectro, patrón PRBS o cese físico.

**Hardware/software:** una CatSniffer, carga/antena apropiada, API `tx_cw()`, `tx_prbs()` y `tx_test_stop()`.
**Precondiciones:** sesión recuperada; band1 para CW BLE ch37 o band2 para PRBS 868; ejecute una familia por proceso.
**Entrada/working directory:** API inline, `FeralRF/python`.

```powershell
$env:FERALRF_TEST_MODE = 'cw'  # o prbs15 / prbs32 tras seleccionar la banda
@'
import os, time
from feralrf import PHY, Radio
mode = os.environ["FERALRF_TEST_MODE"]
r = Radio(port="COM_DUT"); active = False
try:
    print("INFO:", r.init())
    if mode == "cw":
        r.set_phy(PHY.BLE_1M, channel=37); r.tx_cw(power_dbm=0)
    else:
        r.set_phy(PHY.SUB_1GHZ_868, channel=0); r.tx_prbs(power_dbm=0, pattern=mode)
    active = True; print(mode, "ACK"); time.sleep(.3)
finally:
    if active: r.tx_test_stop(); print("TX_TEST_STOP ACK")
    r.disconnect()
'@ | python -
$LASTEXITCODE
Remove-Item Env:FERALRF_TEST_MODE
```

**Parámetros/procedimiento:** sustituya COM; no combine bandas sin cerrar, cambiar Shell y abrir un proceso nuevo.
**Observaciones esperadas:** ACK del modo y `TX_TEST_STOP ACK`.
**Aceptación:** cuando se reejecute, `PASS` sólo de control requiere ambos ACK y cleanup; sin instrumento no cambie `Physical RF`.
**Cleanup/recovery:** STOP en `finally`; si no se confirma, power-cycle completo antes de otro PHY.
**Evidencia:** modo, banda lógica, ACK, exit, cleanup y, si se añade, traza de instrumento.
**Limitaciones:** la composición exacta no fue validada; no use `smoke_f22_tx_test.py`, que deriva Shell y usa condiciones históricas no seguras.
**Historia:** EV-13 y [`User-Manual/06`, OP-10](<../../User-Manual/06 - Procedimientos IEEE, TX y modos de prueba.md>).

<a id="op-11"></a>
## OP-11 — Switching IEEE y transición cross-PHY

**Estado:** `BLOCKED`
**Objetivo:** conservar dos objetivos distintos: IEEE 25→20→25 y IEEE↔Sub-GHz.
**Capacidades relacionadas:** [switching IEEE y transición IEEE↔Sub-GHz](../../CURRENT_VALIDATION_MATRIX.md#52-phy-ble-e-ieee-802154).
**Demuestra históricamente:** una corrida estrecha observó 10/10, 0/10 mismatch, 10/10 y 9/10 al volver; la transición Sub-GHz no se alcanzó.
**No demuestra:** switching global estable ni transición cross-PHY.

**Hardware/software:** dos CatSniffer; los runners coordinados `preflight.py`, `rf01_ieee_channel_switch.py` y `rf03_cross_phy_switch.py` serían requeridos, pero están ausentes del checkout.
**Precondiciones:** tooling restaurado y revisado, power-cycle disponible, dos pares Bridge/Shell confirmados.
**Entrada/working directory/comando:** **ninguna entrada actual ejecutable**. No sustituya silenciosamente los scripts por OP-06. Los comandos históricos se conservan en [`User-Manual/06`, OP-11](<../../User-Manual/06 - Procedimientos IEEE, TX y modos de prueba.md>) y apuntan a archivos inexistentes.

**Parámetros/procedimiento esperado:** RF-01 y RF-03 deben ser ejecuciones separadas con preflight y recovery frescos.
**Observaciones/aceptación:** RF-01 sólo podría pasar con cuatro etapas y cleanup; RF-03 sólo al alcanzar y verificar ambos PHY con cleanup. Un bloqueo previo no es FAIL de la transición.
**Cleanup/recovery:** preserve JSONL, power-cycle ≥15 s y control fresco; no confunda desconexión con `RX_STOP ACK`.
**Evidencia:** runner/hash restaurado, comando, JSONL por etapa, markers, exits y recovery.
**Limitaciones:** missing tooling confirmado; `RX_STOP` recurrente impidió cierre.
**Historia:** [`User-Manual/08`, OP-11](<../../User-Manual/08 - Validación reciente del manual frente a evidencia histórica.md>).

<a id="op-15"></a>
## OP-15 — KillerBee HIL y PCAP

**Estado:** `BLOCKED`
**Objetivo:** validar captura IEEE por el adaptador KillerBee y producir un PCAP útil.
**Capacidad relacionada:** [KillerBee adapter](../../CURRENT_VALIDATION_MATRIX.md#55-integraciones-protocolos-nombrados-y-funciones-experimentales).
**Demuestra si se completa:** integración host/HIL para captura en las condiciones del ensayo.
**No demuestra:** inyección, interoperabilidad general ni propiedades RF.

**Hardware/software:** una CatSniffer, [`killerbee_sniff.py`](../../../python/examples/killerbee_sniff.py), adapter host y dependencia externa opcional `killerbee>=3.0.0`.
**Precondiciones:** dependencia instalable/reproducible, band1, tráfico atribuido y ruta de salida. La campaña RF-04 no cumplió el primer requisito.
**Entrada/working directory:** helper actual en `FeralRF/python`, pero **no se autoriza ejecutarlo como procedimiento current mientras la dependencia siga bloqueada**.

Comando condicionado para una futura repetición, no evidencia actual:

```powershell
python .\examples\killerbee_sniff.py --port COM_RX --channel 25 --count 10 --timeout 100 --pcap .\killerbee-ch25.pcap
```

**Parámetros/procedimiento:** sólo después de resolver y registrar la dependencia; preserve versión del paquete.
**Observaciones/aceptación:** `PASS` requiere paquetes atribuibles, PCAP no vacío, apertura independiente y cleanup; importar el adapter o pasar tests host no basta.
**Cleanup/recovery:** `sniffer_off()`/`close()` en `finally`; ante fallo conserve el archivo parcial y no anuncie PCAP exitoso.
**Evidencia:** resolución de dependencia, versiones, comando, stdout, PCAP/hash, validación externa y cleanup.
**Limitaciones:** RF-04 quedó bloqueado antes de hardware; no produjo PCAP.
**Historia:** [`killerbee-catsniffer.md`](../../killerbee-catsniffer.md) y [`CURRENT_VALIDATION_MATRIX.md`](../../CURRENT_VALIDATION_MATRIX.md#55-integraciones-protocolos-nombrados-y-funciones-experimentales).
