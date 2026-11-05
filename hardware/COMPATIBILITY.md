# Limites de compatibilidad fuente de FeralRF

Este documento separa el cableado/configuracion declarados por source de la validacion fisica. `SOURCE_COMPATIBLE` indica que el source revisado respalda la interfaz indicada; no certifica una placa ensamblada ni su rendimiento RF.

## Vocabulario de estados

Estado fuente: `SOURCE_COMPATIBLE`, `CONDITIONALLY_COMPATIBLE`, `SOURCE_INCOMPATIBLE`, `UNKNOWN`.

Estado fisico: `PHYSICALLY_VALIDATED`, `HISTORICAL_TEST_REPORTED`, `NOT PHYSICALLY VALIDATED`, `UNKNOWN`.

## Evaluacion por revision y subsistema

| Fuente de hardware | Subsistema | Estado fuente | Evidencia y condicion | Estado fisico |
|---|---|---|---|---|
| v3.1 | Target MCU | `CONDITIONALLY_COMPATIBLE` | El tag declara P1; FeralRF usa P7 por defecto. La alternativa P1 esta declarada, pero no se ha demostrado que su configuracion generada y dependencias esten completas. Identificar pieza y target. | `HISTORICAL_TEST_REPORTED`; identidad de placa y hashes de binario no disponibles |
| v3.1 | UART TX/RX | `SOURCE_COMPATIBLE` | Nets PCB conectan RP GPIO0/1 a CC DIO12/13; FeralRF configura DIO12/13, 921600 baud. | `HISTORICAL_TEST_REPORTED`; sin atribucion a revision |
| v3.1 | RTS/CTS | `SOURCE_INCOMPATIBLE` | DIO14 esta NC; DIO15 va a BOOT. FeralRF deshabilita flow control. | `NOT PHYSICALLY VALIDATED` |
| v3.1 | Reset/boot | `UNKNOWN` | El PCB declara RP GPIO3 y GPIO15 en `/RESET_CC`; GPIO2 y CC DIO15 en `/BOOT`. El firmware RP y el net compartido siguen sin resolverse. | `NOT PHYSICALLY VALIDATED` |
| v3.1 | Control de antena | `CONDITIONALLY_COMPATIBLE` | CTF1-3 conectan RP GPIO8-10 con U2. El callback CC usa DIO28-30, que no conectan con U2. La compatibilidad del sistema depende del RP. | `HISTORICAL_TEST_REPORTED`; no prueba el estado del switch |
| v3.1 | Rutas 2.4 GHz / Sub-GHz | `CONDITIONALLY_COMPATIBLE` | El hardware de radio y las rutas RF estan declarados; no se establecio el mapeo completo de puertos ni el estado de control RP. | `HISTORICAL_TEST_REPORTED` en algunas configuraciones, no valida la ruta completa |
| v3.2 | Target MCU | `CONDITIONALLY_COMPATIBLE` | El tag declara P1; aplican los mismos limites del target P7 y la evidencia incompleta del target P1. | `HISTORICAL_TEST_REPORTED`; sin atribucion a placa |
| v3.2 | UART TX/RX | `SOURCE_COMPATIBLE` | Los nets criticos consultados coinciden con v3.1 y la configuracion UART de dos hilos de FeralRF. | `HISTORICAL_TEST_REPORTED`; sin atribucion a revision |
| v3.2 | RTS/CTS | `SOURCE_INCOMPATIBLE` | DIO14 NC; DIO15 BOOT. | `NOT PHYSICALLY VALIDATED` |
| v3.2 | Reset/boot | `UNKNOWN` | Persisten el net de reset declarado y el comportamiento RP sin verificar. | `NOT PHYSICALLY VALIDATED` |
| v3.2 | Control de antena | `CONDITIONALLY_COMPATIBLE` | El control CTF esta del lado RP; el callback CC en DIO28-30 no alcanza U2. | `HISTORICAL_TEST_REPORTED`; sin medicion electrica del switch |
| v3.2 | Rutas 2.4 GHz / Sub-GHz | `CONDITIONALLY_COMPATIBLE` | El diseno declara las rutas; la seleccion durante operacion y la ruta completa requieren comprobacion. | `HISTORICAL_TEST_REPORTED`, limitado y sin atribucion a revision |
| Tag v3.3 | Target MCU | `CONDITIONALLY_COMPATIBLE` | El tag declara P1; el esquematico P7 posterior desciende del tag y no forma parte de el. | `NOT PHYSICALLY VALIDATED` |
| Tag v3.3 | UART TX/RX | `SOURCE_COMPATIBLE` | El mapeo de nets consultado coincide con v3.1/v3.2. | `NOT PHYSICALLY VALIDATED` |
| Tag v3.3 | RTS/CTS | `SOURCE_INCOMPATIBLE` | DIO14 NC; DIO15 BOOT. | `NOT PHYSICALLY VALIDATED` |
| Tag v3.3 | Reset/boot | `UNKNOWN` | El PCB declara asignaciones RP; su comportamiento no se verifico. | `NOT PHYSICALLY VALIDATED` |
| Tag v3.3 | Control de antena | `CONDITIONALLY_COMPATIBLE` | El tag conserva RFSW8006 y control CTF desde RP; el callback CC no controla directamente U2. | `NOT PHYSICALLY VALIDATED` |
| Tag v3.3 | Rutas 2.4 GHz / Sub-GHz | `CONDITIONALLY_COMPATIBLE` | El source declara radio y ruteo; no hay una unidad v3.3 disponible. | `NOT PHYSICALLY VALIDATED` |
| Diseno post-tag SKY13317 | Ruteo del switch | `UNKNOWN` | En el PCB posterior CTF2/3 se asignan a RF3/RF2; la compatibilidad depende de la imagen RP y del ruteo RF completo. No es el tag v3.3. | `NOT PHYSICALLY VALIDATED` |

