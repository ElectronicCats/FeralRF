# Revisión normativa, arquitectónica y de licencias — FeralRF Zigbee

**Fecha de consulta:** 2026-11-04  
**Checkout:** `f0a64ef9a00438757d474fd037c0d1847e6596e4`, rama `feature/hardware-validation`  
**SDK:** submódulo `5b31d0a4903351e544546e23ef3330eaa4291ceb`, SimpleLink `8.30.01.01`, Z-Stack `8.30.00.x`  
**Target:** CC1352P7  
**Naturaleza:** revisión documental de fuentes, arquitectura, normas y provenance; no es opinión legal ni evidencia experimental.

## 1. Dictamen

La arquitectura host-side, pasiva, opt-in y sin cambios iniciales a `Radio`, `Packet` ni firmware sigue siendo adecuada. No hay diferencias de código `firmware/` o `python/` entre el HEAD que respaldó la propuesta 1.1 (`59816be...`) y el HEAD actual; los commits intermedios que afectan a esas carpetas no añaden cambios al comparar directamente ambos árboles. El HEAD nuevo incorpora cambios documentales y correcciones, por lo que el baseline de la propuesta sí debe actualizarse.

La propuesta necesita revisión sustantiva 1.2 antes de supervisión: resolver `R-ID-01` con un único parser NWK; distinguir evidencia declarada, derivada y verificada; fijar como referencia Core R22 1.0 / ZCL 7 / IEEE 802.15.4-2015; corregir las dos reglas ZCL verificadas; ampliar la descripción de licencia por archivo; y dejar explícito que decodificar ZDO/ZCL de tráfico normal protegido no se logra con el alcance sin descifrado. La aplicación `0x0104` tiene respaldo documental histórico/oficial suficiente para recomendarla como primer perfil contextual, pero no para habilitar clusters o declarar conformidad: se debe acotar a la foundation genérica ZCL7, con contexto APS correcto, plaintext, manufacturer bit apagado y aprobación de perfil/versionado.

No se verificó una autorización legal general para copiar, traducir o redistribuir los documentos CSA/IEEE ni para portar source TI con licencia restringida a una biblioteca Python GPL-3.0. La estrategia más defendible sigue siendo implementación independiente basada en normas obtenidas legalmente, sin transportar expresión de código TI. TI puede usarse para localizar y contrastar campos; toda adaptación directa requiere revisión legal de archivo y distribución concretos. Nada en este informe constituye autorización de redistribución.

## 2. Método, baseline y fuentes

Se verificaron los dos documentos complementarios bajo `docs/Z-Stack_Reports/zigbee_proposal/`, la propuesta 1.1, su registro 1.1, la revisión arquitectónica, las auditorías requeridas, el `LICENSE` de raíz, `python/pyproject.toml`, el manifest y licencia instalados en el SDK, y cabeceras de los archivos TI de la propuesta. Los hallazgos de paquetes y código se contrastaron con sus fuentes primarias. La inspección no modificó los documentos de investigación complementarios.

| Hecho | Evidencia directa | Alcance |
|---|---|---|
| HEAD/branch | `git rev-parse HEAD`: `f0a64ef9a00438757d474fd037c0d1847e6596e4`; `git branch --show-current`: `feature/hardware-validation` | Verificado al inicio; el HEAD no cambia durante la edición documental. |
| SDK | `git submodule status`: `5b31d0a4903351e544546e23ef3330eaa4291ceb`; `.metadata/product.json` identifica 8.30.01.01 | Se usó esta copia; ninguna instalación alternativa. |
| Código desde baseline 59816be | `git diff 59816be..HEAD -- firmware python`: sin cambios | La propuesta puede conservar hallazgos de código, actualizando el identificador de HEAD. No equivale a test. |
| Licencia FeralRF | raíz `LICENSE` = GPL versión 3 o, a elección, posterior; `python/pyproject.toml` declara GPL-3.0 | Declaración aplicable a esos componentes; no presupone licencia uniforme para todo el repositorio o material de terceros. |
| SDK manifest local | `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/manifest_simplelink_cc13xx_cc26xx_sdk_8_30_01_01.html` y archivo `license_simplelink_cc13xx_cc26xx_sdk_8_30_01_01.txt` | El manifest registra TI Z-Stack como TI Text File License para binary/limited source y BSD-3-Clause para source, en la misma zona; el manifest no sustituye el aviso por archivo. |
| Alcance de código | No hay diff de firmware/Python desde el baseline anterior; decoder no existe en el checkout | No compilación, ejecución, flasheo, radio, ni claim de función implementada. |

## 3. Normativa y perfiles

### 3.1 Perfil de wire version recomendado

