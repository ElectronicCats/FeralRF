# 01 — OTA IEEE: RAW, FRAME y BURST con dos CatSniffer V3

## 1. Objetivo y alcance

El objetivo de esta fase fue determinar si FeralRF, ejecutándose sobre CatSniffer V3, es capaz de transmitir por RF datos conocidos desde un dispositivo y recibirlos físicamente en un segundo CatSniffer V3.

La pregunta principal fue:

> ¿Una orden de transmisión enviada mediante la API de FeralRF termina produciendo una emisión RF observable y atribuible en otro CatSniffer?

Esta distinción fue necesaria porque una respuesta `ACK` del firmware sólo demuestra que el comando fue aceptado por la ruta de control. Por sí misma, no demuestra que el RF Core haya emitido energía ni que los bytes solicitados hayan sido transmitidos correctamente por aire.

La validación se concentró en IEEE 802.15.4, PHY 4, canal 25, utilizando dos CatSniffer V3 como endpoints independientes. Se evaluaron `TX_RAW`, `TX_FRAME` y `TX_BURST`.

No se considera terminada todavía la caracterización general de todas las funciones de TX de FeralRF. Quedan para una fase posterior `CONTINUOUS + STOP`, PHY proprietary/Sub-GHz, presets adicionales, BLE y otras modalidades. El resultado de este informe debe interpretarse como el cierre de la **primera validación OTA dirigida a demostrar TX/RX físico**.

---

## 2. Baseline de software examinado

### FeralRF source tree

Código local examinado:

* branch: `main`
* HEAD: `0178721cbd4f0d0f6f8eba5ae919ca46066d5dea`
* Python package: `0.3.0`
* TI SimpleLink SDK utilizado por el árbol: `simplelink_cc13xx_cc26xx_sdk_8_30_01_01`

El análisis estático de firmware se realizó sobre este código.

### Limitación de procedencia del firmware instalado

Los dos CC1352P7 reportaron:

```text
feralrf_cc1352 (custom)
firmware_version = 1.0.0
```

pero no se obtuvo un hash Git que vincule de forma inequívoca el binario instalado con `0178721...`.

Por tanto:

**Código observado:** corresponde al source tree actual indicado arriba.

**Hardware observado:** ejecuta una imagen identificada como `feralrf_cc1352 (custom)` / `1.0.0`.

**Limitación:** no debe afirmarse todavía que el binario instalado sea byte a byte el correspondiente al HEAD examinado.

---

## 3. Arquitectura funcional involucrada en la prueba

La ruta validada fue:

```text
Python / feralrf.Radio
        ↓
COBS + protocolo FeralRF sobre USB CDC
        ↓
Cat-Bridge del RP2040
        ↓
UART hacia CC1352P7
        ↓
FeralRF sobre CC1352P7
        ↓
CommandProcessor / ControlTask / DataTask
        ↓
RadioIF
        ↓
TI RF Driver
        ↓
RF Core del CC1352P7
        ↓
front-end RF / antena
        ↓
aire
        ↓
segundo CatSniffer V3
```

El RP2040 no ejecuta el firmware RF de FeralRF. Actúa principalmente como bridge/controlador de las interfaces USB y del hardware auxiliar del CatSniffer. La lógica RF examinada de FeralRF reside en el CC1352P7.

El SX1262/LoRa del CatSniffer no intervino en estas pruebas.

---

## 4. Endpoints físicos identificados

Durante esta sesión se estableció el siguiente mapeo:

| Dispositivo   | Cat-Bridge |    LoRa | Cat-Shell |
| ------------- | ---------: | ------: | --------: |
| CatSniffer #1 |    `COM33` | `COM34` |   `COM35` |
| CatSniffer #2 |    `COM88` | `COM86` |   `COM87` |

Las direcciones OTA utilizadas fueron:

```text
#1: COM33 TX → RF → COM88 RX
#2: COM88 TX → RF → COM33 RX
```

Un hallazgo metodológico importante fue que no debe derivarse Cat-Shell como `Bridge + 2`. El segundo dispositivo constituye un contraejemplo directo:

```text
Bridge COM88
Shell  COM87
```

Por ello todos los puertos fueron tratados como asociaciones observadas durante la sesión y no como relaciones aritméticas.

