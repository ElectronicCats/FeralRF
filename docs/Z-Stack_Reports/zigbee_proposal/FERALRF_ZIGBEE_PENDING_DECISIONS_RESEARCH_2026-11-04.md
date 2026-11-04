# Investigación complementaria — decisiones arquitectónicas y normativas Zigbee para FeralRF

**Fecha:** 2026-11-04  
**Tipo:** consolidación técnica propuesta + investigación de fuentes oficiales; no es implementación ni validación experimental.  
**Registro separado de fuentes:** `FERALRF_ZIGBEE_DECISION_RESEARCH_SOURCES_2026-11-04.md`.  
**Destino sugerido en repositorio, sujeto a revisión:** `docs/Z-Stack_Reports/` (no se ha modificado el repositorio del usuario).

## 1. Dictamen ejecutivo

La dirección arquitectónica de la propuesta 1.1 —decoder Zigbee pasivo, **opt-in**, en Python sobre `Packet` sin nuevo firmware— sigue siendo razonable. La revisión crítica interna identifica correctamente los límites de datos, seguridad y proveniencia [INT-01–03]. Esta investigación ofrece una definición concreta de `R-ID-01` que **evita un segundo parser NWK**, un modelo de evidencias para `CaptureContext`, una justificación documental de `0x0104`, el perfil normativo R22/ZCL7/IEEE **802.15.4-2015** y una política cautelar de licencias.

**Nueva evidencia normativa esencial:** ya se identificaron, en fuentes oficiales/históricas de Zigbee Alliance, el documento **R22 1.0 `05-3474-22` (19-abr-2017)** y **ZCL Revision 7 `07-5123-07` (feb-2018)**. El propio **R22 §§1.5.1 [B1] y Annex D §D.1** identifica **IEEE 802.15.4-2015**. Esto actualiza el estado «revisión IEEE exacta desconocida» de la propuesta 1.1: **hay evidencia primaria identificada para R22**, aunque faltan adquisición/control documental y evaluación de cobertura/capturas [NORM-01–04, 07, 10].

**Riesgo funcional prioritario:** Z-Stack/Zigbee 3.0 utiliza protección NWK obligatoria para tráfico normal. Si se mantiene «sin descifrado» en el baseline, los decoders de APS/ZDO/ZCL podrán probarse con fixtures autorizados de plaintext y observar algunas situaciones particulares, pero **no debe prometerse decodificación ZCL habitual en redes Zigbee 3.0 reales** a partir de RF raw [NORM-11–12]. Esto exige una decisión explícita de expectativas, no un rediseño automático.

**Riesgo de propiedad intelectual:** Z-Stack **no es enteramente software de uso irrestricto**. Su manifest oficial para SDK 8.30.01.01 enumera **BSD-3-Clause** para ciertos archivos fuente y **TI Text File License** para binarios/fuente limitada, con condiciones TI Devices [LEG-01–03]. No copiar componentes restrictivos a la biblioteca Python, no presumir que un repositorio público concede autorización y no mezclar Scapy GPL-2.0-only en código GPL-3.0 sin evaluación. El baseline preferido de implementación independiente y cita de normas es correcto, **pero no reemplaza** el examen de derechos sobre el texto normativo ni de patentes/marcas [LEG-04–10].

## 2. Niveles de certeza

| Etiqueta | Significado |
|---|---|
| **EVIDENCIA DE CÓDIGO REPORTADA** | Ruta/símbolo y comportamiento consignados por auditoría del checkout; no reabiertos directamente aquí. |
| **VERIFICADO EN FUENTE WEB PRIMARIA** | Documento de organización editora o del proveedor identificado, versión y sección. |
| **PROPUESTA DE DISEÑO** | Política nueva que no existe en firmware/Python. |
| **CONDICIONAL** | Depende de perfil, especificación, fixture, licencia o aprobación. |
| **NO VALIDADO** | Sin código del decoder ni pruebas RF/PCAP de esa capacidad. |

## 3. `R-ID-01`: un parser NWK, no dos

### 3.1 Evidencia y problema

