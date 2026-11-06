# Documentación de FeralRF

Este archivo es el punto de entrada para la documentación del proyecto. Su función es indicar qué fuente consultar y qué autoridad tiene; no sustituye los documentos enlazados.

## Roles documentales

- `CANONICAL`: describe el contrato o la arquitectura vigente de su área.
- `SUPPORTING`: amplía una fuente canónica sin reemplazarla.
- `OPERATIONAL_DRAFT`: material de trabajo para procedimientos todavía no canónicos.
- `HISTORICAL`: conserva el contexto o resultado de una ejecución o baseline fechado.
- `RESEARCH`: investigación técnica que informa decisiones, pero no define el comportamiento vigente.
- `PROPOSAL`: cambio o arquitectura futura todavía no adoptada.

## Arquitectura de software

[`ARCHITECTURE.md`](ARCHITECTURE.md) es la fuente `CANONICAL` para las capas de software, los componentes del host y firmware, y el flujo de control y datos. Delega pinout, revisiones y compatibilidad física a la documentación de hardware.

## Hardware

[`../hardware/README.md`](../hardware/README.md) y los documentos enlazados desde allí son la fuente `CANONICAL` para topología física, pinout, revisiones, compatibilidad, procedencia de fuentes y preguntas abiertas de hardware.

El target de software, una configuración RF o un reporte experimental no establecen por sí solos qué MCU, switch o revisión está físicamente poblada en una unidad.

## Protocolo wire

[`protocol.md`](protocol.md) es la fuente `CANONICAL` para el protocolo binario entre la biblioteca host y el firmware FeralRF del CC1352, transportado mediante Cat-Bridge. Incluye framing, comandos, respuestas y reglas de compatibilidad.

## API Python

[`PYTHON_API.md`](PYTHON_API.md) es la referencia `CANONICAL` de la API pública prevista del paquete `feralrf`. Si la documentación y el código difieren, los exports de `python/feralrf/__init__.py` y la implementación pública actual determinan qué existe; la discrepancia debe corregirse en la documentación o resolverse como decisión de API.

## Validación

[`CURRENT_VALIDATION_MATRIX.md`](CURRENT_VALIDATION_MATRIX.md) es la referencia `CANONICAL` del estado actual defendible de implementación, control, comportamiento funcional/OTA, validación física, nivel protocolario, interoperabilidad y fuerza de evidencia.

[`VALIDATION_MATRIX.md`](VALIDATION_MATRIX.md) es un baseline `HISTORICAL` de abril de 2026 y fases posteriores. Ya no es la matriz autoritativa del estado actual y sus resultados originales no deben reinterpretarse silenciosamente.

- [`User-Manual/02 - Matriz histórica de experimentos y ejecuciones.md`](<User-Manual/02 - Matriz histórica de experimentos y ejecuciones.md>) es el inventario histórico de ejecuciones y lookup de trazabilidad (`SUPPORTING`).
- [`Presets_Reports/`](Presets_Reports/) contiene el análisis consolidado de implementación, presets y gaps que sustenta la matriz actual (`SUPPORTING`).
- [`HARDWARE_VALIDATION_RESULTS/`](HARDWARE_VALIDATION_RESULTS/) conserva evidencia primaria y supporting de campañas (`HISTORICAL`), además de algunos análisis y propuestas fechados.
- [`User-Manual/`](User-Manual/) conserva síntesis de evidencia y drafts operativos (`SUPPORTING`/`HISTORICAL`) usados para construir el Runbook. Ya no es la entrada operacional canónica.

## Procedimientos de validación

[`validation/runbook/README.md`](validation/runbook/README.md) es la entrada `CANONICAL` para ejecutar validaciones. Clasifica cada procedimiento por ejecutabilidad actual, enlaza scripts/API verificados, define criterios, cleanup, recovery y el registro mínimo de evidencia.

La matriz canónica responde **qué** es defendible; el Runbook responde **cómo** ensayarlo. Los procedimientos preservados en [`User-Manual/`](User-Manual/) siguen disponibles como fuentes históricas/supporting y no deben sustituir al Runbook.

## Integraciones

[`killerbee-catsniffer.md`](killerbee-catsniffer.md) y [`TESTING-ON-LINUX.md`](TESTING-ON-LINUX.md) documentan trabajo de integración con KillerBee. Son documentación `SUPPORTING` de una integración experimental y no una garantía de validación completa de extremo a extremo.

## Investigación y propuestas

- [`Z-Stack_Reports/`](Z-Stack_Reports/) contiene investigación y propuestas para trabajo Zigbee/Z-Stack futuro.
- [`FERALRF_NATIVE_ZIGBEE_CAPABILITY_ASSESSMENT.md`](FERALRF_NATIVE_ZIGBEE_CAPABILITY_ASSESSMENT.md) es un assessment `RESEARCH`.
- [`FERALRF_DEEP_CODE_IMPLEMENTATION_AUDIT_2026-10-27.md`](FERALRF_DEEP_CODE_IMPLEMENTATION_AUDIT_2026-10-27.md) es una auditoría `RESEARCH` fechada.

Estos documentos no demuestran que Zigbee nativo u otras capacidades propuestas estén implementadas.

## Evidencia histórica

Los registros bajo [`HARDWARE_VALIDATION_RESULTS/`](HARDWARE_VALIDATION_RESULTS/) describen lo ocurrido en ejecuciones concretas y deben conservarse con su procedencia. El nombre del directorio es más estrecho que su contenido: también incluye evidencia de control/API, matrices, análisis de readiness y propuestas fechadas. No es la autoridad de hardware actual.

## Resolución de conflictos

Cuando dos documentos discrepan:

1. El código fuente actual determina el comportamiento implementado del software, pero no prueba comportamiento ni población física.
2. La [documentación canónica de hardware](../hardware/README.md) gobierna las afirmaciones de diseño, pinout, revisiones y compatibilidad de hardware, con los límites de evidencia que declara.
3. [`protocol.md`](protocol.md) y [`PYTHON_API.md`](PYTHON_API.md) describen los contratos públicos previstos; cualquier diferencia con la implementación es una discrepancia que debe resolverse explícitamente.
4. Los registros experimentales describen una ejecución concreta bajo sus condiciones y no sustituyen los contratos actuales.
5. Investigación y propuestas informan decisiones futuras y no reemplazan la implementación ni la documentación canónica vigente.