También se observó que los dos endpoints FeralRF reportan el mismo serial lógico:

```text
464552414c524631
```

Ese valor identifica a FeralRF, pero no permite distinguir físicamente una placa de la otra.

---

## 5. Identidad del firmware RP2040 / Cat-Shell

### CatSniffer #1 — COM35

Se observó:

```text
FW v3.1.0.0
Git 8eaa84c
clean
build 2026-02-28
Radio: LoRa
CC1352 FW: feralrf_cc1352 (custom)
```

### CatSniffer #2 — COM87

Se observó:

```text
FW v3.1.0.1
Git c0cd5a4
clean
build 2026-09-04
Board v3 RP2040 CC1352P7
CC1352 FW: feralrf_cc1352 (custom)
```

Por tanto las dos placas **no utilizan exactamente la misma imagen del RP2040**.

Esta discrepancia debe conservarse como variable experimental. Sin embargo, los resultados RAW bidireccionales posteriores muestran que ambas direcciones consiguen comunicación OTA satisfactoria, por lo que esta diferencia no impidió el objetivo fundamental de esta fase.

---

## 6. Verificación de la ruta de control

Se ejecutó la inicialización de FeralRF mediante Cat-Bridge tanto en `COM33` como en `COM88`.

Ambos endpoints respondieron con:

```text
DeviceInfo(
    firmware_version='1.0.0',
    capabilities=7,
    serial='464552414c524631'
)
```

y estadísticas inicialmente sin actividad anómala.

### Interpretación

Esto constituye evidencia directa de funcionamiento de:

```text
PC → USB CDC → RP2040 bridge → UART → CC1352/FeralRF
```

pero **no es evidencia RF**.

Clasificación: **PASS-control / evidencia B-C**.

---

## 7. Preparación del front-end de 2.4 GHz

Antes de las pruebas OTA se forzó explícitamente una transición de banda mediante Cat-Shell en ambas placas:

```text
band2
band1
```

La secuencia produjo respuestas correspondientes primero a Sub-GHz y después a 2.4 GHz.

La transición se forzó deliberadamente en lugar de asumir que el estado inicial del front-end era correcto.

El hecho de que posteriormente exista comunicación IEEE 802.15.4 OTA satisfactoria constituye evidencia práctica fuerte de que la ruta de 2.4 GHz quedó funcional durante las pruebas.

No se realizó una medición eléctrica directa de GPIO/CTF, por lo que no se afirma cuál era el nivel físico de cada señal del switch.

---

## 8. Preflight de recepción IEEE 802.15.4

Antes de transmitir marcadores propios se verificó que ambos CatSniffer pudieran recibir actividad IEEE 802.15.4 ambiental en canal 25.

### COM88

```text
16 paquetes recibidos
primer paquete: canal 25
RSSI ≈ -73 dBm
crc_ok = True
```

### COM33

```text
15 paquetes recibidos
primer paquete: canal 25
RSSI ≈ -70 dBm
crc_ok = True
```

### Conclusión

Ambos dispositivos demostraron capacidad RX física independiente antes de utilizarlos como observadores del peer.

Estos paquetes no se atribuyeron al otro CatSniffer y no fueron usados como evidencia de TX de FeralRF.

---

# 9. Validación OTA RAW IEEE 802.15.4

## 9.1 CatSniffer #1 → CatSniffer #2

Marcador solicitado:

```text
a10133008801
```

### Control negativo

Antes de transmitir:

```text
marker_hits = 0
```

Por tanto no se observó el marcador de prueba de forma espontánea.

### Transmisión

Durante la ejecución positiva se realizaron accidentalmente 20 solicitudes en vez de las 10 inicialmente previstas.

Resultado de TX:

```text
20 solicitudes
20 Config ACK
20 TX_RAW ACK
```

### Recepción en COM88

Resultado:

```text
20 marker hits
data = a1013300880117b5
len = 8
crc_ok = True
RSSI ≈ -27 ... -28 dBm
```

Bytes solicitados:

```text
a1 01 33 00 88 01
```

Bytes recibidos:

```text
a1 01 33 00 88 01 17 b5
```

Los seis bytes solicitados aparecen exactamente como prefijo y los dos bytes adicionales corresponden al FCS generado en la ruta IEEE 802.15.4.

