# Capacidades demostradas y catálogo de presets

## Matriz B — capacidad frente a evidencia

| Capacidad | Declaración/interfaz | Evidencia positiva | Evidencia negativa o bloqueo | Evaluación actual |
|---|---|---|---|---|
| Abrir, inicializar e identificar | `Radio`, `init`, `get_stats` | Muchas campañas; EV-03 5/5 | Serial lógico `FERALRF1` no distingue placas | DEMOSTRADA por control, no procedencia binaria |
| Reabrir entre procesos | `disconnect`/nuevo `Radio` | EV-03 5/5 | No Ctrl+C, USB loss o misma instancia | PASS acotado |
| Reset/recovery manual | Cat-Shell `boot`/`exit` | EV-04 1/1 y campañas posteriores | `reset_device()` deriva puerto aritmético | Manual explícito demostrado; API automática no generalizable |
| Selección de ruta RF | Cat-Shell `band1`/`band2` | Respuestas y estado lógico; firmware RP2040 trazado | 3D funcionó con `Band:0`; sin medición CTF/U2 | Control lógico documentado; posición física no verificada |
| Selección/configuración PHY | `set_phy`, `set_channel`, `configure_prop` | EV-10 8/8; EV-11 | ACK puede preceder fallo async/backend | Demostrada por control; efectiva por RF sólo en subconjuntos OTA |
| RX IEEE 802.15.4 | `start_rx`, `read_packets` | EV-05; baselines; RF-01/RF-03 | `RX_STOP` intermitente/reproducible | RX demostrada en canal 25; robustez de stop no |
| TX IEEE RAW/FRAME | `transmit`, `transmit_frame` | Marcadores de peer en ambos sentidos/campañas | ACK solo no basta | Demostrada bajo canal 25 y bancos registrados |
| BURST | `transmit_burst` | Un marcador observado y ACK | Umbrales 5/40 no cumplidos | PARTIAL; repetición/conteo físico INCONCLUSIVE |
| CONTINUOUS | `transmit_continuous` | Múltiples hits con intervalo 0 | Un hit en otras variantes; stop/correlación problemática | PARTIAL y dependiente de configuración |
| TX/RX propietario Sub-GHz | `PROPRIETARY_GFSK`, presets | 60/60 recovery; 13 presets 10/10; RF-02 10/10 reverso | Campaña previa 0/240 | Demostrada entre dos CatSniffer bajo condiciones estrechas; causa histórica abierta |
| Switching de PHY/banda sin reset | `set_phy` + band Shell | EV-14 BLE→IEEE 3/3 narrado; cambios dentro de Sub-GHz | RF-03 no llegó a transición | Parcial; no validación general IEEE↔Sub-GHz |
| CW/PRBS | `tx_cw`, `tx_prbs`, `tx_test_stop` | ACK EV-13 | Sin analizador/espectro | Control demostrado; señal física no validada |
| Errores async | `RxStreamError` | Tres tests FakeSerial; eventos registrados | Error RF físico no inducido | Contrato host parcial; HIL pendiente |
| KillerBee | `KillerBeeFeralRF` y ejemplo | Código/docs y tests host | RF-04 bloqueado por dependencia | No validación HIL/captura actual |
| OOK | preset/API; claims históricos | Matriz histórica dice 10/10 a 868 | Logs ausentes; RF-05 no ejecutado; requiere power-cycle | No reproducido con evidencia reciente suficiente |
| Crypto/emulación | API y matriz | Claims 9/9 y 20/20 | Fuera del corpus crudo revisado | Documentado, requiere auditoría específica si es material |

## Cinco niveles que no deben confundirse

1. El preset existe en Python.
2. El firmware responde ACK a la configuración.
3. Dos CatSniffer intercambian un paquete raw compatible.
4. Se procesa framing/MAC de un protocolo.
5. Se demuestra interoperabilidad con una implementación externa.

La evidencia reciente llega al nivel 3 para varios presets FSK/GFSK. No llega a 4–5 para Wi-SUN, Sidewalk o Wireless M-Bus.

## Catálogo compacto de presets actuales

Los parámetros provienen de `python/feralrf/presets.py` en el árbol local. `deviation` y `rx_bw` son valores de configuración, no medidas físicas. `mod_type=4/5/6` es reservado en la estructura TI auditada; el nombre del preset no valida MSK/4-FSK/4-GFSK.