El backend de FeralRF recibe IEEE 802.15.4 genérico y no expone una marca per-packet que identifique Zigbee [INT-02, §4, §10; INT-03, `FW-H01`, `FW-F05`]. Un encabezado MAC válido no implica Zigbee; interpretar un NWK aparente tampoco prueba procedencia. El requisito `R-ID-01` de la propuesta 1.1 separa `confirmed_by_context`, `structurally_consistent`, `ambiguous`, `invalid_for_profile` y `not_eligible`, pero su flujo dibuja al clasificador **antes** del parser NWK y luego exige que compruebe estructura NWK [INT-01, §§7.2–8]. Es una división de responsabilidades potencialmente duplicada.

### 3.2 Contrato recomendado

**Decisión propuesta:** separar la *elegibilidad de intentar un parse* de la *clasificación de origen/protocolo*. **Sólo habrá una implementación de lectura de cabecera NWK** (`parse_nwk_header_bounded` conceptual, nombre no comprometido). El clasificador usa su resultado inmutable; no vuelve a leer ni decodificar la cabecera.

```text
Packet + CaptureContext
  -> CaptureAdapter -> RawCapture
  -> MAC bounded parse -> MacResult
  -> EligibilityGate(MacResult, SchemaProfile, CaptureContext)
       no elegible  -> raw IEEE + reason (STOP para NWK)
       elegible     -> parse_nwk_header_bounded(mac_payload, profile)
                            -> NwkHeaderResult | NwkParseIssue
                            -> classify(NwkHeaderResult, issue, provenance, profile)
                                confirmed_by_context / structurally_consistent
                                      -> continuar con el **mismo** NwkHeaderResult
                                         [NWK Command, secured, invalid? STOP]
                                         [NWK Data plaintext -> APS parser]
                                ambiguous / invalid_for_profile -> raw + issue; STOP
```

**Regla:** `classify()` es evaluación de un **resultado ya producido**; no recibe un cursor mutable, no reinterpreta offsets ni ejecuta otro parse NWK. Los parsers de capas inferiores no modifican el buffer. El trabajo estructural puede ser tentativo sobre tráfico desconocido, pero es bounded y debe marcarse como *candidato*, nunca como Zigbee «confirmado» sólo por bytes.

### 3.3 Elegibilidad MAC (definida para el baseline, no para todo Zigbee)

- Contexto PHY IEEE, versión/política FCS y longitud disponibles; MAC parse sin contradicción que impida localizar payload.
- MAC Data con payload visible y sin seguridad MAC que lo oculte; formatos no soportados (FCF/PAN/IE/sequence suppression) regresan con `unsupported`, no se adivinan offsets.
- Si no se cumplen, `not_eligible` describe **«no elegible para NWK en este decoder»**, **no** «confirmadamente no Zigbee». Mac Command, Beacon, Inter-PAN/Green Power u otros formatos pueden ser importantes en el ecosistema, pero no son el NWK Data/Command convencional de este alcance; indicarlos fuera de subset y no etiquetarlos como tráfico ajeno.
- La edición de referencia IEEE 802.15.4-2015 no obliga a rechazar todas las tramas con campos de formato legado; documentar qué `Frame Version`/combinaciones concretas cubre el subset conforme a R22 y sus restricciones.

### 3.4 Semántica de clasificación (recomendada)

| Estado | Regla verificable | Conducta |
|---|---|---|
| `confirmed_by_context` | Contexto explícito **atribuible** a una captura Zigbee conocida, vinculado al frame/fixture, **más** estructura NWK compatible. Atributo `provenance_assurance` explica si sólo es declaración o evidencia externa contrastada. | Puede procesar NWK; no significa autenticado, certificado ni «demostrado por RF» si provenance sólo se declaró. |
| `structurally_consistent` | El parser NWK retorna cabecera completa y coherente con el perfil, pero la procedencia no está verificada. | Resultado tentativo que no puede ascender su confianza por la salida APS/ZCL. |
| `ambiguous` | No hay suficiente información para elegir interpretación, FCS/PHY no fiable, estructura incompleta que no demuestra contradicción o conflicto de contexto. | Preservar raw, candidatos e issues; no dispatch de aplicación autoritativo. |
| `invalid_for_profile` | Estructura contradice regla **documentada** del perfil solicitado (version/reserved/control/length). | Guardar el último segmento seguro y offset del problema. |
| `not_eligible` | Condición de formato MAC/seguridad/contexto excluye intentar NWK del subset. | Devolver resultado IEEE; no etiquetar «no Zigbee». |