### Resultado

```text
20 TX solicitados
20 ACK
20 recepciones RF atribuibles
```

**Resultado: PASS-RF.**

---

## 9.2 CatSniffer #2 → CatSniffer #1

Marcador:

```text
a10288003301
```

### Control negativo

```text
marker_hits = 0
```

### TX COM88

```text
10 solicitudes
10 ACK
```

### RX COM33

```text
10 marker hits
data = a1028800330194d7
len = 8
crc_ok = True
RSSI ≈ -30 ... -31 dBm
```

FCS observado:

```text
94 d7
```

Resultado:

```text
10 solicitados
10 ACK
10 recepciones RF atribuibles
```

**Resultado: PASS-RF.**

---

## 9.3 Conclusión RAW

Quedó demostrado físicamente que dos CatSniffer V3 ejecutando FeralRF pueden intercambiar por aire bytes conocidos mediante `TX_RAW` usando IEEE 802.15.4 canal 25.

La capacidad quedó demostrada **en ambos sentidos**.

Esto ya no es evidencia basada exclusivamente en ACK o documentación: existe evidencia física obtenida en un segundo receptor.

No debe inferirse de los RSSI observados:

* sensibilidad del receptor;
* potencia EIRP;
* alcance;
* presupuesto de enlace;
* desempeño a distancia.

No se registraron con suficiente rigor distancia, geometría, orientación, cableado/antena y entorno RF para realizar esas afirmaciones.

---

# 10. Validación OTA TX_FRAME

Dirección evaluada:

```text
COM33 → RF → COM88
```

Marcador:

```text
a20133008802
```

### Control negativo

```text
marker_hits = 0
```

### TX

```text
10 solicitudes TX_FRAME
10 aceptadas
```

### RX

```text
10 marker hits
data = a20133008802f18b
len = 8
crc_ok = True
RSSI ≈ -29 ... -30 dBm
```

FCS:

```text
f1 8b
```

### Resultado

```text
10 solicitados
10 recepciones OTA atribuibles
```

**Resultado: PASS-RF.**

### Alcance de esta conclusión

`TX_FRAME` queda validado como contrato/API de transmisión, pero no debe presentarse como un camino RF independiente de RAW.

En el código examinado:

```c
ControlTask_onTxFrame(...)
{
    return ControlTask_onTxRaw(...);
}
```

por lo que FRAME termina delegando al mismo mecanismo de TX RAW.

La prueba es útil porque demuestra que el endpoint `TX_FRAME` expuesto al host termina generando una transmisión válida, no porque revele un segundo backend RF.

---

# 11. Validación TX_BURST

## 11.1 Prueba inicial

Marcador:

```text
a30133008803
```

Parámetros:

```text
count       = 5
interval_us = 250000
```

Control negativo:

```text
marker_hits = 0
```

TX:

```text
Config ACK
TX BURST PASS scheduled=5
```

La ventana RX fue de 12 segundos, ampliamente superior al tiempo nominal de aproximadamente 1 segundo necesario para cinco transmisiones separadas 250 ms.

### Resultado físico

COM88 recibió exactamente:

```text
1 marker hit
data = a30133008803539e
len = 8
crc_ok = True
RSSI ≈ -30 dBm
```

El criterio esperado era cinco observaciones.

### Resultado

**PARTIAL.**

Quedó demostrado que `TX_BURST` puede producir al menos una transmisión RF física.

No quedó demostrado que:

```text
count = 5
```

produzca cinco emisiones.

---

# 12. Prueba de la hipótesis «el host desconecta demasiado pronto»

El helper inicialmente utilizado termina la comunicación con el dispositivo después de recibir el ACK de BURST. Por ello surgió una hipótesis razonable:

> quizá BURST depende de que la conexión host permanezca abierta después del ACK.

Se diseñó una prueba específica para eliminar esa variable.

Marcador:

```text
a31133008803
```

Parámetros:

```text
count       = 5
interval_us = 250000
```

Secuencia:

```text
init
config
transmit_burst()
ACK
mantener conexión abierta 3 s
disconnect
```

### RX durante BURST

Ventana de observación:

```text
12 s
```

Resultado:

```text
24 paquetes totales observados
todos con CRC válido
1 marker hit
a31133008803132a
len = 8
RSSI ≈ -29 dBm
```

### Ventana posterior

Se mantuvo RX otros 12 segundos buscando exactamente el mismo marcador:

```text
15 paquetes ambientales
marker_hits = 0
```

### Conclusión

Mantener el host conectado durante 3 segundos **no recuperó las cuatro transmisiones restantes**.

Por tanto, la hipótesis:

```text
«BURST sólo transmite una vez porque el host desconecta inmediatamente»
```

queda **fuertemente debilitada**.

No se observaron tampoco las transmisiones restantes de forma tardía en los siguientes 12 segundos.

---

# 13. Análisis estático de TX_BURST

## 13.1 Semántica del comando

En:

```text
firmware/cc1352/src/command_processor.c
```

`CMD_TX_BURST` extrae:

```text
payload
tx_count
interval_us
```

y llama:

```c
ControlTask_onTxBurst(...)
```

El `ACK` se devuelve inmediatamente después de aceptar/programar el estado.

Por tanto:

> El ACK de BURST demuestra aceptación del comando y creación del estado de scheduling. No demuestra que todas las emisiones hayan terminado.

---

## 13.2 Estado del scheduler

En:

```text
firmware/cc1352/src/control_task.c
```

BURST mantiene, entre otros:

```c
s_tx_burst_pending
s_tx_burst_payload
s_tx_burst_remaining
s_tx_burst_interval_us
s_tx_burst_next_due_us
s_tx_burst_power_dbm
```

`ControlTask_onTxBurst()` establece:

```c
remaining = count;
interval = interval_us;
next_due = ControlTask_getTimeUs();
pending = true;
TaskEvent_set(TASK_EVENT_CONTROL_TX_BURST);
```

Después `ControlTask_processTxBurst()` realiza esencialmente:

```c
now_us = ControlTask_getTimeUs();

if (now_us < next_due)
    return;

RadioIF_transmitRaw(...);

remaining--;

if (remaining == 0)
    clear();

next_due = now_us + interval_us;
```

Esto demuestra que BURST no es una única orden RF entregada al RF Core para cinco paquetes. Es un scheduler software que vuelve a invocar TX cuando vence `next_due`.

---

## 13.3 El evento BURST no se consume después del primer paquete

En:

```text
firmware/cc1352/src/data_task.c
```

la lógica es:

```c
if (TaskEvent_isSet(TASK_EVENT_CONTROL_TX_BURST)) {
    ControlTask_processTxBurst();

    if (!ControlTask_isTxBurstPending()) {
        TaskEvent_clear(TASK_EVENT_CONTROL_TX_BURST);
    }
}
```

Por tanto el evento permanece activo mientras BURST siga pendiente.

### Hipótesis descartada por código

```text
«El event bit se limpia después de la primera transmisión y por eso nunca
se ejecuta la segunda»
```

no corresponde al código examinado.

**Estado: descartada como explicación simple.**

---

# 14. Repetición dentro de RadioIF

Se examinó:

```text
firmware/cc1352/src/radio_if.c
RadioIF_executeTxCommand()
```

Para cada llamada, el helper limpia explícitamente:

```c
fs_cmd->status = 0x0000;
tx_cmd->status = 0x0000;
```

y, cuando ya existe una sesión TX para el mismo PHY, vuelve a ejecutar:

```text
FS
TX
```

mediante `RF_runCmd()`.

Esto es relevante porque debilita dos explicaciones iniciales.

### Hipótesis: estado `status` obsoleto

Una explicación posible era que el segundo TX reutilizara un RF command cuyo `status` permaneciera en estado final de la ejecución previa.

El helper actual limpia ambos `status` antes de cada ejecución.

**Estado: descartada como causa simple en el código actual.**

### Hipótesis: una sesión RF sólo permite el primer TX

El código tiene explícitamente una ruta para reutilizar una sesión del mismo PHY y volver a ejecutar `FS + TX`.

**Estado: debilitada.**

Un fallo del RF Driver durante una ejecución posterior sigue siendo posible, pero no existe evidencia estática de que la API esté diseñada deliberadamente como one-shot.

---

# 15. Error RF tardío como causa posible

`ControlTask_processTxBurst()` contiene:

