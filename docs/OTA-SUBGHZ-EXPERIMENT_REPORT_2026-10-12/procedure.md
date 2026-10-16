# Guía enfocada OTA FeralRF — TX/RX con dos CatSniffer

## Propósito, fuentes y límite

Objetivo inmediato:

> Verificar que FeralRF transmite bytes/tramas RF conocidos desde un CatSniffer V3 y que un segundo CatSniffer V3 recibe físicamente la transmisión esperada.

Esta guía fue reconstruida el 2026-10-07 exclusivamente desde el Vault autoritativo `Understanding-CatSniffer` y el código de `FeralRF` en `0178721cbd4f0d0f6f8eba5ae919ca46066d5dea`. El Vault obsoleto `CatSniffer-Understanding` y sus borradores no se usaron como evidencia.

No se ejecutaron radios, tests, builds ni flashing al producirla. No valida stacks Zigbee, Thread, Matter, GATT, W-MBus, Wi-SUN, Sidewalk o MIOTY; el nombre de un preset sólo designa parámetros RF. CW, PRBS, jamming, crypto, RSA, spectrum y robustez exhaustiva quedan fuera del núcleo.

Regla central:

```text
Comando aceptado / ACK recibido
        ≠
Transmisión RF demostrada
```

## Línea base auditada

### Repositorios

| Artefacto | Rama/upstream | Commit | Estado al auditar |
| --- | --- | --- | --- |
| `FeralRF` | `main` / `origin/main` | `0178721cbd4f0d0f6f8eba5ae919ca46066d5dea`, 2026-07-22 | submódulo SDK marcado modificado; sin cambios producidos por esta guía |
| Python FeralRF | — | versión `0.3.0` | desde `python/feralrf/__init__.py` |
| TI SDK submódulo | detached | `5b31d0a4903351e544546e23ef3330eaa4291ceb` (`lpf2-8.30.01.01`) | `Board.md` marcado `M` preexistente |
| este Vault | `main` / `origin/main` | `bce7a55560178748288e8b4fd37215c6d06d2643`, 2026-10-07 | tenía cambios de usuario preexistentes; esta nota añade documentación |
| CatSniffer-Firmware, sólo lectura | `v3.x` | `c0cd5a45e019dbd14ed11d039aacb13f300e5731` | limpio |
| CatSniffer-Tools, sólo lectura | `fix/CLI_control` | `126f13bc0441ad3f37fe0b029160a3c526e4d309` | limpio; cinco commits detrás del upstream al auditar |

### Equipo actual

| Dispositivo | Board | Cat-Bridge | Cat-LoRa | Cat-Shell |
| --- | --- | ---: | ---: | ---: |
| CatSniffer #1 | V3 RP2040 + CC1352P7 | `COM33` | `COM34` | `COM35` |
| CatSniffer #2 | V3 RP2040 + CC1352P7 | `COM88` | `COM86` | `COM87` |

Dirección inicial: `COM33 TX → RF → COM88 RX`. La línea base se repite como `COM88 TX → RF → COM33 RX`. Los resultados no se promedian.

Estas asignaciones son de la sesión actual, no identidad permanente. Verificarlas al comienzo. Nunca calcular Shell como `Bridge+2`: `COM88 → COM87` demuestra que esa regla es inválida. `FERALRF1` tampoco identifica físicamente una placa.

## Modelo de evidencia vigente

Se preservan las definiciones canónicas actuales del Vault:

- **A:** evidencia RF física directa por receptor/instrumento independiente del transmisor. Otro FeralRF observando TX cuenta como A, con independencia limitada por implementación compartida.
- **B:** comportamiento directo del dispositivo/firmware.
- **C:** control/API; declarar si es mock. Un ACK TX prueba aceptación/programación, no emisión.
- **D:** implementación o documentación estática.
- **E:** inferencia o hipótesis.
- **F:** no evaluado.

El nivel se registra separado de `PASS`, `PARTIAL`, `FAIL`, `INCONCLUSIVE` o `NOT FULLY VALIDATED`. `FakeSerial` sólo ofrece C de host/mock.

## Reauditoría de EV

