# Informe consolidado de evaluación experimental de FeralRF

## Resumen ejecutivo

FeralRF dispone de evidencia experimental positiva, pero de alcance desigual. Está sólidamente documentado que la cadena PC → Cat-Bridge/RP2040 → CC1352P7 acepta comandos, devuelve identidad y estadísticas, recibe tramas y puede entregar marcadores entre dos CatSniffer. IEEE 802.15.4 cuenta con recepción ambiental repetida y TX/RX dirigido. Sub-GHz cuenta con campañas recientes donde dos equipos intercambiaron paquetes propietarios en FSK/GFSK bajo trece presets y una inversión de roles el 20-oct-2026.

Esto **no demuestra pilas de protocolo completas**. IEEE 802.15.4 no equivale a Zigbee o Thread; `wisun_*`, `sidewalk_*` y `wireless_mbus_*` configuran parámetros PHY y no acreditan MAC, seguridad, asociación o interoperabilidad externa.

El principal fallo reproducible reciente es un timeout host al esperar la confirmación de `RX_STOP` después de recibir tráfico IEEE. RF-01 lo reprodujo dos veces y RF-03 volvió a bloquearse en su primera etapa. Esto bloqueó la validación de cambio de canal y de transición IEEE → Sub-GHz → IEEE; no demuestra que el cambio de PHY sea defectuoso porque esas transiciones no llegaron a ejecutarse.

La historia Sub-GHz es contradictoria pero informativa: existe una afirmación F29 de 70/70 sin transcript crudo; una campaña posterior registró 0/70 para ese subconjunto y 0/240 en 24 corridas; después, controles del 15–16-oct reprodujeron entrega 10/10 y una campaña de trece presets obtuvo 10/10 por preset. No se estableció causa raíz ni regresión. Faltan identidades exactas de los binarios CC1352P7 y equivalencia completa de estados físicos.

## Plataforma observada

La ruta de FeralRF es:

`PC → USB → RP2040/Cat-Bridge → UART 921600 → CC1352P7/FeralRF → RF integrado → front-end → selector U2 → J1/antena`.

Responsabilidades:

- El **RP2040** termina USB, ofrece Cat-Bridge, Cat-LoRa y Cat-Shell, transporta bytes hacia el CC1352P7, controla reset/boot y genera las señales `CTF1..3` del selector RF.
- El **CC1352P7** ejecuta FeralRF, mantiene el estado de radio, configura PHY y procesa RX/TX.
- **Cat-Bridge** es el transporte binario de FeralRF; **Cat-Shell** es el control textual del RP2040. No deben inferirse puertos mediante `Bridge + 2`: COM88/COM87 es un contraejemplo medido.
- El **SX1262/Cat-LoRa** es una ruta separada; no participa en los experimentos FeralRF del CC1352P7.

En firmware RP2040, `band1` selecciona `GIG` y solicita `CTF1/2/3 = 0/1/0`; `band2` selecciona `SUBGIG_1` y solicita `0/0/1`; `band3` corresponde al SX1262 y solicita `1/0/0`. El overlay asigna CTF1/2/3 a GPIO8/9/10. El esquema/notas identifican U2 `RFSW8006QTR7` como selector de tres vías hacia J1.

La respuesta textual y `Band: 0/1` prueban estado lógico del RP2040, no niveles eléctricos en GPIO/CTF ni posición física de U2. Además, seleccionar `band1`/`band2` no configura el PHY del CC1352P7: ambas operaciones son necesarias y pertenecen a componentes distintos.

La evidencia aplica a placas identificadas como CatSniffer v3 con RP2040 + CC1352P7; la nota física registra marcado `CC1352P74`, mientras el esquema histórico v3.1 nombra un P1. Esa discrepancia de revisión debe conservarse.

## Evolución histórica

### Baseline abril y F29 mayo de 2026

`docs/VALIDATION_MATRIX.md` declara un baseline OTA del 8-abr con PHY BLE, IEEE y varios presets propietarios, además de crypto y emulación. Es evidencia documental agregada; los logs crudos no están en el conjunto revisado. La matriz mezcla “validado por control” y “validado OTA” en algunos resúmenes y contiene conclusiones físicas demasiado fuertes, como atribuir OOK 433 a sensibilidad/link budget sin medición preservada.

