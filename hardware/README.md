# Referencia de hardware para FeralRF

Este directorio documenta las interfaces CatSniffer relevantes para FeralRF. Es una referencia enfocada; no sustituye ni copia el repositorio completo de hardware de [Electronic Cats CatSniffer](https://github.com/ElectronicCats/CatSniffer).

## Alcance

La comparacion cubre los tags inmutables CatSniffer [v3.1](https://github.com/ElectronicCats/CatSniffer/tree/5b99d984933a8b8c36e789853c08d3a2a785bed4/hardware), [v3.2](https://github.com/ElectronicCats/CatSniffer/tree/fbcccd86e84e3fa853bcae02c5938db822e76ac4/hardware) y [v3.3](https://github.com/ElectronicCats/CatSniffer/tree/6da5050fefde59138fc11d9e2ecf7c0de36f3a22/hardware). Los cambios upstream posteriores, como el esquematico P7 y el switch SKY13317, se identifican por separado; no se atribuyen al tag v3.3.

## Navegacion

- [PINOUT.md](PINOUT.md) - conexiones declaradas por PCB y pines de firmware.
- [ARCHITECTURE.md](ARCHITECTURE.md) - responsabilidades de procesadores y rutas de control.
- [HARDWARE_REVISIONS.md](HARDWARE_REVISIONS.md) - tags de release y cambios posteriores.
- [COMPATIBILITY.md](COMPATIBILITY.md) - limites de compatibilidad fuente y validacion fisica.
- [HARDWARE_SOURCES.md](HARDWARE_SOURCES.md) - fuentes inmutables y procedencia de archivos locales.
- [OPEN_HARDWARE_QUESTIONS.md](OPEN_HARDWARE_QUESTIONS.md) - preguntas pendientes y evidencia necesaria.

## Responsabilidad de los componentes

El RP2040 proporciona el puente USB y esta conectado electricamente a los controles del switch externo de antena. El CC1352 ejecuta el firmware de radio FeralRF y controla su RF Core. El diseno CatSniffer tambien contiene un radio SX1262 y su propia ruta RF; el comportamiento en ejecucion de ese componente no se establece en el codigo FeralRF del CC1352 revisado aqui. La seleccion de matching, switch y antena depende de rutas de la placa que no se pueden inferir solo a partir de un comando de radio del CC1352.

## Fuente de verdad y limites

Los esquematicos y PCB oficiales se mantienen en el repositorio upstream. Las copias actuales de este directorio no forman un conjunto verificado de una sola release: el blob PCB coincide con v3.1 aunque su title block dice v3.2; el esquematico local tiene contenido propio y title block revision 2.0; otros archivos coinciden con varios tags o tienen procedencia incierta. Ver [HARDWARE_SOURCES.md](HARDWARE_SOURCES.md).

La compatibilidad depende del MCU y switch ensamblados, de la revision real del PCB y de los firmwares RP2040 y CC1352 en conjunto. La inspeccion fuente confirma conexiones declaradas, no la poblacion fisica ni el rendimiento RF. Los reportes historicos se identifican por separado y no validan una revision fisica especifica.