```c
if (!RadioIF_transmitRaw(...)) {
    ControlTask_clearTxBurstState();
    return;
}
```

Por tanto, si el segundo intento de TX falla dentro de `RadioIF`, todo el BURST se cancela.

A diferencia de `TX_RAW`, esta ruta no genera necesariamente una respuesta asíncrona visible al host que identifique ese fallo.

Así, una secuencia:

```text
TX #1 correcto
TX #2 falla
BURST se limpia silenciosamente
```

es compatible con el diseño.

### Estado actual

**Posible, pero no demostrado.**

Se debilitó durante el análisis porque:

1. `RadioIF_executeTxCommand()` limpia los command status;
2. existe reutilización explícita de sesión del mismo PHY;
3. existen rutas que llaman repetidamente al mismo helper;
4. evidencia histórica de `CONTINUOUS interval=0` consiguió múltiples transmisiones.

No debe descartarse completamente hasta instrumentar o modificar el firmware.

---

# 16. Hipótesis de temporización / scheduler

BURST y CONTINUOUS comparten:

```text
ControlTask_getTimeUs()
next_due_us
interval_us
```

`ControlTask_getTimeUs()` utiliza directamente:

```c
SysTickValueGet()
```

con un wrap asumido de:

```text
0x00FFFFFF
```

y convierte los ciclos a microsegundos.

Durante el análisis se comprobó que el build CMake actual define:

```cmake
option(USE_TIRTOS ... ON)
```

y por defecto selecciona:

```text
src/main_rtos.c
```

en lugar de:

```text
src/main.c
```

Esto importa porque `main.c`, correspondiente a la rama NoRTOS, inicializa explícitamente SysTick con:

```text
SysTickDisable()
SysTickPeriodSet(0x00FFFFFF)
SysTickEnable()
```

mientras que en el `main_rtos.c` inspeccionado no aparece esa inicialización; éste entrega el control al kernel mediante `BIOS_start()`.

Además, TI-RTOS7 tiene su propio `ClockSupport` para CC26xx. En el SDK examinado, `ClockSupport_init()` construye un `Timer`, configura su período mediante `Clock_tickPeriod` y lo expresa en microsegundos. También expone su propio mecanismo para obtener ticks y período del timer.

Esto crea una línea de investigación concreta:

> El scheduler de BURST/CONTINUOUS de FeralRF utiliza directamente SysTick y supuestos de wrap propios, mientras el build TI-RTOS utiliza su infraestructura temporal `Clock/Timer`.

No se terminó de verificar quién inicializa o modifica finalmente SysTick en todas las bibliotecas enlazadas del build, por lo que **todavía no se puede afirmar que ésta sea la causa raíz**.

### Evidencia conductual adicional

En pruebas históricas ya existentes:

```text
CONTINUOUS con interval > 0 → aproximadamente una transmisión observable
CONTINUOUS con interval = 0 → múltiples transmisiones observables
```

Este patrón es particularmente compatible con un problema en el avance/comparación de `next_due_us`:

* con `interval_us = 0`, una nueva transmisión puede permanecer inmediatamente elegible;
* con `interval_us > 0`, el scheduler depende de que su base temporal avance correctamente.

### Estado

**Hipótesis principal abierta para futura investigación.**

No es todavía una corrección validada.

---

# 17. Evaluación de hipótesis para BURST