| Capa | Identificación verificada | Relación/estado | Decisión propuesta |
|---|---|---|---|
| Zigbee Core | `05-3474-22`, *Zigbee Specification*, Revision 22 1.0, 2017-04-19 | El documento CSA extraído identifica título, revisión y fecha. R22 Annex D §D.1 dice que su MAC/PHY aclaratoria aplica a IEEE 802.15.4-2015 y que R22 asume esa MAC/PHY. | Fijar Core R22 1.0 como primer `SchemaProfile`, sujeto a copia autorizada controlada, hash/errata y aprobación académica. No tratar “R22.1” como sinónimo de R22 1.0. |
| ZCL | `07-5123-07`, *Zigbee Cluster Library Specification*, Revision 7, copyright 2007–2018 | La extracción del PDF CSA muestra identificador `075123`, revisión 7 y reglas de foundation; release notes TI Z-Stack 8.30 declara ZCL7 y Zigbee PRO 2017/R22. | Fijar ZCL7 para el subset declarado. La ficha metadata está verificada; los detalles citados aquí se contrastaron en el texto extraído, pero guardar/compartir el documento queda sujeto a licencia CSA. |
| IEEE MAC/PHY | IEEE Std 802.15.4-2015; página IEEE señala publicación 2016-04-22, estado superseded y superseded-by 802.15.4-2020 | R22 Annex D §§D.1.1–D.1.2 identifica 2015 expresamente; IEEE registra errata/corrigendum y amendment list. R22 no se reemplaza automáticamente por la edición actual. | Usar 2015 como perfil base referido por R22; confirmar errata/corrigendum y subset MAC contra copia IEEE legal. No adoptar 2020/2024 silenciosamente. |
| `0x0104` | R21 `05-3474-21` Table 2.44 nombra Profile ID `0x0104` como Common (HA); CSA *Lighting & Occupancy Device Specification* `15-0014-05` es una especificación de aplicación histórica; TI Academy enlaza documentos de perfil/R22/ZCL7 | R22 1.0 no contiene el literal `0x0104` en el texto extraído. Por ello su identidad se apoya en documentos de perfil históricos y documentación de fabricante; no en una declaración del SDK ni en una tabla R22 localizada. | Recomendado como primer contexto APS de aplicación para foundation ZCL genérica, condicionado a conservar fuente/perfil exactos y aprobación. No significa admitir todos los clusters, versiones de clusters, semántica HA o manufacturer-specific. Si no se conserva licencia/copia del perfil fuente, el identificador queda recomendado con esta limitación expresa. |

Las fuentes primarias consultadas están listadas en §9 con URLs, fechas y límites de acceso. El PDF oficial CSA R22 fue indexable y permitió inspeccionar secciones R22; enlaces directos históricos pueden redirigir la página web. ZCL7 se localizó en ruta oficial y el texto indexado expone reglas específicas. Esta consulta web no otorga permiso para redistribuir PDFs ni sustituye control institucional de copias. Las especificaciones CSA están protegidas por copyright; los términos CSA permiten descargas bajo términos aplicables y restringen copia/republicación/modificación sin autorización. IEEE declara acceso de compra/suscripción; su texto no es público dominio. El protocolo puede resumirse con referencias precisas sin copiar tablas o pasajes extensos.

### 3.2 ZDO y ZCL seleccionados

R22 confirma cluster IDs, nombres y orden de campos de las seis estructuras seleccionadas. Los IDs permanecen: `Device_annce 0x0013`, `NWK_addr_rsp 0x8000`, `IEEE_addr_rsp 0x8001`, `Node_Desc_rsp 0x8002`, `Simple_Desc_rsp 0x8004`, `Active_EP_rsp 0x8005`. El TSN del ZDP envelope se separa del transaction-data/ASDU en la arquitectura TI (`zd_profile.c:2104-2106`); en el parser propuesto se contabiliza exactamente una vez. Status, solicitud original (para interpretar listas asociadas), length/count, APS profile/endpoints y truncación deben seguir condicionados a las reglas de R22 §2.4; un parser sin contexto de request no deduce semántica RequestType por el layout únicamente. La validación completa de condiciones status/variable y corpus sigue pendiente; la disponibilidad normativa ya no es el mismo vacío que en propuesta 1.1.

ZCL7 Table 2-3 confirma IDs foundation en alcance: Read Attributes `0x00`; Read Attributes Response `0x01`; Write Attributes `0x02`; Write Attributes Response `0x04`; Report Attributes `0x0A`; Default Response `0x0B`; Discover Attributes `0x0C`; Discover Attributes Response `0x0D`. Sus direcciones de uso típico y layouts son del command/protocol context, no una regla de identidad independiente: validar frame type global, manufacturer-specific bit, direction bit, APS context y estructura acotada. El subset no incorpora cluster-specific commands ni perfiles de clusters.

Dos reglas antes provisionales quedan resueltas por ZCL7: (1) la respuesta Write Attributes de éxito total es un status record único SUCCESS cuyo campo Attribute ID se omite; respuestas con error incluyen records status y attribute ID; no se modela como solo byte de status. (2) en octet string y character string cortas, length 0 es vacío y `0xFF` indica valor inválido; ambos casos tienen cero bytes de contenido. No confundir este sentinel de string con el datatype ID desconocido `0xFF`. El resto de tamaños, invalid values y dirección debe estar trazado a ZCL7 antes de afirmar que el subset está completo. La implementación inicial sigue siendo bounded parser, no una emulación completa del ZCL.

`0x0104` sólo permite seleccionar un contexto de aplicación, no probar que cada ASDU sea ZCL. Condiciones acumulativas para dispatch foundation ZCL: APS Data plaintext; APS profile exacto en allowlist versionada; Cluster ID y source/destination endpoints preservados; APS security ausente o plaintext autenticado por mecanismo independiente fuera del baseline; no fragmentación pendiente; ZCL frame type global; header bounded; manufacturer-specific bit 0; command ID y tipos dentro del allowlist. Si falta cualquiera, devolver ASDU original opaco/candidato, sin semantic decode. El profile `0x0000` + endpoint 0 continúa reservado a ZDO/ZDP; los seis mensajes ZDO no se derivan de `0x0104`.

## 4. Arquitectura: un parser NWK y estados separados