| EV | Estado autoritativo actual | Qué se ejecutó físicamente | Qué prueba realmente | ¿Sólo ACK/control? | ¿RF observada? | ¿Bytes verificados? | ¿Conteo verificado? | ¿Relevante? | Acción nueva |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| EV-05 | PASS+B local; atribución media | RX IEEE ch25, 3×30 s: 41/43/43 paquetes | RX local repetible frente a tráfico ambiental/fuente | No | Sí, RX local; no TX propio atribuido | No completos | Sí, conteo de observación | Sí, preflight RX | KEEP |
| EV-10 | PASS+C, 8/8 | Configuración/ACK con reset entre casos | API/configuración | Sí | No | No | Sólo casos control | Secundaria | KEEP como control |
| EV-11 | PARTIAL; 18 casos C individuales y 9 C resumidos | 27 presets a nivel control | Aceptación/configuración y un TX_RAW ACK por caso | Sí | No | No OTA | 27 casos declarados; 9 sin stdout literal | Sí | REWORK como OTA por tiers |
| EV-12 | PARTIAL global | OTA COM33→COM88 IEEE ch25: RAW, FRAME, BURST, CONTINUOUS y STOP | RAW/FRAME marcador recibido; CONT interval=0 actividad repetida; anomalías de modos programados | Mixto | Sí, dirección única | Marcador/prefijo sí; semántica de 2 bytes finales no cerrada | No para emisión solicitada | Central | COMPLETE/REPEAT dirigido |
| EV-13 | PARTIAL | CW BLE1M y PRBS15/32 Sub-G, ventanas 0.3 s; STOP ACK | Control C y comportamiento BLE local B | Mayormente | No medición física de onda | N/A | No | No, no son bytes | OUT OF CURRENT SCOPE |
| EV-14 | NOT FULLY VALIDATED | Bytes IEEE, cambio BLE→IEEE y tres tests FakeSerial | Baseline B y manejo host C mock | Parcial | No error asíncrono inducido físicamente | Parcial | No | Útil para registrar errores | OPTIONAL |
| EV-20 | PARTIAL indirecto por EV-12 | Una dirección y marcadores aislados | Existe camino OTA IEEE | No | Sí | Prefijo | No serie/bidireccional | Central | COMPLETE bidireccional |
| EV-21 | PARTIAL | RX ambiental BLE1M; otros PHY control | RX BLE1M B; no OTA propio completo | Mixto | Ambiental, no TX propio | No | No | Opcional | OPTIONAL |
| EV-22 | PARTIAL/control | Casos 868/915/4FSK de EV-10/11 | Configuración C | Sí | No | No | No | Sí, representativos | COMPLETE/REWORK |
| EV-23 | PARTIAL/control | Seis casos 433 C; historial RF marginal | Configuración, no camino actual | Sí | No actual | No | No | Sólo con antena/ruta | BLOCKED/OPTIONAL |
| EV-24 | F/no ejecutado actual | OOK excluido de EV-11 | Nada OTA; riesgo de bloqueo/recovery | — | No | No | No | No | OUT OF CURRENT SCOPE |
| EV-25 | PARTIAL/control | Presets W-MBus C | Parámetros RF aceptados, no W-MBus | Sí | No | No | No | Opcional | OPTIONAL |
| EV-26 | PARTIAL/control resumido | Presets Wi-SUN/Sidewalk C | Parámetros RF aceptados, no interoperabilidad | Sí | No | No | No | Opcional | OPTIONAL |
| EV-27 | PARTIAL/control | Dos presets proprietary 2.4 C | Configuración C; afirmaciones históricas no resuelven OTA actual | Sí | No actual | No | No | Sí | REWORK OTA |
| EV-28 | D/helper | Sin corrida dedicada válida | Sólo capacidad estática del helper | — | No | No | No | Secundaria | OPTIONAL |
| EV-29 | Bloqueado/Mocks | Adapter mock; KillerBee real no validado | Contrato host parcial | Sí/mock | No | No | No | No para núcleo | OUT OF CURRENT SCOPE |
| EV-44 | Sin estrés válido | Anomalía BURST de EV-12 | No caracteriza capacidad ni tasa | Mixto | Observaciones insuficientes | Marcador aislado | No | Después del básico | BLOCKED |

### EV-11, respuesta explícita

El resultado previo quedó **independientemente reconfirmado** desde el Vault actual y el código actual:

1. Se declararon 27 casos: 6 en 433, 8 en 868, 2 en 169, 9 en 902/915 y 2 en 2440 MHz. El código actual contiene 31 presets; OOK×3 y MIOTY fueron excluidos de aquella campaña.
2. Los 27 sólo produjeron evidencia de control/ACK; ninguno documenta recepción RF por el otro dispositivo.
3. Ninguno tiene evidencia OTA física asociada.
4. Dieciocho tienen stdout individual: 6+8+2+2.
5. Los nueve de 902/915 sólo tienen resumen, por lo que su auditabilidad es menor.
6. Los 27 siguen siendo útiles para compatibilidad de configuración C; no son éxito RF.
7. Sólo una selección OTA representativa debe repetirse primero. La expansión se hace por tiers, no como barrido ciego de 31 casos.

### EV-12 por modo

| Modo | Evidencia de control | Evidencia RF | Evidencia de payload | Evidencia de conteo | Evidencia STOP/cese | Conclusión actual |
| --- | --- | --- | --- | --- | --- | --- |
| RAW | ACK C | 1 marcador entre 23 paquetes tras control 0/23 | `deadbeef1519`, CRC reportado válido | Una recepción; no una emisión única | N/A | PASS-A limitado para camino/prefijo; repetir bidireccional/serie |
| FRAME | ACK C; primer intento tuvo marcador equivocado | 1/30 con marcador corregido | `a1b2c3d417f1`, CRC válido | Una recepción | N/A | PASS-A limitado; contrato host distinto pero TX de firmware compartido |
| BURST | ACK C | 1 marcador en corridas que pedían 40 o 5 | Marcador sí | Umbral del observador falló; emisión real pedida queda inconclusa | N/A | FAIL del criterio del observador, no prueba de que DUT emitió sólo una |
| CONTINUOUS | ACK C | interval=0: 99 coincidencias/102; intervalos positivos: una coincidencia por corrida | Marcador sí; CRC reportado válido | Actividad repetida para 0; exactitud/tasa no establecidas | Separado | PARTIAL; repetir dos puntos después de one-shot |
| STOP | ACK C | No se midió ventana post-STOP | N/A | N/A | Cese físico F; además RX_STOP tuvo timeout y 9 respuestas inesperadas | Control solamente; completar con control negativo post-STOP |

En IEEE, el código actual configura RX con cabecera PHY y CRC incluidos. El parser elimina el byte PHR/longitud y entrega los siguientes `length` bytes; TX entrega el payload al driver TI, que genera PHR/FCS. Por D, la relación esperada es:

```text
Packet.data esperado = bytes solicitados + 2 bytes FCS generados por radio
```

La aceptación de esta campaña exige prefijo exacto, longitud `len(solicitado)+2`, `crc_ok=True` y registrar los dos bytes finales. Esto no reclasifica retroactivamente EV-12: allí los dos bytes finales no se verificaron expresamente como FCS.

`TX_FRAME` posee contrato host/potencia distinto, pero `ControlTask_onTxFrame()` delega al mismo camino real que RAW; por eso basta una comprobación física dedicada, no una matriz duplicada.

## Inventario de ejemplos actuales

| Script actual | Ruta desde raíz | TX | RX | Dos placas | Modo/PHY | ¿Reset? | Supuesto Shell | ¿RF observable? | Decisión |
| --- | --- | ---: | ---: | ---: | --- | --- | --- | --- | --- |
| `ota_rx_probe.py` | `python/examples/lab/` | No | Sí | Como observador | PHY/canal y match modes | No | Ninguno | Sí; filtra `RxStreamError` de la lista | KEEP |
| `smoke_tx_phase1.py` | `python/examples/` | Sí | No | Con observador aparte | RAW, PHY/canal | No | Ninguno | Sólo por peer | KEEP |
| `ota_tx_frame.py` | `python/examples/lab/` | Sí | No | Con observador aparte | FRAME | No | Ninguno | Sólo por peer | KEEP |
| `ota_tx_burst.py` | `python/examples/lab/` | Sí | No | Con observador aparte | BURST | No | Ninguno | Sólo por peer; desconecta tras ACK | KEEP con cautela |
| `smoke_tx_continuous_phase1.py` | `python/examples/` | Sí | No | Con observador aparte | CONTINUOUS+STOP | No | Ninguno | Sí mediante peer | KEEP |
| `smoke_phy4_ieee154.py` | `python/examples/` | No | Sí | No requerido | IEEE RX baseline | No | Ninguno | Sí, ambiental | KEEP preflight |
| `smoke_prop_phase1.py` | `python/examples/` | Sí | Sí local | No coordinado | Preset proprietary | Sólo con `--auto-reset` | Con auto-reset hereda `+2` | No prueba OTA solo | Control, no core |
| `smoke_ota_txrx.py` | `python/examples/` | Sí | Sí | Sí | RAW/preset | Sí | deriva Shell `Bridge+2` | Sí | NO usar sin corregir |
| `smoke_f29_subg_915.py` | `python/examples/` | Sí | Sí | Sí | siete presets | Sí | deriva `+2` | Sí | NO usar |
| `smoke_f9_phy_matrix_ota.py` | `python/examples/lab/` | Sí | Sí | Sí | matriz PHY | Sí | deriva `+2` | Sí | NO usar |
| `smoke_f22_tx_test.py` | `python/examples/lab/` | Sí | Sí | Sí | TX | Sí | deriva `+2` | Sí | NO usar |
| `smoke_f17_emulation.py` | `python/examples/` | Sí | Sí | Sí | emulación | Sí | deriva `+2` | Potencial | NO usar |
| `killerbee_sniff.py` | `python/examples/` | No | Sí | No | IEEE/KillerBee | No | Ninguno | RX solamente | OPTIONAL |