## Limite del target MCU

Los esquematicos de los tags v3.1-v3.3 especifican `CC1352P1F3RGZT`; el CMake actual de FeralRF usa `CC1352P7` por defecto y la configuracion generada revisada corresponde a P7. La opcion alternativa `CC1352P` selecciona un linker/subfamilia P1, pero la configuracion generada orientada a P7 y una ruta de biblioteca driver P7 permanecen en el repositorio. No se hizo un build. La variante instalada en cada placa se desconoce, asi que este documento no prescribe cambiar el target.

## Limite de control RF

`rfDriverCallbackAntennaSwitching()` de FeralRF usa CC DIO28-30 y seleccion de RF Core GPO. Los PCB oficiales revisados conectan el switch externo U2 a RP2040 GPIO8-10. Esto descarta la interpretacion de que el callback CC controla directamente U2; no demuestra que el sistema CatSniffer completo carezca de control del switch. El source RP2040 y los hashes de firmware instalados no estan disponibles en el repositorio FeralRF autorizado.

No clasificar todo el subsistema RF como incompatible solo por el callback CC. Tampoco declararlo plenamente compatible hasta identificar la arquitectura de control y las rutas RF fisicas.

## Evidencia historica y limite Zigbee

La [evidencia de validacion de hardware FeralRF](../docs/HARDWARE_VALIDATION_RESULTS/EXPERIMENTAL_EVIDENCE_REFERENCE.md) documenta reportes historicos de comunicacion IEEE 802.15.4 y propietaria, incluidos payloads observados en el peer. Los reportes describen CatSniffer v3 / CC1352P7, pero no vinculan revisiones exactas de placa y hashes de firmware instalado con suficiente evidencia para validacion por revision. `HISTORICAL_TEST_REPORTED` no equivale a `PHYSICALLY_VALIDATED`.

La recepcion pasiva IEEE 802.15.4 requiere una ruta RF de 2.4 GHz funcional en el CC1352 y el estado correcto del switch de antena. Estas fuentes de hardware solo establecen requisitos de diseno; no validan la ruta de recepcion en cada revision.