El diagrama de la propuesta 1.1 era circular: el clasificador consultaba estructura NWK antes de invocar luego el parser NWK. Se elimina esa duplicación. El nuevo contrato conceptual, no implementado, es:

```text
Packet.data + CaptureContext
    → MAC bounded parser → MacResult (campos/spans/seguridad/estado)
    → MAC eligibility gate
       ├─ no elegible, PHY/FCS insuficiente, MAC no-Data, MAC ciphertext
       │    → preservar raw; NWK no se invoca
       └─ candidato elegible
            → UNA invocación: NWK structural parser(mac_payload_span, profile)
            → NwkResult (campos/opcionales/spans/seguridad/status/issues/payload span)
            → classifier(NwkResult, provenance, profile)
                 ├─ identificado por procedencia independiente y estructura válida
                 ├─ estructuralmente consistente, origen no probado
                 ├─ ambiguo/parcial/no soportado
                 └─ inválido bajo profile explícito
            → APS sólo con NwkResult válido, Data, plaintext y elegible;
              structurally_consistent continúa como análisis tentativo, nunca como origen confirmado
```

`NwkResult` debe ser independiente de la hipótesis de origen: recibe span bounded desde el gate MAC, profile/revisión solicitada y estado de provenance como metadato, no como permiso para saltarse bounds. Devuelve `status` separado de `classification`: valid / partial / malformed / unsupported-version; NWK FCF y campos base/opcionales en orden normativo; offsets absolutos del MPDU; spans header, auxiliary/security, ciphertext/MIC sólo si norma permite calcularlo, y remaining payload; reservado/unknown values; issue con offset y requerido/disponible; booleano/razón `aps_dispatch_permitted`. Nunca debe calcular el NWK offset un segundo módulo clasificador.

| Clasificación/disposición | Condición necesaria | Qué afirma y permite |
|---|---|---|
| `verified_by_provenance` (reemplaza `confirmed_by_context`) | Provenance `verified_from_source` independiente identifica la fuente/trama como Zigbee; un único NWK parse válido para el profile. | Afirma procedencia según evidencia documentada, no certificación ni autenticación criptográfica. Puede ir a APS si plaintext/seguridad lo permite. Contexto caller-declared no basta. |
| `structurally_consistent` | Elegible MAC y parser NWK válido para perfil, sin provenance independiente. | Sólo candidato compatible; permite APS tentativo con etiqueta/caveat y security gates. Nunca identidad Zigbee. |
| `ambiguous` | Falta contexto, NWK parcial, formato no resuelto o revisión no soportada sin contradicción demostrada. | Preservar raw + issues; no dispatch semántico. |
| `invalid_for_profile` | Regla obligatoria de un profile explícito contradicha (longitud, reserved field/version). | Parcial/raw + issue; no significa “no Zigbee” universal ni borra provenance separado. |
| `not_eligible` | MAC/context gate: tipo no Data, payload ausente, PHY/FCS/PHR desconocidos, versión MAC fuera de cobertura, MAC security bloquea plaintext. | Raw IEEE; no se invoca NWK. |

Así `verified_by_provenance` no se confunde con contexto sólo declarado ni autenticación. Una fixture construida/etiquetada demuestra que el parser maneja ese fixture; no que esa trama salió por RF. Una captura OTA observada con equipo/emisor identificado puede apoyar `verified_from_source`, pero sólo valida lo que realmente registra ese experimento. Autenticación Zigbee requiere CCM* y contexto/claves apropiados y es otra categoría.

## 5. `CaptureContext`: snapshot con fuente y nivel de evidencia

No ampliar `Packet`. El adapter opt-in debe crear el contexto en el momento de adquirir/encolar cada packet, no consultar estado mutable de `Radio` más tarde durante decode. Actualmente `Radio` conserva PHY configurada en el host; hay comandos de set PHY/canal y ACK de comando, pero no campo PHY por paquete ni telemetry independiente de firmware. Un comando host aceptado o session state cambiado no confirma lo que el RF Core aplicó a cada frame. Channel tampoco demuestra PHY.

Para cada valor contextual usar par `value + evidence + source + capture/session identity + epoch/time`; evidencias: `declared_by_caller` (aserción no verificada), `derived_from_session` (valor calculado del historial host de comandos/ACK, no observación per-packet), `verified_from_source` (metadata correlacionada por captura externa o fixture/documento controlado, describiendo exactamente qué se verificó), `unknown`. No usar etiqueta `phy_confirmed` sin una procedencia capaz de confirmarla.

| Dato | Fuente disponible/límite | Tratamiento seguro |
|---|---|---|
| PHY | `Radio` conoce selección host/session; `Packet` no lleva PHY; no hay metadata por packet independiente observada. | Valor con `derived_from_session` si epoch válido, nunca hardware-confirmed. Ausente/desalineado → raw, sin parsear MAC. |
| PHR/FCS policy | Backend documentado define RX IEEE entrega MPDU con FCS, PHR removido; no existe un flag FCS-policy autocontenido en `Packet`. | Atribuir a contract/build concreto o fuente externa versionada. Unknown/conflict → no inferir tail/FCS por heurística. |
| CRC | `Packet.crc_ok` está presente; firmware descarta bad CRC en IEEE path auditado. Un `True` sólo prueba campo reportado en esa ruta/build, no origen Zigbee. | Preservar valor y source; `False`/unknown pasa raw y queda fuera de semántica por default. |
| Sesión/epoch/cambios PHY | Host puede emitir cambios; recepción buffering/demora no se correlaciona con un firmware-reported epoch por packet. | Congelar snapshot al enqueue/read; al cambio cerrar epoch. Paquetes previos en buffer con epoch ambiguo → context mismatch/unknown y raw-only. |
| Timestamp/canal/RSSI/LQI-correlation | Valores de `Packet` y backend; timestamp RAT, channel, RSSI, correlation presentado como `lqi`. | Mantener origen/unidad y exact bytes. No llamar wall clock/LQI calibrado sin evidencia. |
| Capture source / offline fixture | Aporte externo del caller/corpus; no parte del paquete de transporte actual. | Identificador, hash/cadena de custodia, autor de fixture y método de observación; caller declaration no equivale a verify. |
| Contract/schema version y ownership | Contrato de adaptación futuro; el input vive en Packet/caller. | Guardar versión de contrato/parser; bytes inmutables/copiados si lifetime incierto. |