`run_validation.py` y `Radio.reset_device()` también calculan Shell como `Bridge+2`; no se usan. Ningún ejemplo con `--auto-reset` debe recibir esa opción. `ota_rx_probe.py` es útil para paquetes, pero descarta objetos `RxStreamError`; anotar cualquier excepción/stdout y no usarlo para declarar ausencia de errores asíncronos.

## Preflight seguro

Entorno: RF autorizado, antena/carga correcta para **ambas** placas y la banda, separación/acoplamiento controlados, 0 dBm inicial, ventanas breves. No usar Catnip `sniff`, flashing ni reset. Los comandos siguientes parten siempre de:

```powershell
Set-Location 'C:\Users\Support\Documents\ec-projects\catsniffer-feralrf\FeralRF'
$env:PYTHONPATH = (Resolve-Path '.\python').Path
Get-CimInstance Win32_SerialPort | Sort-Object DeviceID | Select-Object DeviceID, Name, PNPDeviceID
```

Cerrar monitores seriales. Confirmar físicamente qué placa responde en cada Shell con operaciones ASCII que no flashean:

```powershell
python -c "import serial,time; s=serial.Serial('COM35',115200,timeout=.5); s.write(b'identify\r\n'); time.sleep(.3); s.write(b'fw_version\r\n'); time.sleep(.3); s.write(b'status\r\n'); time.sleep(.5); print(s.read_all().decode(errors='replace')); s.close()"
python -c "import serial,time; s=serial.Serial('COM87',115200,timeout=.5); s.write(b'identify\r\n'); time.sleep(.3); s.write(b'fw_version\r\n'); time.sleep(.3); s.write(b'status\r\n'); time.sleep(.5); print(s.read_all().decode(errors='replace')); s.close()"
```

Registrar respuestas, etiquetas físicas, antenas y firmware. Si no coinciden con la tabla, detenerse y corregir todos los comandos; no inferir puertos.

Verificar que cada Bridge ejecuta FeralRF, sin reset:

```powershell
python -c "from feralrf import Radio; r=Radio('COM33'); print(r.init()); print(r.get_stats()); r.disconnect()"
python -c "from feralrf import Radio; r=Radio('COM88'); print(r.init()); print(r.get_stats()); r.disconnect()"
```

Baseline RX IEEE ch25, una placa a la vez:

```powershell
python .\python\examples\smoke_phy4_ieee154.py --port COM88 --channel 25 --duration 10
python .\python\examples\smoke_phy4_ieee154.py --port COM33 --channel 25 --duration 10
```

Esto sólo prueba RX B/ambiental. No usar la ausencia de tráfico ambiental como FAIL de FeralRF.

## Selección explícita del frente RF

Fuente actual de CatSniffer-Firmware:

- Shell `band1` = 2.4 GHz; `change_band(GIG)` programa CTF `0,1,0`.
- Shell `band2` = Sub-GHz; `change_band(SUBGIG_1)` programa CTF `0,0,1`.
- U2 usa GPIO RP2040 8/9/10.

Incertidumbre crítica: el estado global inicia en cero y `GIG` también puede ser cero; `change_band(GIG)` retorna si cree estar ya en ese estado. Por ello se fuerza una transición. Para 2.4 GHz, ejecutar `band2` y después `band1` en ambos Shell:

```powershell
python -c "import serial,time; s=serial.Serial('COM35',115200,timeout=.5); s.write(b'band2\r\n'); time.sleep(.3); s.write(b'band1\r\n'); time.sleep(.3); print(s.read_all().decode(errors='replace')); s.close()"
python -c "import serial,time; s=serial.Serial('COM87',115200,timeout=.5); s.write(b'band2\r\n'); time.sleep(.3); s.write(b'band1\r\n'); time.sleep(.3); print(s.read_all().decode(errors='replace')); s.close()"
```

Para Sub-GHz, ejecutar `band1` y después `band2`:

```powershell
python -c "import serial,time; s=serial.Serial('COM35',115200,timeout=.5); s.write(b'band1\r\n'); time.sleep(.3); s.write(b'band2\r\n'); time.sleep(.3); print(s.read_all().decode(errors='replace')); s.close()"
python -c "import serial,time; s=serial.Serial('COM87',115200,timeout=.5); s.write(b'band1\r\n'); time.sleep(.3); s.write(b'band2\r\n'); time.sleep(.3); print(s.read_all().decode(errors='replace')); s.close()"
```

La respuesta Shell es B de comando, no medición GPIO ni confirmación de la imagen instalada. Si no se conoce el hash del RP2040 o no se demuestra ruta/antena, cero paquetes significa **BLOCKED/INCONCLUSIVE**, no FAIL FeralRF.

## Suite mínima derivada

Orden: one-shot antes de BURST/CONTINUOUS; 2.4 GHz antes de cambiar de banda; Sub-G sólo en banda autorizada.

### 1. RAW IEEE bidireccional

Usar la selección 2.4 GHz anterior. Terminal RX primero. Cada dirección tiene control negativo de 5 s y luego diez solicitudes. Marcadores únicos de esta sesión.

Dirección #1→#2, negativo:

```powershell
python .\python\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 5 --marker-hex a10133008801 --match-mode contains --min-hits 0 --print-limit 30
```

Exigir `marker_hits=0`. Positivo: iniciar primero RX en terminal A:

```powershell
python .\python\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 60 --marker-hex a10133008801 --match-mode contains --min-hits 10 --print-limit 80
```

Luego TX en terminal B:

```powershell
1..10 | ForEach-Object { python .\python\examples\smoke_tx_phase1.py --port COM33 --phy 4 --channel 25 --power 0 --packet-hex a10133008801 --tx-timeout 2 }
```

Dirección #2→#1, negativo:

```powershell
python .\python\examples\lab\ota_rx_probe.py --port COM33 --phy 4 --channel 25 --duration 5 --marker-hex a10288003301 --match-mode contains --min-hits 0 --print-limit 30
```

Positivo RX primero:

```powershell
python .\python\examples\lab\ota_rx_probe.py --port COM33 --phy 4 --channel 25 --duration 60 --marker-hex a10288003301 --match-mode contains --min-hits 10 --print-limit 80
```

TX después:

```powershell
1..10 | ForEach-Object { python .\python\examples\smoke_tx_phase1.py --port COM88 --phy 4 --channel 25 --power 0 --packet-hex a10288003301 --tx-timeout 2 }
```

Por paquete esperado: prefijo solicitado exacto, longitud 8 (`6+2`), `crc_ok=True`; guardar los 2 bytes FCS. Registrar ACKs y recepciones por separado. `PASS-RF` requiere negativo cero, 10 solicitudes ACK y 10 observaciones atribuibles; menos de 10 es PARTIAL/INCONCLUSIVE para fiabilidad, aunque puede demostrar camino OTA.

### 2. FRAME contractual

Una dirección basta inicialmente porque firmware comparte el camino RF con RAW. Negativo:

```powershell
python .\python\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 5 --marker-hex a20133008802 --match-mode contains --min-hits 0 --print-limit 30
```

RX positivo en terminal A:

```powershell
python .\python\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 45 --marker-hex a20133008802 --match-mode contains --min-hits 10 --print-limit 80
```

TX terminal B:

```powershell
python .\python\examples\lab\ota_tx_frame.py --port COM33 --phy 4 --channel 25 --power 0 --payload-hex a20133008802 --count 10 --interval-ms 250 --tx-timeout 2 --progress-every 1
```

Esperado por D: los 6 bytes solicitados como prefijo y 2 FCS; mismos criterios RF que RAW.

### 3. BURST caracterizado

Sólo después de RAW/FRAME. Negativo con marcador `a30133008803` como arriba:

```powershell
python .\python\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 5 --marker-hex a30133008803 --match-mode contains --min-hits 0 --print-limit 30
```

