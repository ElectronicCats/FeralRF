# Límites de compatibilidad del código fuente de FeralRF

Este documento separa el cableado y la configuración declarados por el código fuente de la validación física. `SOURCE_COMPATIBLE` indica que el código fuente revisado respalda la interfaz indicada; no certifica una placa ensamblada ni su rendimiento RF.

## Vocabulario de estados

Estado del código fuente: `SOURCE_COMPATIBLE`, `CONDITIONALLY_COMPATIBLE`, `SOURCE_INCOMPATIBLE`, `UNKNOWN`.

Estado físico: `PHYSICALLY_VALIDATED`, `HISTORICAL_TEST_REPORTED`, `NOT_PHYSICALLY_VALIDATED`, `UNKNOWN`.

## Evaluación por revisión y subsistema

| Fuente de hardware | Subsistema | Estado fuente | Evidencia y condición | Estado físico |
|---|---|---|---|---|
| v3.1 | Target MCU | `CONDITIONALLY_COMPATIBLE` | El tag declara P1; FeralRF usa P7 por defecto. La alternativa P1 está declarada, pero no se ha demostrado que su configuración generada y dependencias estén completas. Identificar la pieza y el target. | `HISTORICAL_TEST_REPORTED`; identidad de placa y hashes de binario no disponibles |
| v3.1 | UART TX/RX | `SOURCE_COMPATIBLE` | Los nets del PCB conectan RP GPIO0/1 a CC DIO12/13; FeralRF configura DIO12/13, 921600 baud. | `HISTORICAL_TEST_REPORTED`; sin atribución a revisión |
| v3.1 | RTS/CTS | `SOURCE_INCOMPATIBLE` | DIO14 está NC; DIO15 va a BOOT. FeralRF deshabilita flow control. | `NOT_PHYSICALLY_VALIDATED` |
| v3.1 | Reset/boot | `UNKNOWN` | El PCB declara RP GPIO3 y GPIO15 en `/RESET_CC`; GPIO2 y CC DIO15 en `/BOOT`. El firmware RP y el net compartido siguen sin resolverse. | `NOT_PHYSICALLY_VALIDATED` |
| v3.1 | Control de antena | `CONDITIONALLY_COMPATIBLE` | CTF1-3 conectan RP GPIO8-10 con U2. El callback CC usa DIO28-30, que no conectan con U2. La compatibilidad del sistema depende del RP. | `HISTORICAL_TEST_REPORTED`; no prueba el estado del switch |
| v3.1 | Rutas 2.4 GHz / Sub-GHz | `CONDITIONALLY_COMPATIBLE` | El hardware de radio y las rutas RF están declarados; no se estableció el mapeo completo de puertos ni el estado de control RP. | `HISTORICAL_TEST_REPORTED` en algunas configuraciones, no valida la ruta completa |
| v3.2 | Target MCU | `CONDITIONALLY_COMPATIBLE` | El tag declara P1; aplican los mismos límites del target P7 y la evidencia incompleta del target P1. | `HISTORICAL_TEST_REPORTED`; sin atribución a placa |
| v3.2 | UART TX/RX | `SOURCE_COMPATIBLE` | Los nets críticos consultados coinciden con v3.1 y la configuración UART de dos hilos de FeralRF. | `HISTORICAL_TEST_REPORTED`; sin atribución a revisión |
| v3.2 | RTS/CTS | `SOURCE_INCOMPATIBLE` | DIO14 NC; DIO15 BOOT. | `NOT_PHYSICALLY_VALIDATED` |
| v3.2 | Reset/boot | `UNKNOWN` | Persisten el net de reset declarado y el comportamiento RP sin verificar. | `NOT_PHYSICALLY_VALIDATED` |
| v3.2 | Control de antena | `CONDITIONALLY_COMPATIBLE` | El control CTF está del lado RP; el callback CC en DIO28-30 no alcanza U2. | `HISTORICAL_TEST_REPORTED`; sin medición eléctrica del switch |
| v3.2 | Rutas 2.4 GHz / Sub-GHz | `CONDITIONALLY_COMPATIBLE` | El diseño declara las rutas; la selección durante operación y la ruta completa requieren comprobación. | `HISTORICAL_TEST_REPORTED`, limitado y sin atribución a revisión |
| Tag v3.3 | Target MCU | `CONDITIONALLY_COMPATIBLE` | El tag declara P1; el esquemático P7 posterior desciende del tag y no forma parte de él. | `NOT_PHYSICALLY_VALIDATED` |
| Tag v3.3 | UART TX/RX | `SOURCE_COMPATIBLE` | El mapeo de nets consultado coincide con v3.1/v3.2. | `NOT_PHYSICALLY_VALIDATED` |
| Tag v3.3 | RTS/CTS | `SOURCE_INCOMPATIBLE` | DIO14 NC; DIO15 BOOT. | `NOT_PHYSICALLY_VALIDATED` |
| Tag v3.3 | Reset/boot | `UNKNOWN` | El PCB declara asignaciones RP; su comportamiento no se verificó. | `NOT_PHYSICALLY_VALIDATED` |
| Tag v3.3 | Control de antena | `CONDITIONALLY_COMPATIBLE` | El tag conserva RFSW8006 y control CTF desde RP; el callback CC no controla directamente U2. | `NOT_PHYSICALLY_VALIDATED` |
| Tag v3.3 | Rutas 2.4 GHz / Sub-GHz | `CONDITIONALLY_COMPATIBLE` | El código fuente declara radio y ruteo; no hay una unidad v3.3 disponible. | `NOT_PHYSICALLY_VALIDATED` |
| Diseño post-tag SKY13317 | Ruteo del switch | `UNKNOWN` | En el PCB posterior CTF2/3 se asignan a RF3/RF2; la compatibilidad depende de la imagen RP y del ruteo RF completo. No es el tag v3.3. | `NOT_PHYSICALLY_VALIDATED` |