Truncated packet, unsupported PHY o contexto conflictivo no se “arreglan” con canal, RSSI ni heuristic. Se devuelve issue, `RawCapture` intacto, sin NWK parse. Una trama recibida después de cambio no debe decodificarse usando el último `Radio.phy` leído al invocar el decoder si el epoch no puede asociarse con esa trama.

## 6. Seguridad y valor científico del alcance

Scope A (baseline) es un decodificador estructural pasivo: MAC, una lectura NWK, clasificación, headers/seguridad visible, APS/ZDO/ZCL desde plaintext autorizado, fixtures offline. No genera claves, no realiza joining, no descifra, no valida MIC y no procesa contadores persistentes. Permite estudiar parsers bounded, offsets, tipos/estados, clasificación conservadora y seguridad estructural; es una contribución académicamente verificable si se declara que no mide semántica de red real protegida.

Scope B (deferido, no implicado) añade key handling autorizado y Zigbee CCM*: security level, nonce, AAD, MIC length/check, frame counters/replay, address/key-source mapping, MAC/NWK/APS security boundary y vectors known-answer. NWK security en R22 protege payload NWK después del encabezado; según security level puede cifrar payload o autenticación sin confidencialidad. Por ello el NWK header/auxiliary security info visibles no garantizan que el payload de NWK Data (APS/ZDO/ZCL) sea plaintext. Excepciones y APS security separada existen; no decir que “todo frame está cifrado” ni que APS siempre invisible. En Scope A, los campos NWK protegidos paran el dispatch; ZCL/ZDO se prueban con fixtures plaintext autorizados. Claim de lectura de payload habitual de una red Zigbee 3.0 asegurada requiere Scope B o una fuente plaintext documentada. La aprobación de Scope B es una decisión separada con fuentes legítimas de claves, privacidad/datos, reproducción y prueba criptográfica.

## 7. Inventario de código TI y evaluación de reutilización

El manifest SDK registra ambas licencias de Z-Stack para rutas generales de ejemplos/source; la leyenda del propio manifest dice que el path con `*` aplica a todos los archivos y las excepciones se señalan aparte. En consecuencia, la categoría general del manifest no resuelve el archivo concreto. Se cotejó la cabecera real de cada archivo central: `zcl.c`/`zcl.h` muestran BSD 3-clause; los archivos ZDO/NWK/APS/ZMAC listados debajo contienen `Limited License` y condición TI Devices. El SDK root `license_simplelink...txt` contiene los términos de licencia limitados correspondientes. La licencia de una unidad C no se propaga a todo Z-Stack ni desde `zcl.c` a un header con distinta cabecera.