Prueba conservadora. RX primero:

```powershell
python .\python\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 12 --marker-hex a30133008803 --match-mode contains --min-hits 5 --print-limit 80
```

TX:

```powershell
python .\python\examples\lab\ota_tx_burst.py --port COM33 --phy 4 --channel 25 --power 0 --payload-hex a30133008803 --count 5 --interval-us 250000 --tx-timeout 2
```

Segundo punto sólo si el anterior funciona, nuevo marcador:

```powershell
python .\python\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 12 --marker-hex a30233008803 --match-mode contains --min-hits 20 --print-limit 100
python .\python\examples\lab\ota_tx_burst.py --port COM33 --phy 4 --channel 25 --power 0 --payload-hex a30233008803 --count 20 --interval-us 50000 --tx-timeout 2
```

Las dos últimas órdenes van en terminales separadas, RX primero. Registrar requested count, ACK, coincidencias RF, duplicados, errores y ventana. El helper desconecta inmediatamente después del ACK; una diferencia RX no determina por sí sola cuántas emisiones hizo el DUT.

### 4. CONTINUOUS + STOP físico

Después de BURST. Negativo:

```powershell
python .\python\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 5 --marker-hex a40133008804 --match-mode contains --min-hits 0 --print-limit 30
```

RX activo primero:

```powershell
python .\python\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 10 --marker-hex a40133008804 --match-mode contains --min-hits 5 --print-limit 120
```

TX durante 3 s, intervalo 250000 µs; el script manda STOP:

```powershell
python .\python\examples\smoke_tx_continuous_phase1.py --port COM33 --phy 4 --channel 25 --power 0 --packet-hex a40133008804 --interval-us 250000 --run-seconds 3 --tx-timeout 2
```

Al terminar ambos, control físico post-STOP:

```powershell
python .\python\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 5 --marker-hex a40133008804 --match-mode contains --min-hits 0 --print-limit 40
```

Exigir cero coincidencias post-STOP. Repetir sólo después con intervalo cero y marcador nuevo:

```powershell
python .\python\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 7 --marker-hex a40233008804 --match-mode contains --min-hits 20 --print-limit 160
python .\python\examples\smoke_tx_continuous_phase1.py --port COM33 --phy 4 --channel 25 --power 0 --packet-hex a40233008804 --interval-us 0 --run-seconds 1 --tx-timeout 2
python .\python\examples\lab\ota_rx_probe.py --port COM88 --phy 4 --channel 25 --duration 5 --marker-hex a40233008804 --match-mode contains --min-hits 0 --print-limit 40
```

Ejecutar RX/TX simultáneamente y el último control después. Registrar intervalo, duración, timestamps, coincidencias, ACK STOP y cese. No equiparar conteo RX con conteo emitido.

### 5. Presets proprietary: wrapper seguro, sin reset

El helper siguiente usa puertos explícitos, inicia RX antes de TX, ejecuta negativo, imprime errores asíncronos y compara el marcador como subsecuencia porque proprietary RX incluye header y CRC mientras TX copia el payload exacto. Ejecutarlo una vez por sesión para definir la función:

```powershell
function Invoke-FeralPresetOta {
  param([string]$TxPort,[string]$RxPort,[string]$Preset,[string]$MarkerHex)
  @'
import sys, time
from feralrf import PHY, PROP_PRESETS, Packet, Radio, RxStreamError
tx_port, rx_port, preset, marker_hex = sys.argv[1:]
marker = bytes.fromhex(marker_hex)
tx, rx = Radio(tx_port), Radio(rx_port)
try:
    tx.connect(); rx.connect()
    for r in (tx, rx):
        r.init(); r.set_phy(PHY.PROPRIETARY_GFSK, 0); r.configure_prop(**PROP_PRESETS[preset])
    tx.set_power(0); rx.start_rx()
    neg = list(rx.read_packets(timeout=3.0))
    neg_packets = [x for x in neg if isinstance(x, Packet)]
    neg_errors = [x for x in neg if isinstance(x, RxStreamError)]
    neg_hits = [x for x in neg_packets if x.crc_ok and marker in x.data]
    print('NEG', 'hits=', len(neg_hits), 'errors=', len(neg_errors))
    for x in neg: print('NEG_ITEM', x)
    acks = 0
    for _ in range(10):
        tx.transmit(marker, power_dbm=0, timeout=2.0); acks += 1; time.sleep(0.1)
    pos = list(rx.read_packets(timeout=5.0))
    packets = [x for x in pos if isinstance(x, Packet)]
    errors = [x for x in pos if isinstance(x, RxStreamError)]
    hits = [x for x in packets if x.crc_ok and marker in x.data]
    print('RESULT', preset, tx_port, '->', rx_port, 'acks=', acks,
          'hits=', len(hits), 'errors=', len(errors))
    for x in pos: print('RX_ITEM', x)
    sys.exit(0 if not neg_hits and not neg_errors and acks == 10 and len(hits) == 10 and not errors else 7)
finally:
    for r in (rx, tx):
        try: r.stop_rx()
        except Exception: pass
        try: r.disconnect()
        except Exception: pass
'@ | python - $TxPort $RxPort $Preset $MarkerHex
}
```