F29 añadió siete presets Sidewalk/Wi-SUN y el commit `f5863c3` declara 70/70. El script exacto es recuperable, pero no el transcript, las imágenes instaladas, los puertos físicos ni el éxito real de su reset aritmético. Se conserva como **afirmación histórica**, no como registro crudo equivalente a las campañas de octubre.

### EV-00 a EV-14

- EV-00 estableció el mapeo funcional y reveló que la aritmética de puertos era insegura.
- EV-01 carece de una corrida dedicada, aunque `init`, `GET_INFO` y `GET_STATS` aparecen repetidamente en otros ensayos.
- EV-02/EV-05 registraron cinco ventanas IEEE; las tres formales entregaron 41/43/43 paquetes. La evidencia prueba recepción local, no una pila Zigbee.
- EV-03 logró cinco reaperturas entre procesos.
- EV-04 observó una recuperación manual `boot`/`exit`, pero `reset_device()` habría elegido COM90 en vez de COM87; el reset automático quedó no validado en ese mapa.
- EV-06 confirmó rechazo `ERR_INVALID_STATE=0x05` al pedir TX durante RX y recuperación del control.
- EV-10 completó 8/8 secuencias PHY por control con reset entre filas; no es una matriz OTA.
- EV-11 registró 27 presets por control; sólo 18 tienen stdout individual, nueve de 902/915 están resumidos. Los valores `mod_type=4/5/6` que los nombres llaman MSK/4-FSK/4-GFSK son reservados en la estructura TI auditada y no deben presentarse como modulaciones físicas demostradas.
- EV-12 separó aceptación de RAW/FRAME/BURST/CONTINUOUS de observación OTA. RAW y FRAME alcanzaron al peer; BURST y varias variantes CONTINUOUS no cumplieron conteos. `interval_us=0` sí produjo actividad repetida abundante. STOP fue ACK de control; cese físico inmediato no se midió.
- EV-13 obtuvo ACK de CW, PRBS15, PRBS32 y stop; no hubo medición de portadora, patrón, frecuencia, potencia o cese.
- EV-14 estableció recepción normal y una transición mínima BLE→IEEE sin reset, más tres tests host/mocks. No reprodujo el error RF físico buscado.

### Campañas IEEE y propietarias del 6–12 de octubre

Los marcadores IEEE RAW/FRAME se recibieron en ambos sentidos o en la dirección documentada, con controles negativos. BURST 5× y 40× produjo un marcador observado en las configuraciones exigentes. CONTINUOUS a intervalo cero produjo múltiples coincidencias; otras configuraciones produjeron una. Un control dirigido posterior observó actividad A antes de STOP, pero no los RAW B enviados después, con timeout de `RX_STOP`; esto es un fallo del criterio de recepción posterior y causa física inconclusa, no prueba aislada de que STOP haya detenido o corrompido la radio.

La campaña propietaria posterior registró 24 corridas y 240 retornos exitosos de `transmit()`, pero cero paquetes entregados al peer. Cumple `FAIL` del criterio receptor y deja causa física `INCONCLUSIVE`: ACK no prueba radiación y tampoco se midió el switch. No debe llamarse regresión confirmada frente a F29.

### Recuperación y campaña Sub-GHz del 15–16 de octubre

Checkpoint 3A restauró entrega `wisun_915_fsk_50k` a 902,2 MHz con 10/10; 3A-R repitió 10/10. Los controles 3C, 3D, 3E y 4A también obtuvieron 10/10 sin reset por ejecución, con `band2`, con `band1`, usando procedimiento legado y después de power-cycle, respectivamente. El éxito bajo `Band: 0` demuestra que ese estado lógico no fue necesario para el enlace cercano observado; no verifica la posición eléctrica de U2.