| Archivo y símbolo | Copyright/licencia por archivo y manifest | Hecho técnico/dependencias | Tratamiento propuesto | Riesgo/acción |
|---|---|---|---|---|
| `source/ti/zstack/stack/zcl/zcl.c`: `zclParseHdr()` 2412–2446; `zclBuildHdr()` 2459–2482; `zclSerializeData()` 2961–3076; `zclGetDataTypeLength()` 3195–3283; `zclGetAttrDataLength()` 3296–3322 | Copyright TI 2019; aviso BSD-style en líneas 9–37; el manifest enumera Z-Stack BSD source. | C con dependencias/translation unit ZCL runtime; parsers/builders sin length/capacity completos, punteros a memoria mutable; `zclBuildHdr` es static. | Funciones como evidencia técnica y cross-check. `zclParseHdr` directo sería `DIRECT_REUSE_POTENTIALLY_PERMITTED` bajo BSD si se cumplen avisos/condiciones, pero `NOT_RECOMMENDED_FOR_DIRECT_REUSE` por falta de bounds y dependencia C. Independiente desde ZCL7 preferido. | Si se copia/adapta, mantener copyright/licencia/disclaimer y no aval; revisar expresividad derivada + compatibilidad final GPL-3. No reutilizar helper inseguro sin endurecimiento y test. |
| `source/ti/zstack/stack/zcl/zcl.h`: FCF, command IDs, data types/structs 490–557, 597–652, 822–1016; declaraciones helper 1927–1994 | Copyright TI 2019; BSD-style líneas 9–37; manifest general/source. | Declaraciones/constantes y estructuras ABI C, algunas no son wire schemas y no transfieren funciones. | `REFERENCE_ONLY`; tablas wire independientes desde ZCL7. | Copiar header sin requerir algoritmo no es necesario; identificar notices si se extraen fragmentos. |
| `source/ti/zstack/stack/zdo/zd_profile.h` y `.c`: IDs, `fillAndSend`, TSN split | Copyright TI 2004–2015; `Limited License`; manifest ubica ruta en categoría TI Text File License y BSD source general, pero archivo lleva el aviso restrictivo. | Códigos de servicio ZDO, dispatch, globales/estado/profile runtime. | Headers como evidencia; `INDEPENDENT_IMPLEMENTATION_FROM_SPEC`. | No copiar/portar/translate; licencia condicionada a TI Devices, asesoría legal si se desea reutilizar. |
| `source/ti/zstack/stack/zdo/zd_object.c`/`.h`: `ZDO_Parse*` (incl. Node/Simple/EP) 3061–3194, 3543–3618 | TI 2004–2015/14, `Limited License`; header completo incluye “only with TI Devices”. | Parse routines con `zdoIncomingMsg_t`, global/context, heap `OsalPort_malloc/free`, ownership y bounds incompletos. | `NOT_RECOMMENDED_FOR_DIRECT_REUSE`; sólo field-order cross-check y reescritura propia desde Core R22. | “Traducir” código no convierte por sí solo el port en independiente; revisión legal si se reproduce estructura o expresión. |
| `source/ti/zstack/stack/nwk/nwk_util.h`: FCF/masks/structs; `NLDE_ParseMsg()` 319–321 | TI 2004–2015 `Limited License`; manifest no hace BSD general a cada file. | Header definitions y declaración solamente; no hay `stack/nwk/nwk.c` ni definición source accesible. | `REFERENCE_ONLY` para nombres/contraste; parser NWK independiente desde R22. | No citar función como reusable; no copiar header restrictivo. |
| `source/ti/zstack/stack/nwk/aps_mede.h`, `aps_util.h`, `aps.h`: APS formats/declarations; `APSDE_ParseMsg()` 110–113 | Cada cabecera leída lleva aviso TI Limited License (rango de header varía; `aps_mede.h` existe). | Definiciones y API de runtime; parser APS declarado sin implementación fuente visible. | `REFERENCE_ONLY`; implementar APS structure desde Core R22. | No asumir `APSDE_ParseMsg` existe/compila; revisión legal para cualquier reutilización. |
| `source/ti/zstack/zmac/f8w/zmac.c` / `zmac_cb.c` | TI 2005–2017; `Limited License`, TI Devices. | Wrappers/callbacks y `MAP_MAC_*`; funcionalidad complementada por librerías TIMAC, RTOS/config y RF/task runtime. | `REFERENCE_ONLY`; no enlace/adaptación en target GNU FeralRF. | Sin compatibilidad técnica ni derecho de copia verificados; no enlazar/copiar. |
| Seguridad Z-Stack (`stack/sec/`, `zd_sec_mgr.c`, `ssp.h`) | Revisar licencia por unidad antes de cualquier reutilización; fuentes no forman parte del runtime FeralRF | CCM*/key lookup/frame count depende de dirección, counters, NWK/AIB/NIB/APS state y persistent security context. | Scope A reference-only; Scope B specification-driven, reviewed separately. | No usar helper source/crypto como prueba de corrección o autorización de clave. |

### 7.1 Interpretación de TI Text File License

Manifest 8.30 distingue TI Text File License y BSD-3-Clause para el componente; el aviso TI Text File permite uso condicionado y dice que redistribución/modificación del source y derivados “are licensed by TI for use only with TI Devices”. Incluye preservación de notices, limitaciones de redistribución binaria, restricción de reverse engineering para binarios, no endoso y descargos. El alcance de una biblioteca Python host que se comunica con un producto CC1352P7 no queda resuelto con seguridad por el texto localizado: no se afirma que PC host esté automáticamente prohibido ni que el hardware TI satisfaga todas las condiciones. Cualquier copia, traducción, derivado o redistribución de esos ficheros bajo GPL-3.0 exige consejo legal específico.

BSD-3-Clause en `zcl.c` y `zcl.h` da permiso de uso/modificación/redistribución sujeto a conservar notices en fuente, reproducirlos en binarios y no usar nombres TI/contribuidores como endorsement; disclaimer/patent status deben conservarse. Ese permiso no permite ignorar seguridad de bounds ni otros headers dependientes. El manifest identifica software delivered types y path buckets; use file header to map exact files. “BSD-style” no significa public domain.

| Candidato | Clasificación solicitada | Justificación/acción |
|---|---|---|
| Lectura/cita de `zcl.c/.h` como evidencia | `REFERENCE_ONLY` | Citar path/símbolo/lineas; evitar reproducir código extenso. |
| `zclParseHdr()` | `DIRECT_REUSE_POTENTIALLY_PERMITTED` + `NOT_RECOMMENDED_FOR_DIRECT_REUSE` | BSD permite sujeto a condiciones; función no conoce longitud/bounds y no es API autónoma. Preferir parser original desde ZCL7. Legal review antes de incorporar. |
| `zclBuildHdr`, `zclSerializeData`, length helpers | `DIRECT_REUSE_POTENTIALLY_PERMITTED`, pero no recomendado | Igual licencia visible; falta capacidad de buffer y runtime-/ABI- assumptions; están fuera del decoder pasivo. |
| `ZDO_Parse*`, ZDO profile implementation | `REQUIRES_LICENSE_REVIEW` / `NOT_RECOMMENDED_FOR_DIRECT_REUSE` | TI-only-device clauses; dependencia de asignación/contexto/ownership y no bounds completo. Usar sólo para consulta, wire parser derivado de norma. |
| `nwk_util.h`, APS headers, ZMAC files | `REQUIRES_LICENSE_REVIEW` / `REFERENCE_ONLY` | Cabeceras con TI Limited License. Declaraciones no son implementaciones. No copiar. |
| Campos/constantes protocolarios | `INDEPENDENT_IMPLEMENTATION_FROM_SPEC` cuando se deriva y especifica limpiamente | Los formatos se documentan en especificación licenciada; no transportar arrays/structs/comentarios/selección distintiva source sin consejo. Norma/copyright/patentes siguen aparte. |
| Derivar Python por traducción de algoritmo TI restringido | `REQUIRES_LICENSE_REVIEW` | Traducción no se etiqueta automáticamente independiente; la licencia define derivados de source y tratamiento depende de hechos/jurisdicción. |