**Invariante implementable:** para un MAC payload elegible, cada paquete invoca el analizador estructural NWK **a lo sumo una vez**; `classify()` no debe tener rutina de parsing secundaria. No usar excepciones genéricas como clasificación de tráfico no Zigbee. Guardar spans absolutos respecto de `Packet.data`.

### 3.5 Pruebas documentales de aceptación propuestas

- `ID-01`: MAC Data + NWK R22 válido y provenance fiable: `confirmed_by_context` y **1** análisis NWK.
- `ID-02`: mismos bytes sin provenance: `structurally_consistent`, no `confirmed`.
- `ID-03`: payload IEEE no Zigbee que imita campos NWK: no certeza; si estructuralmente indistinguible, queda **candidato**, no false positive confirmed.
- `ID-04`: MAC secured/non-Data/truncated: `not_eligible` o error de contexto/capa apropiado, sin lectura NWK.
- `ID-05`: reserved/version/longitudes inválidas bajo profile: `invalid_for_profile`; offset y raw preserved.
- `ID-06`: NWK Command no pasa a APS; NWK secured no pasa ciphertext a APS; APS fragment no pasa a ZDO/ZCL.
- `ID-07`: fuzz/bounds/inmutabilidad sin crash, sin doble parse; prueba de cuenta de invocaciones y spans `[start,end)`.

## 4. `CaptureContext`: contrato de evidencia, no etiqueta de certeza automática

### 4.1 Datos ya reportados por código

`Packet` contiene bytes, channel, timestamp, RSSI, correlación presentada como `lqi`, `crc_ok` y otros metadatos; no PHY ni campo FCS policy. `Radio._phy`/`set_phy()` describen configuración de sesión, pero no un reporte PHY RF independiente **por trama**. Firmware IEEE elimina PHR, conserva MPDU+FCS y descarta CRC-bad en la ruta auditada [INT-02, §§4, 6; INT-03, `FW-H01–03`, `FW-F09–10`]. El decoder no alterará `Packet`.

### 4.2 Modelo de procedencia propuesto

**Definir campos como valores + evidencia**; evitar un único `phy_confirmed: bool` sin procedencia.

| Propiedad | Valor conceptual | Evidencia adjunta | Comportamiento sin evidencia |
|---|---|---|---|
| `phy` | `IEEE_802_15_4` u otro | `session_state` y secuencia de configuración conocida, o descriptor de archivo verificado | `missing_context`, no MAC |
| `phr_policy` | absent/present/unknown | contrato del backend/build, o fuente fixture | unknown → raw hasta resolver |
| `fcs_policy` | present/absent/unknown | contrato del backend/build/PCAP DLT; estado CRC separado | unknown → no recortar FCS por heurística |
| `packet_origin` | device/corpus/pcap/fixture | ID de sesión, archivo y hash | provenance unspecified |
| `radio_generation` | epoch incrementado al cambiar PHY/RX/session | orden de configuración e inicio de RX | no asignar paquetes tardíos a epoch supuesto |
| `firmware_identity` | versión/hash de artefacto si se conoce | hash binario, manifest de flasheo, reporte de build | `unknown`, no inventar |
| `timestamp`/`lqi` | valores originales | metadata reportada por Packet + semántica actual conocida | sin convertir a tiempo absoluto/LQI calibrado |

Para cada propiedad de contexto utilizar una categoría independiente **`declared_by_caller`, `derived_from_session`, `verified_from_source`, `unknown`**, con `evidence_ref` opcional, `source_id` y `confidence_scope`. Estas etiquetas se diseñan para el contrato FeralRF; no son estados Zigbee ni mediciones RF. `verified_from_source` significa contrastado con una evidencia específica (build/archivo/instrumentación), no precisión física probada.