Antes de estos casos, aplicar `band1`→`band2` en ambos Shell, usar antenas Sub-G correctas y escoger **sólo una opción autorizada** de banda primaria:

```powershell
Invoke-FeralPresetOta -TxPort COM33 -RxPort COM88 -Preset gfsk_868_50k -MarkerHex b10133008801
Invoke-FeralPresetOta -TxPort COM88 -RxPort COM33 -Preset gfsk_868_50k -MarkerHex b10288003301
```

o, en entorno autorizado para 915 MHz:

```powershell
Invoke-FeralPresetOta -TxPort COM33 -RxPort COM88 -Preset gfsk_915_50k -MarkerHex b11133008801
Invoke-FeralPresetOta -TxPort COM88 -RxPort COM33 -Preset gfsk_915_50k -MarkerHex b11288003301
```

Para modulación alternativa, sólo con autorización/antena/ruta 868 establecidas:

```powershell
Invoke-FeralPresetOta -TxPort COM33 -RxPort COM88 -Preset 4fsk_868_50k -MarkerHex b20133008802
```

Para proprietary 2.4, volver a `band2`→`band1` en ambos Shell y ejecutar:

```powershell
Invoke-FeralPresetOta -TxPort COM33 -RxPort COM88 -Preset gfsk_2440_50k -MarkerHex b30133008803
```

Esperado: el marcador solicitado aparece contiguo dentro del PDU recibido, `crc_ok=True`; registrar PDU completo, longitud, header y CRC. No exigir igualdad del paquete completo.

## Tiers de presets recalculados

> **Actualización 2026-10-08:** Tier A/B fue ejecutado parcialmente y extendido en [[04 - OTA proprietary - campaña ampliada]], con cero paquetes entregados. La expansión ciega queda pausada. Esta sección conserva el diseño del procedimiento, no un estado pendiente de barrido.

- **Tier A, representativo:** un GFSK autorizado entre `gfsk_868_50k` o `gfsk_915_50k`, bidireccional, y `gfsk_2440_50k`. `4fsk_868_50k` queda fuera como prueba de 4FSK: el setup TI auditado reserva `modType=5`.
- **Tier B, OTA ampliada:** tasas/modulaciones restantes de 868/902/915/2440 después del Tier A; presets W-MBus, Wi-SUN o Sidewalk sólo como parámetros RF y con lenguaje estricto.
- **Tier C, bloqueada/diferida:** OOK por lifecycle/recovery; MIOTY pending; 169 fuera de silicio/U4; 433 dentro del silicio pero fuera de U4 publicado y restringido por el asesor; cualquier banda no autorizada o cuyo selector no se pueda establecer.

Un éxito con preset nombrado W-MBus/Wi-SUN/Sidewalk/MIOTY sólo permite afirmar: “los endpoints FeralRF intercambiaron los bytes esperados bajo los parámetros RF asociados con ese preset”. No valida interoperabilidad.

## BLE opcional

No forma parte del mínimo si IEEE y proprietary ya cubren el objetivo. Si se ejecuta BLE1M, TX raw se encapsula como AdvData; RX entrega PDU LL sin CRC, y el matcher extrae AdvData tras cabecera de 2 bytes y AdvA de 6. Por ello no se compara `Packet.data` completo.

RX primero y TX después:

```powershell
python .\python\examples\lab\ota_rx_probe.py --port COM88 --phy 0 --channel 37 --duration 5 --marker-hex c10133008801 --match-mode ble_adv_payload_exact --min-hits 0 --print-limit 30
python .\python\examples\lab\ota_rx_probe.py --port COM88 --phy 0 --channel 37 --duration 45 --marker-hex c10133008801 --match-mode ble_adv_payload_exact --min-hits 10 --print-limit 80
1..10 | ForEach-Object { python .\python\examples\smoke_tx_phase1.py --port COM33 --phy 0 --channel 37 --power 0 --packet-hex c10133008801 --tx-timeout 2 }
```