## 8. Ecosistema y licencias de comparadores

Estos paquetes no entran en el runtime ni en dependencias de distribución del decoder propuesto. El registro 1.1 fija Scapy v2.8.0 / commit `e2e35c07…`, zigpy 2.4.1 / `10b68377…`, WHAD v1.2.18 / `9d480163…`; son referencias de comparación, no ejecución verificada en esta investigación. KillerBee ya está como extra opcional FeralRF, separado del decoder. Wireshark se considera herramienta externa PCAP, con versión/configuración por fijar antes de una futura prueba.

| Herramienta | Licencia comprobada/declarada y uso | Consecuencia |
|---|---|---|
| Scapy | Pyproject upstream declara GPL-2.0-only; el snapshot registrado dice comparator, no runtime dependency. | Riesgo de incompatibilidad si se importa/bundlea como parte del paquete GPL-3.0; mantener fuera del runtime/distribución y evaluar distribución del comparador por separado. |
| zigpy | Repositorio declara GPL-3.0; es stack de controlador y cross-check, no oracle de raw frames. | No es necesario incorporarlo; revisar dependencias y corpus si se ejecuta externamente. |
| Wireshark | README upstream dice aplicación GPLv2 y que la mayoría, no todos, sus componentes son GPL-2-or-later; cada archivo puede diferir. | Usar ejecutable separado sobre PCAP no equivale automáticamente a embedding; fijar versión/decoder prefs y revisar distribución del ejecutable. |
| WHAD | Upstream declara MIT; paquete/framework externo, sin adaptador FeralRF demostrado. | No necesidad de dependencia; MIT no suple compatibilidad técnica. |
| KillerBee | Upstream señala BSD en su licencia; FeralRF ya declara extra opcional `killerbee>=3.0.0`, pero no se ha fijado versión de ese dependency aquí. | Mantener adapter existente independiente; auditar resolución instalada y redistribución si se distribuye paquete agregado. No importar a módulo decoder. |

Invocar herramienta por proceso separado y consumir salida no es automáticamente lo mismo que copiar/embedding, pero tampoco constituye inmunidad general: revisar jurisdicción, linking/IPC, distribución y material resultante. Guardar tool version, decoder fields, preferences, FCS policy y commandline antes de usarla como comparator. No incluir output derivado que incorpore material protegido sin revisión.

## 9. Matriz de decisiones arquitectónicas

| Decisión | Posición propuesta 1.1 | Evidencia nueva | Recomendación y alternativas | Limitación / aprobación / bloqueo |
|---|---|---|---|---|
| `R-ID-01` | Clasificador anterior a parser pero consulta NWK aparente | El flujo es duplicativo/circular; una lectura NWK acotada basta | Elegibility MAC → single NWK parser → classifier sobre `NwkResult`; separar `status` de classification; renombrar `confirmed_by_context` a `verified_by_provenance`. | Cambio de arquitectura del requisito, no del runtime. Supervisor debe aprobar semántica; implementation no empieza antes de fijar schema. |
| `CaptureContext` | `phy_confirmed` más policies declaradas por caller | `Radio` conoce configuración de host; no per-packet PHY/FCS telemetry; ACK no confirma hardware | Snapshot con `value/evidence/source/epoch`; cuatro categorías de provenance; no cambiar Packet. | Contexto ausente/conflictivo bloquea parsing semántico pero no raw; inventar confirmación está prohibido. |
| Core | R22-aligned provisional | TI release notes R22; CSA Core `05-3474-22` R22 1.0, 2017-04-19 | Fijar R22 1.0, no R22.1/R23. | Requiere copia lícita/hash/errata y aprobación de alcance; no Certification. |
| IEEE | No resuelta | Annex D R22 declara IEEE 802.15.4-2015 | Fijar 2015 como base MAC, reconcile corrigendum/amendments for subset. | Texto IEEE de pago; adquisición/licencia y coverage MAC pendientes. |
| ZCL | ZCL7 provisional | `07-5123-07` metadata, TI Z-Stack release; command/sentinel rules extracted | Fijar R7, corregir rules y table citations. | Validar la totalidad de tipos/flags/direction bajo texto controlado. |
| Perfil APS `0x0104` | Deshabilitado/candidato | R21 Table 2.44 = Common (HA); app profile 2016 and vendor docs corroborate; not established by SDK or literal R22 | Recomendarlo como primer profile contextual para generic foundation subset, no cluster schemas. | Condicional a profile reference, APS fields/endpoints, spec rights, supervisor approval and fixtures. |
| Seis ZDO | IDs/layout candidates from TI | R22 §2.4 shows IDs and formats; TI TSN extraction | Mantener; mark field-level status/count/conditional checks crosswalk-required; no add request parsing/correlation baseline. | Fixture/corpus exact after full schema audit; status errors partial/raw-safe. |
| Ocho ZCL | Selected list, some schema provisional | ZCL7 Table 2-3 and §§2.5, 2.6; success Write response and short strings clarified | Mantener bounded subset; exact common command schemas; no cluster-specific. | Full type invalid values, command direction and fixtures still due; enable only approved profile. |
| Security baseline | No decrypt | R22 security describes NWK/APS security and CCM procedure; encrypted NWK payload hides APS when confidentiality set | Scope A retain; Scope B separately approved. Security header-visible parsing distinct from plaintext. | Real traffic utility reduced. Claims of typical ZDO/ZCL need OTA corpus; payload decode requires keys/decrypt under B or plaintext fixture. |
| TI reuse | TI source cross-check | Individual headers/manifest show mixed licensing and unbounded APIs | Independent spec-derived implementation. `zcl.c` BSD direct reuse potentially permitted but not technically recommended; restricted files no copying absent legal clearance. | No legal clearance opinion; legal review before any source reuse/derivative/distribution. |
| Comparators | Secondary tools recorded | Upstream license metadata; version details mostly prior-pinned | External process only; not vendored or imports in runtime. | Fix exact releases/config/PCAP corpus and legal distribution before V-COMP. |
| Validation | Proposed/future | No tests run; R22/ZCL7 provide oracles but input provenance remains | fixtures first for parser; host API regression/backlog and capture/FCS separately; OTA tests only authorized/later. | All claims remain not implemented/not validated. Supervisor approves whether Scope A answers academic question. |