**Separar tres preguntas:** qué bytes hay (`Packet.data`); qué sabe el software sobre su modo y FCS (contrato/configuración); qué se verificó físicamente (captura externa o experimento reproducible). Ninguna inferencia de PHY por número de canal. Guardar copia immutable cuando el lifetime/ownership de la entrada no esté garantizado.

### 4.3 Aceptación

Diseñar fixtures `known-IEEE-FCS`, `unknown-FCS`, `missing-PHY`, `mismatched-PHY`, `stale-epoch`, `truncated-length` y `PCAP-with-provenance`, con resultados `raw/error` estables. No modificar API pública durante la decisión documental.

## 5. Perfil APS y política ZCL

### 5.1 Qué es la dependencia

APS es la **Application Support Sublayer**, entre NWK y los objetos/aplicaciones. La cabecera APS (en formatos pertinentes) aporta `Profile ID`, `Cluster ID` y endpoints; esos campos dan el contexto de la ASDU para decidir ZDO, ZCL u otra aplicación. No todo APS Data equivale a ZCL; un header con apariencia ZCL tampoco prueba su origen [INT-01, §§8, 11; NORM-01, §2.2; NORM-08–09].

### 5.2 Resolución documental de `0x0104`

Antes de esta investigación se dejaba `0x0104` sin justificación oficial suficiente. Ahora hay evidencia convergente:

1. Zigbee R21 (2015), tabla 2.44, identifica `0x0104` como **Common (HA)** [NORM-05], **histórica**, no la edición R22.
2. Zigbee *Lighting & Occupancy Device Specification* (2016) establece uso de `0x0104` [NORM-06].
3. TI SimpleLink Academy cita formalmente especificaciones R22, ZCL7 y Zigbee Lighting & Occupancy [NORM-07].
4. NXP y Silicon Labs confirman el uso de `0x0104` en contexto de Zigbee 3.0 [NORM-08–09].

**Conclusión de investigación:** `0x0104` ya no es un número meramente plausible; tiene justificación documental suficiente para **recomendarlo como primer perfil permitido del decoder**, de manera **condicional** al schema ZCL7 exacto, revisión del contexto APS R22 y fixtures. La activación no significa admitir todos los clusters, manufacturero o semánticas específicas del perfil.

### 5.3 Allowlist propuesta

- `0x0000` con endpoint 0: sólo los **seis** mensajes ZDO seleccionados.
- `0x0104`: primer perfil **propuesto** para habilitar ZCL foundation (ocho comandos/candidatos ZCL7), únicamente en `APS Data` con contexto y plaintext, cluster/endpoints presentes, header ZCL bounded y `manufacturer_specific == false` para semántica foundation genérica. Exigir `SchemaProfile` versionado.
- Otros application profile IDs, `0xFFFF` wildcard sin contexto y manufacturer-specific desconocidos: preservar APS ASDU raw, no confirmar ni inventar ZCL.
- Perfil permitido ≠ garantía de que el payload sea un mensaje ZCL válido; la decisión final depende de la estructura, tipo de frame ZCL, seguridad y schemas soportados.

**Estado:** ID y uso avalados documentalmente; política de activación y coverage siguen decisión de diseño y validación, no hecho implementado.

## 6. Revisión normativa — avance real frente a propuesta 1.1