## Límite del target MCU

Los esquemáticos de los tags v3.1-v3.3 especifican `CC1352P1F3RGZT`; el CMake actual de FeralRF usa `CC1352P7` por defecto y la configuración generada revisada corresponde a P7. La opción alternativa `CC1352P` selecciona un linker/subfamilia P1, pero la configuración generada orientada a P7 y una ruta de biblioteca driver P7 permanecen en el repositorio. No se hizo un build. La variante instalada en cada placa se desconoce, así que este documento no prescribe cambiar el target.

## Límite de control RF

`rfDriverCallbackAntennaSwitching()` de FeralRF usa CC DIO28-30 y selección de RF Core GPO. Los PCB oficiales revisados conectan el switch externo U2 a RP2040 GPIO8-10. Esto descarta la interpretación de que el callback CC controla directamente U2; no demuestra que el sistema CatSniffer completo carezca de control del switch. El código fuente RP2040 y los hashes de firmware instalados no están disponibles en el repositorio FeralRF autorizado.

No clasificar todo el subsistema RF como incompatible solo por el callback CC. Tampoco declararlo plenamente compatible hasta identificar la arquitectura de control y las rutas RF físicas.

## Evidencia histórica y límite Zigbee

La [evidencia de validación de hardware FeralRF](../docs/HARDWARE_VALIDATION_RESULTS/EXPERIMENTAL_EVIDENCE_REFERENCE.md) documenta reportes históricos de comunicación IEEE 802.15.4 y propietaria, incluidos payloads observados en el peer. Los reportes describen CatSniffer v3 / CC1352P7, pero no vinculan revisiones exactas de placa y hashes de firmware instalado con suficiente evidencia para validación por revisión. `HISTORICAL_TEST_REPORTED` no equivale a `PHYSICALLY_VALIDATED`.

La recepción pasiva IEEE 802.15.4 requiere una ruta RF de 2.4 GHz funcional en el CC1352 y el estado correcto del switch de antena. Estas fuentes de hardware solo establecen requisitos de diseño; no validan la ruta de recepción en cada revisión.