## 10. Validation criteria and unresolved dependencies

Before claiming implementation, version and legally locate the standards copies and errata; complete table-to-norm crosswalk for the six ZDO schemas and eight ZCL commands/types; implement single bounded NWK parse and corpus proving one parser invocation; build offline known Zigbee/known non-Zigbee/ambiguous/invalid/truncated/security fixtures; compare offsets to normative-derived expected values; add fixture provenance/hash; demonstrate unknown/raw bytes remain unchanged; and test import/API/backlog under sustained host capture without changing RF contract. These are prospective checks, not executed here.

For any OTA claim, record transmitter identity, firmware/build, channel/PHY, timestamp source, frame bytes, FCS policy, independently observed capture, and criteria. Separately observe command accepted, RF completion/OTA observed, MAC ACK, and application response. Current host ACK is accepted/queued, not terminal TX complete. No proposed parser can supply unavailable telemetry.

Remaining blockers: legal access/retention of exact CSA/IEEE specifications; formal file-level legal interpretation for any TI source-derived work and GPL redistribution; complete normative crosswalk and test corpus; source/per-packet context proving IEEE PHY/FCS and packet epoch; real network captures showing value of Scope A; consent/privacy/authorization; approved `SchemaProfile` and supervisor scope choice; future independent implementation and tests. Nothing is newly validated.

## 11. Fuentes primarias (consultadas 2026-11-04)

### Normativa / standards owner