| Elemento | Antes | Nueva evidencia | Recomendación |
|---|---|---|---|
| Zigbee Core | R22 «alineado» sin documento exacto | Documento oficial **05-3474-22, R22 v1.0, 19-abr-2017** encontrado [NORM-01] | Seleccionarlo como objetivo; obtener copia autorizada/versionada y comprobar erratas |
| ZCL | ZCL7 sin texto normativo | Documento oficial **07-5123-07, R7, feb-2018** encontrado y portada inspeccionada [NORM-02] | Fijar R7; extraer schemas de forma original y con citas a secciones |
| IEEE 802.15.4 | Edición asociada no resuelta | **R22 §1.5.1 [B1] y Annex D §D.1: IEEE 802.15.4-2015**; catálogo IEEE confirma la edición [NORM-01, NORM-03] | Referenciar **802.15.4-2015**, no 2020/2024, para baseline R22; acotar subconjunto MAC |
| APS profile app | `0x0104` candidato sin sustento oficial suficiente | R21 table 2.44, ZLO 2016 + TI/NXP/Silabs [NORM-05–09] | Proponer `0x0104` al asesor como primer perfil y justificar ámbito/limitaciones |
| Status/strings ZCL | Varios detalles provisionales | ZCL7 §2.5.5.1.2: write response success es **un único status byte sin attribute ID**; §§2.6.2.11–12: prefijo `0xFF` = valor inválido de string corta [NORM-02] | Actualizar esos dos esquemas normativos concretos y corpus; no dejar «desconocido» donde la norma ya responde |
| Seis mensajes ZDO | Candidatos derivados TI | R22 §2.4 expone formatos originales; p. ej. Device_annce §2.4.3.1.11 y NWK_addr_rsp §2.4.4.2.1 [NORM-01] | Comparar **seis** schemas, condiciones status/TSN/count contra R22 antes de dar los restantes por verificados |

**Importante:** «localizado e identificado» no significa que la totalidad de 600+ páginas esté analizada y que el corpus esté hecho. El resto de los campos normativos y correcciones de edición/errata requieren revisión focalizada. Conservar identificadores de sección, fecha, fuente y hash de copia de uso autorizado; no publicar el PDF original en Git/Obsidian.

## 7. Riesgo de utilidad real: NWK security obligatoria

La propuesta pasiva permite interpretar únicamente los campos **visibles** cuando existe seguridad; prohíbe descifrar en el baseline [INT-01, §9]. La documentación TI de Z-Stack y Silicon Labs indica que **NWK security es obligatoria en Zigbee 3.0** para tráfico de red ordinario [NORM-11–12]. La mayoría de las APS ASDU ordinarias no serán visibles como plaintext desde una captura RF pasiva. Por tanto:

- **Es correcto** implementar/declarar MAC y NWK header, clasificador, límites de seguridad, raw preservation e issues sin claves.
- **Es condicionado** afirmar que el decoder «decodifica ZDO/ZCL de tráfico normal de producción»; sólo podrá hacerlo cuando exista plaintext legítimamente disponible según el contrato (fixtures sintéticos/controlados o un camino futuro de descifrado autenticado expresamente aprobado).
- No confundir «parser ZCL probado con bytes plaintext de fixture» con «ZCL observado OTA de red protegida». Son criterios de aceptación **distintos**.
- Si el asesor espera un sniffer Zigbee útil para analizar atributos/comandos de redes reales, debe aprobar **otra fase**: fuentes legítimas de claves, CCM* exacto, autenticación MIC, privacy, captura/corpus autorizado y pruebas de extremo a extremo. **No** integrar esa fase silenciosamente en el baseline.

## 8. Licencia y otros riesgos jurídicos: dictamen de prudencia

### 8.1 Texas Instruments no licencia todo Z-Stack igual

El manifest oficial **8.30.01.01** lista TI Z-Stack con **TI Text File License** para binario/fuente limitada y **BSD-3-Clause** para fuente [LEG-01]. La licencia TI Text tiene restricciones de uso/redistribución relativas a **TI Devices**, mantiene avisos, limita redistribución y prohíbe ingeniería inversa de binarios; la variante BSD-3 autoriza redistribución/modificación bajo condiciones típicas de aviso y no aval [LEG-02–03]. Las auditorías registran `zcl.c` como aviso BSD-style y `zd_object.c`, `nwk_util.h`, `aps.h` con términos TI Devices [INT-03, `SDK-Z01`, `SDK-Z13`]; **verificar cabeceras reales por archivo en el commit SDK antes de cualquier reutilización**.