| Preset/familia | Frecuencia configurada | `mod_type` | Cobertura histórica | Evaluación |
|---|---:|---:|---|---|
| `gfsk_433_50k`, `gfsk_433_10k` | 433,92 MHz | 1 | Control EV-11; claim histórico marginal | RF actual no validada; antena/hardware condicionan |
| `fsk_433_50k` | 433,92 MHz | 0 | Control; claim 1/10 | No validado funcionalmente |
| `ook_433_*` | 433,92 MHz | 2 | Claim 0/10 | No ejecutar sin plan aislado; no validado |
| `msk_433_50k`, `4fsk_433_50k`, `4gfsk_433_50k` | 433,92 MHz | 4/5/6 | Control | Nombre físico no sustentado; valores reservados |
| `gfsk_868_50k`, `gfsk_868_100k` | 868 MHz | 1 | 10/10 en campaña 15–16-oct | PASS de paquetes raw entre CatSniffer |
| `wireless_mbus_s_868` | 868,3 MHz | 1 | 10/10 | PHY raw; no EN 13757/interoperabilidad |
| `wireless_mbus_t_868`, `wireless_mbus_c_868` | 868,95 MHz | 1 | 10/10 | PHY raw; no stack W-MBus |
| `ook_868_4k8` | 868 MHz | 2 | Claim histórico 10/10; RF-05 NOT RUN | Requiere power-cycle; sin validación reciente |
| `msk_868_50k`, `4fsk_868_50k`, `4gfsk_868_50k` | 868 MHz | 4/5/6 | Control y claims contradictorios | Modulación física no demostrada |
| `wireless_mbus_n_169_*` | 169,45 MHz | 1 | Control solamente | Frecuencia fuera del alcance defendible del CC1352P74/U4 documentado |
| `gfsk_915_50k` | 915 MHz | 1 | 10/10 el 15-oct | PASS raw entre dos CatSniffer |
| `gfsk_902_50k` | 902,2 MHz | 1 | 10/10 | PASS raw |
| `sidewalk_915_fsk_50k`, `_250k` | 915 MHz | 0 | 10/10 cada uno | Sólo capa FSK raw; no Sidewalk; LR/LoRa fuera del CC1352 |
| `wisun_915_fsk_50k` | **902,2 MHz** | 0 | Recovery 60/60 y RF-02 10/10 | PASS raw estrecho; no Wi-SUN |
| `wisun_915_fsk_100k/150k/200k/300k` | **902,2 MHz** | 0 | 10/10 cada uno | PASS raw; no FAN/RPL/seguridad |
| `gfsk_2440_50k`, `_250k` | 2440 MHz | 1 | Control; octubre 0 hits | Criterio receptor falló; causa física inconclusa |
| `mioty_868_tsunb` | 868 MHz | 0, 396 baud | 0/10 histórico y marcado pending | No soporte nativo demostrado; posible patch requerido |

## IEEE 802.15.4, Zigbee y Thread

FeralRF ha recibido y transmitido frames bajo el PHY IEEE 802.15.4. Esto no demuestra asociación a una PAN, NWK/APS/ZCL, commissioning, cifrado Zigbee o stack Thread/6LoWPAN. La integración KillerBee, si se habilita, pretende exponer captura/inyección raw; RF-04 no llegó a ejecutarla.

## Ejemplo práctico: “quiero transmitir protocolo X con GFSK cerca de 915 MHz”

Primero se debe preguntar qué nivel se necesita:

- Si basta un **paquete raw compatible entre dos FeralRF**, `gfsk_915_50k` tiene evidencia positiva.
- Si se requiere una **trama del protocolo X**, debe existir un encoder/framing verificable; el nombre del preset no lo aporta.
- Si se requiere **asociación, seguridad o interoperabilidad**, hace falta una pila y un peer externo; no está demostrado por estas campañas.
- Si se requiere confirmar **915,000 MHz y potencia**, hace falta instrumentación. La configuración solicitada no es una medición.

## Gaps que requieren Fase II

- Semántica real de los valores propietarios, especialmente `mod_type=4/5/6`, `deviation`, `rx_bw` y `sync_word`.
- Implementación superior asociada —si existe— a nombres Wi-SUN, Sidewalk, W-MBus y MIOTY.
- Estado estable/experimental exacto de cada método y divergencias entre `__init__.py`, `PYTHON_API.md` y evidencia.
- Comportamiento de colas y correlación durante `RX_STOP`, BURST y CONTINUOUS.