| Fuente | URL y versión | Uso / cautela |
|---|---|---|
| CSA Zigbee Core | [05-3474-22, Revision 22 1.0, 2017-04-19](https://zigbeealliance.org/wp-content/uploads/2021/10/docs-05-3474-22-0csg-zigbee-specification-1.pdf), especialmente §2.4, §3.3, §4, Annex D §§D.1–D.3 | El texto indexado/extraído verifica ID, revisión, campos/IDs ZDO citados y que Annex D aplica a IEEE 802.15.4-2015. CSA copyright; obtener/usar copia sujeta a términos. No copiar texto extenso. |
| CSA ZCL | [07-5123-07 Revision 7](https://www.zigbeealliance.org/wp-content/uploads/2021/10/07-5123-07-ZigbeeClusterLibrary_Revision_7-1.pdf), §§2.5.1–2.5.14, §§2.6.2.11–2.6.2.12 y Table 2-3 | Texto extraído verifica commands, Write Response success record, short string sentinels; © 2007–2018 Zigbee Alliance. No dominio público. |
| CSA histórico perfil | [ZigBee PRO 05-3474-21 Revision 21, Table 2.44](https://zigbeealliance.org/wp-content/uploads/2019/12/docs-05-3474-21-0csg-zigbee-specification.pdf) | La tabla llama a `0x0104` Common (HA). Historical reference supports ID/name, not entirety of R22 current profile semantics. |
| CSA application profile | [Lighting & Occupancy Device Specification 15-0014-05, 2016-02-24](https://www.zigbeealliance.org/wp-content/uploads/2019/11/docs-15-0014-05-0plo-Lighting-OccupancyDevice-Specification-V1.0.pdf) | Primary profile source listed by complementary research; archived link currently redirects via CSA site in web inspection. Do not claim clause-level local verification until authorized copy checked. |
| CSA terms/download | [Terms of Use](https://csa-iot.org/terms-conditions/), [Specification Downloads](https://csa-iot.org/developer-resource/specifications-download-request/) | Terms as accessed: website material copying/republication restricted absent authorization; specs downloadable subject to terms/notices; other IP not granted. Terms can change; not a full rights opinion. |
| IEEE | [IEEE 802.15.4-2015 record](https://standards.ieee.org/ieee/802.15.4/5788/) | Published 2016-04-22; superseded by 2020; record exposes amendment/corrigendum resources and purchase/subscription access. Underlying document rights remain licensed. |
| CSA trademark | [Trademark and Logo Usage Guidelines](https://csa-iot.org/wp-content/uploads/2021/12/CSA-Trademark-and-Logo-Usage-Guidelines-05.06.2021.pdf) | Do not claim certified status/logo absent authorization. Use neutral description “analizador de tramas con estructuras compatibles”; no conformance claim. |

### TI / software provenance

| Fuente | URL/path/version | Uso / cautela |
|---|---|---|
| Local TI manifest | `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/manifest_simplelink_cc13xx_cc26xx_sdk_8_30_01_01.html`, manifest ID SRAS00012194; [official manifest](https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/8.30.01.01/exports/manifest_simplelink_cc13xx_cc26xx_sdk_8_30_01_01.html) | Z-Stack 8.30.00.x entries both TI Text File License (binary/limited source) and BSD-3 Clause (source); see legend and license clauses. Manifest scope not file-resolved. |
| Local SDK license | `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/license_simplelink_cc13xx_cc26xx_sdk_8_30_01_01.txt` | Installed TI Text File License, copyright 2015–2024; the Z-Stack manifest excerpt names TI Z-Stack license copyright 2015–2021 and BSD TI copyright 2015–2020. Preserve version-specific record. |
| TI Z-Stack release notes | [8.30.00 Release Notes](https://software-dl.ti.com/simplelink/esd/simplelink_cc13xx_cc26xx_sdk/8.30.01.01/exports/docs/zigbee/release_notes_zigbee_8_30_00.html) | Reports Zigbee PRO 2017 R22/ZCL7; supports target choice, not decoder presence. |
| SDK source | All exact file/function/line references in §7 table; root `SDKROOT` is pinned submodule path above | Each file header inspected directly. `nwk.c` absent; `aps_mede.h` present; `NLDE_ParseMsg` and `APSDE_ParseMsg` declarations only. |
| FeralRF licenses | root `LICENSE`; `python/pyproject.toml:5-20` | Root GPL-3-or-later text; Python metadata GPL-3.0. Check file attribution individually before distributing a combined artifact. |

### Comparators / official upstream sources

| Fuente | URL/version recorded | License/provenance use |
|---|---|---|
| Scapy | [v2.8.0 registered commit `e2e35c07bce2f40e7513e1b29a6d300d5329dbc3`](https://github.com/secdev/scapy/tree/e2e35c07bce2f40e7513e1b29a6d300d5329dbc3), upstream [`pyproject.toml`](https://github.com/secdev/scapy/blob/master/pyproject.toml) | GPL-2.0-only. Do not embed/import by default in FeralRF GPL-3.0 package; external execution still reviewed by distribution arrangement. |
| zigpy | [2.4.1 registered commit `10b68377bcbd9a8b15cbbfb506cebdc450704614`](https://github.com/zigpy/zigpy/tree/10b68377bcbd9a8b15cbbfb506cebdc450704614) | Upstream declares GPL-3.0; valuable schema/controller comparison but not raw MPDU identity oracle. |
| WHAD | [1.2.18 registered commit `9d48016375cd2fdae58517fe6f2fb97022c3bb61`](https://github.com/whad-team/whad-client/tree/9d48016375cd2fdae58517fe6f2fb97022c3bb61) | Upstream repo says MIT; version/commit retained from register; not required integration dependency. |
| Wireshark | [Zigbee NWK reference fields](https://www.wireshark.org/docs/dfref/z/zbee_nwk.html), [README licensing](https://github.com/wireshark/wireshark/blob/master/README.md) | Primarily GPL-2-or-later, with file-level exceptions; release not fixed. Use external tool only after fixing version, dissector prefs, FCS and link type. |
| KillerBee | [upstream](https://github.com/riverloopsec/killerbee), FeralRF optional dependency in Python metadata | Upstream DEVELOPMENT says BSD and points to LICENSE; exact installed version not pinned by this review. Existing adapter is separate from decoder. |

## 12. Decisiones que requieren supervisor

1. Aprobar Core R22 1.0 / ZCL7 / IEEE 802.15.4-2015 como perfil acotado; aceptar trabajo condicionado a adquisición autorizada y errata/corrigendum.
2. Aprobar `verified_by_provenance` en lugar de `confirmed_by_context`, con parser NWK único y classification separada del parse status.
3. Aceptar `0x0104` como primer APS profile contextual sólo para foundation ZCL genérica y permitir semántica únicamente con schema/fixtures aprobados; alternativamente dejar allowlist desactivada.
4. Confirmar que Scope A (sin decryption) responde al objetivo académico aunque ZCL real en red protegida quede opaco; Scope B debe ser una fase separada explícita.
5. Aprobar política de implementación independiente y no copiar código TI salvo autorización legal específica; confirmar política institucional para distribución GPL y normas/licencias.
6. Aprobar corpus autorizado de fixtures/OTA y reglas de privacidad, anonimización y conservación.

## 13. Conclusión

La estrategia técnica no requiere rediseño: FeralRF puede seguir como firmware y API host existentes, añadiendo más adelante un módulo Python opt-in sobre `Packet.data`. Los cambios necesarios son precisión de clasificación/provenance y límites de claim; no integración de Z-Stack runtime. Normativa accesible ahora permite especificar mejor el primer subconjunto. La licencia refuerza la elección de implementación independiente, pero deja cuestiones de derivados TI y uso/redistribución de normas a revisión especializada. La arquitectura es aprobable condicionalmente; implementación de parser, decisión final de profile, corpus, licencias y validación aún no están cerrados.