**No se puede concluir «uso libre de todo el repositorio porque está en GitHub»**. Tampoco se puede presumir que ejecutar un port Python en PC esté cubierto por una licencia específica TI Devices sólo porque el sensor contiene un CC1352P7. Separar **consultar código para analizar** de **copiar código/firma/comentarios/implementación** y de **redistribuir**.

### 8.2 Política propuesta

| Uso | Posición conservadora |
|---|---|
| Leer el SDK y citar función/ruta/sección como evidencia | Aceptable conforme al acceso legítimo, sin republicar grandes extractos y respetando términos. |
| Implementar decodificador desde normas autorizadas, con tablas/algoritmos propios y test vectors originales | **Preferido**, citando la norma sin reproducir su texto protegido; revisar patentes/condiciones aplicables al producto. |
| Adaptar fuente BSD-3-Clause concreta | Posible solo tras revisión de cabecera/licencia/autoría/avisos de ese archivo y compatibilidad con distribución resultante. |
| Copiar/portar `zd_object.c`, `nwk_util.h`, `aps.h` con términos TI Devices a Python host | **No aprobado**; estudiar permiso expreso o reimplementación independiente. |
| Enlazar binarios Z-Stack/TIMAC o decompilar | Fuera de alcance; además de incompatibilidad técnica, existen restricciones contractuales explícitas. |
| Incorporar Scapy GPL-2.0-only como código/dependencia en FeralRF Python marcado GPL-3.0 | **Riesgo de incompatibilidad**, no hacerlo automáticamente; usar comparator externo por proceso separado y evaluar su distribución. |

### 8.3 Normas CSA/IEEE ≠ bibliotecas open source

La CSA conserva copyright sobre las especificaciones y sus términos contemplan consulta/descarga/uso interno bajo condiciones, no la publicación o adaptación del documento [LEG-04–05]. **Gratis de descargar no significa dominio público**. Un algoritmo implementado de manera independiente a partir de reglas técnicas no es automáticamente copia del texto, pero derechos de patente, marcas, condiciones contractuales y límites territoriales son materias separadas: no puede certificarse «sin riesgo legal» sin revisión especializada. IEEE vende/accede por licencia a ediciones técnicas [NORM-03]. No subir PDFs ni secciones extensas al Git público ni a repositorios sin permisos.

### 8.4 Otras obligaciones

- **Marca/certificación:** describir la función como «decodificación/analizador de tramas compatibles con Zigbee», nunca como «Zigbee Certified» ni mostrar logos sin certificación CSA [LEG-06].
- **Licencia GPL FeralRF:** `python/pyproject.toml` fue reportado con GPL-3.0 [INT-03]. Confirmar raíz y terceros; **GPLv2-only Scapy ≠ automáticamente combinable** con GPLv3 en el mismo programa [LEG-07–09]. El uso aislado de un programa externo como comparador se evalúa separadamente, sin prometer inmunidad automática.
- **Datos capturados:** conservar sólo el corpus necesario, anonimizar identificadores sensibles cuando sea posible, documentar consentimiento/autorización del laboratorio, proteger credenciales/claves y no publicar capturas de redes ajenas sin autorización.
- **RF y jurisdicción:** baseline pasivo sin cambios TX; cualquier ensayo de inyección/TX posterior debe ejecutarse con autorización, parámetros de banda/potencia y reglas regulatorias aplicables al lugar de prueba, no suponerse exento por ser académico.
- **Patentes y certificación:** cumplimiento de estándares, ausencia de infracción y derecho de certificación no se deducen de disponer del SDK o de la norma.

**Alcance jurídico de este informe:** identificación de condiciones y riesgos visibles, **no opinión legal vinculante**. Antes de distribución pública/comercial o adaptación de código TI restrictivo, solicitar revisión institucional o asesoría jurídica.

## 9. Requisitos/resultados de validación que deben separarse