| Hipótesis                                                   | Estado                                                       | Evidencia                                                                                                                                       |
| ----------------------------------------------------------- | ------------------------------------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------- |
| El host desconecta demasiado pronto                         | **Fuertemente debilitada**                                   | Mantener host abierto 3 s siguió produciendo 1/5; 12 s posteriores sin marcador                                                                 |
| El event bit se consume tras el primer TX                   | **Descartada por código actual**                             | `TASK_EVENT_CONTROL_TX_BURST` sólo se limpia cuando `pending` termina                                                                           |
| `tx_cmd->status` queda stale y bloquea segundo TX           | **Descartada como causa simple**                             | `RadioIF_executeTxCommand()` pone `fs_cmd->status` y `tx_cmd->status` a cero                                                                    |
| La sesión RF no soporta otra transmisión del mismo PHY      | **Debilitada**                                               | El helper reutiliza sesión y vuelve a ejecutar FS + TX                                                                                          |
| El segundo `RadioIF_transmitRaw()` falla y BURST se cancela | **Posible**                                                  | `processTxBurst()` limpia todo el estado ante fallo; no tenemos evidencia runtime del segundo intento                                           |
| El receptor perdió cuatro paquetes                          | **Posible pero poco consistente con el resto de la campaña** | RAW 20/20, RAW inverso 10/10 y FRAME 10/10 con RSSI fuerte; aun así RX count no equivale estrictamente a emitted count                          |
| Front-end/ruta 2.4 GHz incorrectos                          | **No explica específicamente BURST**                         | RAW y FRAME sobre la misma configuración RF funcionaron repetidamente                                                                           |
| Scheduler/timebase de `ControlTask_getTimeUs()`             | **Hipótesis principal abierta**                              | BURST/CONTINUOUS con intervalo dependen de `next_due`; patrón interval 0 vs >0; build TI-RTOS y uso directo de SysTick requieren reconciliación |

---

# 18. Estado final de las funciones evaluadas

| Función                          | Evidencia de control      | Evidencia física        | Resultado         |
| -------------------------------- | ------------------------- | ----------------------- | ----------------- |
| RX IEEE ambiental COM88          | sí                        | 16 paquetes reales      | PASS              |
| RX IEEE ambiental COM33          | sí                        | 15 paquetes reales      | PASS              |
| RAW #1→#2                        | 20/20 ACK                 | 20/20 marcador esperado | **PASS-RF**       |
| RAW #2→#1                        | 10/10 ACK                 | 10/10 marcador esperado | **PASS-RF**       |
| FRAME #1→#2                      | 10/10                     | 10/10 marcador esperado | **PASS-RF**       |
| BURST 5×250 ms                   | ACK / scheduled=5         | 1 marcador              | **PARTIAL**       |
| BURST manteniendo host conectado | ACK                       | 1 marcador; 0 tardíos   | **PARTIAL**       |
| Semántica `count=5` de BURST     | aceptada                  | no demostrada           | **NOT VALIDATED** |
| CONTINUOUS/STOP actual           | no ejecutado en esta fase | —                       | **DEFERRED**      |
| Proprietary/Sub-G OTA            | no ejecutado en esta fase | —                       | **DEFERRED**      |

---

# 19. Conclusión principal de esta fase

La evidencia obtenida permite afirmar:

> **FeralRF sobre CatSniffer V3 transmite físicamente por RF bytes conocidos mediante IEEE 802.15.4 y dichos bytes son recibidos correctamente por un segundo CatSniffer V3. La transmisión RAW quedó demostrada en ambas direcciones y TX_FRAME quedó demostrado en una dirección.**

Por tanto, el camino fundamental:

```text
host
→ FeralRF API
→ Cat-Bridge
→ CC1352P7
→ TI RF Driver / RF Core
→ front-end
→ aire
→ segundo CatSniffer
→ FeralRF RX
→ host
```

está demostrado experimentalmente bajo las condiciones de esta sesión.

Esta conclusión utiliza evidencia RF directa y no depende exclusivamente de ACKs.

---

# 20. Conclusión específica de BURST

No debe clasificarse BURST como completamente funcional.

Tampoco debe afirmarse que BURST no transmite.

La formulación técnicamente correcta es:

> `TX_BURST` aceptó correctamente solicitudes de cinco transmisiones y produjo al menos una emisión RF atribuible, pero sólo se observó una recepción del marcador solicitado. Mantener la sesión host abierta no modificó ese comportamiento y no aparecieron marcadores tardíos. La semántica `count` no quedó validada y el resultado se clasifica como **PARTIAL**.

El comportamiento es reproducible y además coincide con evidencia histórica anterior donde BURST también tendía a producir un único marcador observable.

El análisis estático ha reducido de forma significativa el espacio de hipótesis. Se han debilitado o descartado como explicaciones simples:

```text
host disconnect
event bit one-shot
stale RF command status
imposibilidad básica de reutilizar una sesión RF
front-end incorrecto como explicación específica
```

Quedan principalmente dos familias de causas:

```text
A. fallo posterior dentro de RadioIF/RF Driver que cancela el BURST;
B. defecto del scheduler/timebase compartido por BURST y CONTINUOUS.
```