La campaña extendida ejecutó 24 intentos E01–E24. Trece presets nuevos lograron 10/10; hubo cinco corridas bloqueadas por timeout/puerto/reset inducido y controles posteriores satisfactorios. Se demostró entrega de paquetes FeralRF compatibles entre dos CatSniffer para FSK/GFSK en 868–915/902,2 MHz, no interoperabilidad Wi-SUN, Sidewalk o Wireless M-Bus.

### Campaña RF del 20 de octubre

- **RF-01 — BLOCKED.** Dos corridas recibieron 10/10 marcadores IEEE canal 25 y fallaron durante `RX_STOP`. Nunca se llegó al cambio 25→20→25.
- **RF-02 — PASS funcional con excepción metodológica.** Dirección CatSniffer #3→#2, `wisun_915_fsk_50k`, 902,2 MHz solicitados: 10 ACK, 10 marcadores, stop y cleanup correctos. Los paquetes reportaron `channel: 25` y RSSI entre valores positivos y negativos incoherentes; frecuencia/potencia no se midieron.
- **RF-03 — BLOCKED.** La primera etapa IEEE recibió 10/10, pero `RX_STOP` falló y `completed_stages=[]`; no se ejecutó la transición Sub-GHz.
- **RF-04 — BLOCKED.** `pip` no resolvió `killerbee>=3.0.0`; no se abrió hardware, no hubo captura ni PCAP. Es un bloqueo de entorno, no fallo del adaptador.
- **RF-05 — NOT RUN.** Era opcional y exigía RF-01..03 PASS, recuperación/power-cycle y aprobación inmediata; esas condiciones no se cumplieron.

## Hallazgos positivos reproducibles

1. Reapertura, inicialización, información y estadísticas funcionan repetidamente en los entornos registrados.
2. IEEE 802.15.4 canal 25 se recibe de fuentes ambientales y de un peer con marcador.
3. TX RAW/FRAME IEEE alcanza otro CatSniffer en condiciones controladas.
4. La API impide TX durante RX con error de estado y permite recuperar el control.
5. Sub-GHz propietario ha entregado marcadores de peer en 902,2, 915, 868, 868,3 y 868,95 MHz configurados, bajo FSK/GFSK solicitados.
6. La inversión de dirección del 20-oct obtuvo 10/10 y cleanup correcto.
7. La campaña 15–16-oct demostró recuperación tras incidentes y cambio de presets dentro de Sub-GHz.

## Fallos, restricciones y resultados parciales

- `RX_STOP` presenta timeout reproducible cuando hay respuestas `0x90` inesperadas en cola en determinadas sesiones IEEE. Sólo se confirma timeout de correlación host; no se ha demostrado que la radio siga recibiendo físicamente.
- BURST/CONTINUOUS no ofrece evidencia consistente de conteo uno a uno. El receptor compartía implementación y no hubo contador independiente de emisiones.
- `TX_STOP` tiene ACK, pero el instante de cese por aire no fue medido.
- La exactitud de `rssi_dbm`, `channel`, frecuencia, potencia, timestamps y LQI no está caracterizada de forma independiente.
- El selector de banda se conoce documentalmente y por respuesta lógica, no por medición eléctrica.
- Los binarios CC1352P7 instalados no están vinculados a commits/hashes; `1.0.0` es genérico.
- La integración KillerBee y OOK reciente no tienen validación funcional completada.
- 169,45 MHz queda fuera del alcance defendible del hardware/CC1352P74 documentado; 433 MHz requiere hardware/antena adecuados y evidencia nueva.

## Conclusión defendible

FeralRF puede describirse como una API/firmware de control RF con evidencia de recepción y transferencia raw entre dos CatSniffer en IEEE 802.15.4 y varias configuraciones propietarias. No debe describirse, con esta evidencia, como implementación experimentalmente validada de Zigbee, Thread, Wi-SUN, Sidewalk o Wireless M-Bus. Su uso actual requiere control explícito de puertos y banda, marcadores de peer, cleanup tolerante a fallos y conservación de versiones. La robustez de stop/transiciones, exactitud de metadatos y equivalencia entre configuración y señal física siguen abiertas.