| Claim | Evidencia futura específica | No prueba |
|---|---|---|
| `Packet.data` conserva MPDU+FCS | code trace identificado + byte-by-byte fixture/OTA con firmware/build y metadata | Zigbee, RF completion host |
| MAC parser bounded | IEEE 2015, corpus positivos/negativos, spans/FCS exactos | Identidad Zigbee |
| `R-ID-01` | Fixtures Zigbee/no Zigbee/ambiguous con provenance y **una sola invocación NWK** | Confirmación de origen RF basada sólo en bytes |
| NWK header | R22 §3.3, fixtures de flags/opcionales/version/security | Plaintext APS si NWK secured |
| APS/ZDO/ZCL | R22 §2.2–2.4 + ZCL7 §2.4–2.6, plaintext **autorizado**, fixtures status/TSN/count/types | Interpretación de tráfico de red cifrado sin claves |
| Licencias/provenance | Inventario reproducible de headers, manifest, notices y aprobación de redistribución | Ausencia global de riesgo legal |
| Compatibilidad | Regression de import/API, buffer/lifetime/CPU/backlog | Que ya se ejecutaron tests o que no habrá pérdidas |

## 10. Decisiones preparadas para el asesor

| ID | Decisión técnica recomendada | Estado | Qué se solicita al asesor |
|---|---|---|---|
| DEC-01 | Una sola lectura NWK; elegibilidad MAC + NWK structural parse + clasificación | **Definible internamente** | Aprobación del diseño general, no de cada nombre de función |
| DEC-02 | `CaptureContext` con valor + fuente/evidence + epoch, sin confundir declarado y observado | **Definible internamente** | Conformidad con rigor de provenance |
| DEC-03 | R22 v1.0 / ZCL7 / IEEE 802.15.4-2015 como base normativa | **Sustentado documentalmente** | Aprobación del target y su cobertura acotada |
| DEC-04 | Perfil `0x0104` candidato como primera allowlist ZCL en uso condicionado | **Sustentado, pendiente de schema/fixtures** | Aprobar incluirlo como cobertura inicial o excluirlo |
| DEC-05 | Software Python host completamente independiente de código restrictivo TI | **Recomendado** | Aprobación de política de desarrollo y provenance |
| DEC-06 | Decoder pasivo **sin** decryption que se detiene ante NWK secured | **Técnicamente consistente, utilidad real limitada** | **Decisión de alcance crítica:** ¿es suficiente el análisis de headers/fixtures o se requiere ZDO/ZCL de capturas reales? |
| DEC-07 | No afirmar licencia libre universal, conformidad ni «Zigbee Certified» | **Obligación de prudencia** | Validar lineamientos institucionales de publicación/distribución |

## 11. Qué permanece sin resolver

1. Contrastar el PDF R22 obtenido legalmente, su hash, sus erratas y **todos** los 6 esquemas ZDO/seguridad con la norma; el buscador web encontró el documento, pero no sustituye la copia local controlada y la revisión detallada.
2. Extraer de ZCL7 **todas** las reglas de los ocho comandos, tipos, dirección y valores inválidos (ya hay evidencia precisa para dos reglas). No afirmar cobertura R7 completa aún.
3. Fijar un corpus legal/identificado para Zigbee y no Zigbee, en claro y secured, además de comparator release/config/DLT/FCS.
4. Verificar cabeceras de licencia **por archivo** directamente en el SDK fijado, avisos, SPDX si los hay, y condiciones sobre redistribución pública de SDK/submódulo; evaluar compatibilidad jurídica por destinatario (PC host vs dispositivo TI).
5. Determinar si los objetivos académicos exigen tráfico Zigbee **real cifrado**: el baseline sin decryption no expone normalmente APS/ZCL. Resolverlo **antes** de comprometer resultados experimentales.
6. Registrar política de capturas/datos, consentimiento, almacenamiento, hash y no publicación de claves o material sensible.

## 12. Recomendación de siguiente actuación

No sobrescribir aún la propuesta 1.1; integrar estos hallazgos como anexo de decisión, comprobar la copia controlada de las normas y el inventario de licencias con Codex en el checkout, y después actualizar **sólo** las secciones afectadas de propuesta y registro de fuentes. No se solicita implementar firmware ni parsers; no se ha compilado, flasheado ni hecho experimento RF.