La evidencia actualmente disponible favorece investigar primero **B**, pero no justifica todavía una modificación de firmware.

---

# 21. Límites de la validación

Esta campaña no demuestra todavía:

* alcance máximo;
* sensibilidad RX;
* potencia radiada/EIRP;
* cumplimiento espectral;
* exactitud temporal real del intervalo BURST;
* número real de emisiones cuando el RX no las observa;
* comportamiento de CONTINUOUS/STOP en la sesión actual;
* funcionamiento OTA de todos los presets declarados;
* interoperabilidad con Zigbee, Wi-SUN, W-MBus, Sidewalk u otros stacks superiores;
* funcionamiento Sub-GHz;
* equivalencia exacta entre el source tree analizado y el binario CC1352 instalado.

Especialmente:

> Un nombre de preset o una función presente en README/API no equivale a una funcionalidad RF validada físicamente.

---

# 22. Evidencia y fuentes que deben conservarse

Para reproducibilidad, deben mantenerse asociadas a esta fase:

```text
FeralRF commit:
0178721cbd4f0d0f6f8eba5ae919ca46066d5dea

Código prioritario:
firmware/cc1352/src/command_processor.c
firmware/cc1352/src/control_task.c
firmware/cc1352/src/data_task.c
firmware/cc1352/src/radio_if.c
firmware/cc1352/src/main.c
firmware/cc1352/src/main_rtos.c
firmware/cc1352/CMakeLists.txt

SDK:
firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/
kernel/tirtos7/packages/ti/sysbios/family/arm/cc26xx/ClockSupport.c

Scripts OTA:
python/examples/lab/ota_rx_probe.py
python/examples/smoke_tx_phase1.py
python/examples/lab/ota_tx_frame.py
python/examples/lab/ota_tx_burst.py
python/examples/smoke_tx_continuous_phase1.py
python/examples/smoke_phy4_ieee154.py
```

Notas metodológicas existentes:

```text
Guía enfocada OTA FeralRF - TX RX con dos CatSniffer
Checklist OTA FeralRF - dos CatSniffer
Registro de validación FeralRF
Auditoría técnica de validación FeralRF - EV ejecutadas
FeralRF - Matriz de pruebas
FeralRF - Wiki técnica integral
```

---

# 23. Estado recomendado al cerrar este chat

Esta fase puede cerrarse con:

```text
IEEE 802.15.4 RAW OTA bidireccional : VALIDATED / PASS-RF
TX_FRAME OTA                        : VALIDATED / PASS-RF
TX_BURST emite al menos una trama   : VALIDATED
TX_BURST count/repetition semantics : PARTIAL / NOT FULLY VALIDATED
Causa raíz de BURST                 : OPEN / DEFERRED
CONTINUOUS + STOP                   : DEFERRED
Sub-G / proprietary presets         : DEFERRED
```

El problema de BURST debe retomarse **durante la fase de implementación y corrección del firmware**, preferentemente mediante análisis dirigido con Codex sobre el source tree y un plan específico de diagnóstico/corrección, en lugar de seguir consumiendo la fase actual de validación.

---

# 24. Punto de reanudación para el siguiente chat

La siguiente sesión no necesita reconstruir la demostración fundamental de TX.

Debe partir de estas conclusiones:

```text
1. Los dos CatSniffer están correctamente asociados a sus puertos.
2. Ambos reciben RF IEEE 802.15.4.
3. RAW funciona OTA en ambas direcciones.
4. FRAME funciona OTA.
5. La ruta 2.4 GHz es funcional bajo las condiciones ensayadas.
6. BURST produce físicamente al menos el primer paquete.
7. BURST no ha demostrado respetar count > 1 con interval_us > 0.
8. El host disconnect fue probado y no explica el problema.
9. La lógica de TaskEvent no es one-shot.
10. RadioIF limpia command status y contempla repetición del mismo PHY.
11. La principal línea pendiente es scheduler/timebase frente a TI-RTOS,
    seguida por un posible fallo RF posterior silencioso.
12. No modificar todavía BURST basándose únicamente en esa hipótesis.
```

Desde ese punto puede continuarse con el plan general de validación, dejando la corrección profunda de BURST para la fase de desarrollo de mejoras.