BLE2M usa extended advertising con primario 1M ch37 y auxiliar 2M ch9; el matcher legacy no expresa ese contrato. Diferir hasta un observador/parser apropiado.

## Criterios de aceptación y captura

Un one-shot es `PASS-RF` sólo si:

1. RX estuvo configurado antes de TX.
2. PHY/configuración y frente RF coincidieron.
3. Se usó marcador único.
4. La solicitud TX y su ACK quedaron registrados por separado.
5. El peer reportó el paquete RF correspondiente.
6. La relación de payload coincide con la semántica documentada.
7. El marcador estuvo ausente en control negativo.
8. Puertos, roles, antenas, potencia, distancia, fecha y firmware quedaron registrados.

Guardar stdout literal por terminal, no sólo resumen. Para cada dirección/modo: bytes solicitados, bytes recibidos completos, CRC, RSSI/LQI/timestamps, ACK, coincidencias, no coincidencias, duplicados, errores y resultado. Para BURST/CONTINUOUS distinguir siempre requested/ACK/RX observed.

Detener la sesión ante puerto ambiguo, reset/bootloader inesperado, placa sin respuesta, calentamiento, ausencia de antena/carga apropiada, banda no autorizada, errores repetidos o imposibilidad de mandar STOP. Cerrar procesos y, si corresponde, mandar STOP desde el helper que mantiene la conexión; no improvisar flashing.

## KillerBee

No añade una prueba necesaria al núcleo. El ejemplo público sólo hace sniff RX y requiere dependencia opcional. El adapter `inject()` sí quita dos bytes FCS de una trama y llama `transmit_frame`, por lo que probaría la adaptación KillerBee→FRAME, no una ruta RF nueva. Diferirlo; no instalar dependencias ni afirmar compatibilidad.

## Decisión final de campaña

El mínimo para declarar cumplido el objetivo es: preflight/selector documentados, RAW IEEE 10/10 en ambas direcciones con controles negativos, FRAME físico una dirección, BURST y CONTINUOUS+STOP caracterizados sin confundir observación con emisión, y Tier A proprietary en rutas/bandas demostrables. Todo cero-packet bajo incertidumbre de U2/CTF, antena o firmware RP2040 conserva la causa física como `INCONCLUSIVE`; esto no impide registrar `FAIL` del criterio concreto de recepción OTA.

### Estado final tras la ejecución

- Preflight/control, RX IEEE en ambas placas, RAW bidireccional y FRAME cumplieron sus criterios; RAW/FRAME tienen evidencia física A y control C.
- BURST fue ejecutado y quedó `PARTIAL`: solicitudes de cinco produjeron una recepción atribuible; el conteo físico solicitado no fue validado.
- `TX_CONTINUOUS interval_us=0` demostró repetición OTA. `TX_STOP` quedó `PASS-CONTROL`; el cese RF inmediato permanece `NOT FULLY VALIDATED`. La campaña dirigida está en [[05 - OTA IEEE - CONTINUOUS STOP dirigido]].
- Tier A y parte de Tier B proprietary fueron ejecutados: 24 corridas, 240 retornos exitosos de `transmit()`, 19 presets y cero paquetes entregados. El criterio de recepción OTA falló; emisión, recepción física y causa raíz permanecen `INCONCLUSIVE` por no haberse medido CTF/U2/J1 ni energía RF.
- Tier C quedó bloqueado o diferido por las condiciones documentadas. BLE era opcional. KillerBee se difirió como integración host y no es una brecha del núcleo RF.

La campaña descrita por esta guía se considera **completada con elementos `PARTIAL`/`INCONCLUSIVE` documentados**, no “todas las funciones validadas”. Repetir más variantes OTA sin medir el frente RF o instrumentar firmware/colas ya no discrimina las hipótesis restantes. El siguiente paso es análisis físico CTF/U2/J1 para proprietary y análisis de implementación para BURST, `TX_STOP`, `RX_STOP` y las respuestas `0x90`. Véase [[Auditoría final de experimentos FeralRF]].

Véase [[Checklist OTA FeralRF - dos CatSniffer]] para la hoja breve de banco.
