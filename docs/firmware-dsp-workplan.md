# Plan activo: reconstrucción del DSP del firmware

**Prioridad acordada: motores, filtros, envolventes y moduladores originales.**
Los presets, bancos, recetas del manual y la comparación con vídeos pasan a una
etapa posterior. Se conserva la investigación previa como referencia, sin usarla
para dirigir ajustes de sonido durante esta etapa.

## Un componente activo y criterios de cierre

Se trabaja un bloque DSP cada vez. Un bloqueo se documenta con su dependencia
concreta; no se rellena con constantes o algoritmos supuestamente originales.

1. **Contrato:** localizar entradas, controles, memoria, tablas, salidas y callbacks
   en el firmware. Separar datos observados de hipótesis.
2. **Datos:** reproducir las tablas/coeficientes usados por ese bloque, incluyendo
   inicializadores y conversiones. Un buffer rellenado por el harness es una
   entrada de prueba, no un dato recuperado.
3. **Oráculo:** ejecutar las instrucciones originales aisladas con límites,
   barridos, estados independientes y secuencias continuas. Comparar muestras y
   estado completo, incluidos RNG y efectos laterales compartidos.
4. **Reconstrucción:** implementar el bloque C++ independiente y comprobar su
   coincidencia. Repetir el oráculo en otro directorio privado y ejecutar pruebas
   Linux/Windows y sanitizadores aplicables.
5. **Integración:** introducir el bloque en el motor de audio cuando sus
   dependencias estén verificadas. Comprobar asignación de voces, orden/cadencia,
   cambios de controles, tasas, note-off/pánico y ausencia de asignaciones en el
   callback. Conservar IDs de automatización/MIDI y sesiones anteriores.
6. **Distribución:** generar instaladores cuando cambie el instrumento ejecutable.
   Una prueba de núcleo aislado no se presenta como un motor completo instalado.

## Bloque activo: Dr. Wave rev246

Se avanza con Dr. Wave por el bloqueo concreto de las dependencias restantes de
Cluster. Su ruta de lectura es SDRAM legible y permite contrastar otro núcleo sin
inventar coeficientes ni ejecutar instrucciones cifradas.

| Parte | Estado |
| --- | --- |
| Lectura, mezcla de cinco osciladores y desviación de pitch | 241.920 muestras contrastadas con ondas sintéticas explícitas. Fases de 24 bits y refinamiento entero del recíproco conservados. |
| Objetos y contexto compartido | 2.048 pools de seis slots; 293 construcciones, 121 resets y 187 releases; cinco incrementos, semillas y puntero compartidos contrastados. |
| Generación de tablas base | Seis bancos / 320 ondas generados desde el seno contrastado; 18 secuencias de arranque y 1.474.560 muestras de tabla coinciden con las rutinas originales. Valores predeterminados y preservación de memoria compartida contrastados. |
| Transformación de ondas | Tres callbacks completos contrastados: retención por grupos, extremo fijo y repetición; 131.072 palabras / 512 llamadas, seis ondas y cachés compartidas. La palabra Q16 y la elección de callback son entradas explícitas. |
| Mezcla espectral | Seis callbacks: 768 llamadas / 196.608 palabras, bancos generados y preservados, reemplazo/adición y seis slots contrastados. |
| Render completo, actualización y controles | 61.568 muestras / 512 registros: 240 actualizaciones, 241 lecturas activas y 31 retornos inactivos. Preparación de cuatro controles, dos familias, inversión de 25 bits, blanco opcional, retención común y cinco osciladores contrastados juntos. |
| Consumo del contador externo | Los 65.536 valores raw contrastados dos veces con vecinos distintos; el resto es sin signo. 61.568 muestras adicionales de render completo cruzan los límites 32767/32768 y 65535/0. El adaptador admite toda la palabra; no establece el reloj original. |
| Eventos Core y seis voces | Factoría tipo interno 2, note-on condicionado al sonido seleccionado y release sin esa condición; 61.568 muestras adicionales junto al render completo. 194 disparos aceptados, 86 rechazados, 122 releases y pools previos al render contrastados. Release de motor no equivale a release ADSR. |
| Activación VoiceCtl | Tabla de 128 notas, pitch, velocidad y actividad contrastados con afinación explícita. 427 activaciones aceptadas / 85 bloqueadas y 61.568 muestras posteriores con pitch mantenido por el harness; no es el proveedor de pitch original. |
| Actualización de pitch VoiceCtl `0x01a85cbe` | 512 llamadas con listas de 0–128 entradas: destino selectivo, inactivo y broadcast contrastados, más 61.568 muestras posteriores. Afinación y copia del registro al buffer siguen explícitas del harness; otras rutas de pitch y el relevo original al render permanecen pendientes. |
| Entrada de pitch bend VoiceCtl `0x01a84efa` | 65.536 parejas de bytes y 256 llamadas continuas contrastadas, incluida la preservación de seis registros de pitch. Control signed16 y tres flags; consumidor candidato en L1 parcialmente cifrada. No establece bend → frecuencia ni añade audio/integración. |
| Índice de afinación VoiceCtl `0x01a842f0` | Setter contrastado con estrés signed32, 4.577 parejas de controles y 256 llamadas continuas. Otras 61.568 muestras verifican activación/render con coeficientes explícitos en índices calculados; no recupera los valores originales de la tabla ni el escritor del buffer. |
| Productor de la tabla común de afinación | Cola SDRAM `0x01a85454` contrastada: 2400 coeficientes, tres precargas y otras 61.568 muestras de activación/render sin overrides de coeficiente. Se omite el setup previo; escritor del buffer y cadencia siguen pendientes. |
| Refresh de pitch VoiceCtl `0x01a854cc` | Rutina completa contrastada con tabla generada, seis ADSR, lista/una/seis voces, gates Core y RNG; 61.568 muestras posteriores. Módulos auxiliares Core explícitamente ausentes; render ADSR y escritor del buffer siguen pendientes. |
| Modificador de pitch de cuatro entradas `0x019cce68` | Ramas pitch/desactivadas contrastadas: 65.536 palabras y 61.568 muestras posteriores, 16 máscaras de canales y saturación serial. Consume un buffer ya lleno; fuentes, orden y contador explícitos. No completa su productor, otros destinos ni la identidad del módulo. |
| Entrada de cuatro canales a parámetros ADSR | Callback `0x019cce68`, destino ENV 0–3, seguido del render ADSR: 1.024 llamadas / 131.072 muestras, estado y RNG contrastados. Parámetros iniciales/cadencia explícitos; auxiliar 4 y relevo por voz pendientes. |
| Selector auxiliar ADSR en contexto Dr. Wave | Color 4 del mismo callback: rango original generado, liberación rápida de seis estados al cambiar nibble y 131.072 muestras ADSR contrastadas; pool completo y scratch dither compartido. Tipo 2 y pool inicializado explícitos; callbacks de tipo 4 y relevo por voz excluidos. |
| Modificador de volumen/pitch en serie | 81 combinaciones, 65.536 muestras de volumen y 65.536 palabras de pitch; 61.568 muestras Dr. Wave posteriores. Cuatro suavizados compartidos contrastados con segundo parche GNU de cargas halfword, verificado con probes propios. Ambos buffers iniciales explícitos; no completa mezcla/salida. |
| Entrada de control a cuatro fuentes del modificador | Ruta válida `0x01a84f8c`: 32.768 parejas + 256 llamadas continuas, tabla completa contrastada. Otros 2.048 decodes alimentan volumen/pitch y 61.568 muestras Dr. Wave; status/canal, inicializador y scheduler no recuperados. |
| Destino instrumento en contexto Dr. Wave | Rangos originales generados, ocho selectores hacia cuatro controles, wrap/clamp y 61.568 muestras posteriores contrastados. Tipo 2, parámetros iniciales y contador explícitos; no completa dispatcher ni integración. |
| Cadencia externa, voz completa e integración | Referencia candidata al contador en L1 junto a franjas cifradas; proveedor/cadencia aún sin verificar. Faltan también asignación/pitch/ADSR y salida antes de integrar el núcleo. Solo se usa desde pruebas; no cambia el instalador. |

[Lectura y mezcla de Dr. Wave](drwave246-reader.md) ·
[Generación de los seis bancos base](drwave246-wavebank.md) ·
[Tres transformaciones de la onda de voz](drwave246-wave-transform.md) ·
[Mezcla espectral y render aislado completo](drwave246-kernel.md) ·
[Contador de 16 bits y dependencia del proveedor](drwave246-counter.md) ·
[Factoría, disparo y release de seis voces](drwave246-core-routing.md) ·
[Preparación y activación VoiceCtl](drwave246-note-activation.md).

La ruta candidata del proveedor del contador continúa bloqueada en L1
parcialmente cifrada. El siguiente paso verificable del mismo componente es
contrastar las demás rutas de actualización del pitch de VoiceCtl y resolver su
relevo al buffer del render. La [actualización selectiva/broadcast](drwave246-pitch-update.md)
ya está contrastada; la referencia candidata al relevo está en L1 parcialmente cifrada.
La activación inicial está contrastada con afinación explícita;
esa entrada no recupera la calibración original ni suple el relevo ADSR pendiente.

La [entrada de pitch bend](drwave246-pitch-bend-input.md) está contrastada como
decodificador independiente. Su control no se conecta a los registros de pitch
ni al render con una fórmula supuesta; su consumidor original sigue pendiente.

El [setter del índice](drwave246-tuning-index.md) y el
[productor de las 2400 entradas](drwave246-tuning-table.md) ya están contrastados
junto a su consumo en la activación de Dr. Wave. La tabla deja de ser una entrada
de coeficientes explícitos en el nuevo ensayo. Setup completo, calibración física,
relevo L1 y demás rutas de pitch permanecen pendientes.

La [repreparación de pitch por lista/modo](drwave246-pitch-refresh.md) ya está
contrastada en el contexto acotado de módulos Core auxiliares ausentes. Incluye
callbacks ADSR y RNG compartido, sin completar render/relevo ADSR, asignador,
cadencia, salida ni el escritor original del buffer. Se mantiene Dr. Wave activo.

El [modificador de cuatro entradas](drwave246-pitch-buffer-modifier.md) ya está
contrastado sobre el buffer y seguido del render. No sustituye el escritor
inicial bloqueado en L1. Sin hardware ni volcado L1 descifrado disponible, el
siguiente avance verificable debe limitarse a otra dependencia legible del
buffer o del relevo ADSR, conservando explícitas las entradas y la cadencia.


La [entrada ENV de cuatro canales](drwave246-envelope-control-input.md) está contrastada
junto al render ADSR común. El [color auxiliar 4 en contexto Dr. Wave](drwave246-envelope-mode-control.md)
ya está contrastado con seis estados ADSR y scratch compartido; no se completa el relevo
por voz, el llenado inicial de pitch ni la salida de Dr. Wave. El [destino de volumen y las combinaciones con pitch](drwave246-volume-buffer-modifier.md)
ya están contrastados sobre buffers preparados, con suavizado compartido y un
segundo parche GNU acotado para cargas halfword. Se mantienen pendientes los
productores iniciales, relevo por voz y salida. La [entrada de control hacia las cuatro fuentes](drwave246-control-source-input.md)
ya está contrastada junto a su consumo por el modificador. Dispatcher MIDI,
inicializador de fuentes y reloj siguen pendientes. El [destino instrumento para el tipo interno 2](drwave246-instrument-control-routing.md)
ya está contrastado con sus rangos originales y el render posterior. El siguiente
contrato puede combinar destinos instrumento/volumen/pitch en una misma llamada,
manteniendo explícitos buffers, selección y orden; no completa el instrumento.

## Cluster rev246: conservado con dependencias bloqueadas

| Parte | Estado |
| --- | --- |
| Render de ocho osciladores, realimentación y mezcla | Reconstrucción aislada contrastada con coeficientes explícitos del llamador. |
| Estado de 108 bytes, reset/release, RNG y contexto compartido | 2.048 bloques / 262.144 muestras; estado completo contrastado. |
| Rangos de los cuatro controles | Contrastados con el inicializador original. |
| 24 coeficientes de amortiguación | Conversión original float → Q31 contrastada. |
| Normalización | Cinco entradas legibles convertidas por el helper original; faltan las de 2, 3 y 8 osciladores. Otras 81.920 muestras contrastadas con las entradas conocidas. No se extrapolan las desconocidas. |
| Factoría de objetos | Seis slots de 108 bytes: 60 casos contrastados de construcción/reset, vecinos, relleno y RNG. No completa el asignador/cadencia. |
| Inicialización compartida | Rutina completa contrastada en 256 casos con ocho palabras fuente explícitas; pesos/display a cero y campos/RNG/amortiguación no escritos preservados. No completa la tabla original. |
| Conversión de tablas | Helpers Q15/Q31 contrastados en 65.536 patrones binary32: 131.072 resultados. Coinciden también con GNU sin el parche local para estos casos. |
| Cadencia, pitch por muestra, envolvente/ruteo y salida completos | Pendientes. |
| Disparo y liberación desde Core | Note-on condicionado a coincidencia entre sonido cargado y seleccionado; release sin esa condición. Factoría Core tipo 4 y seis punteros contrastados. |
| Estado compartido entre voces | 131.072 muestras adicionales y 1.024 pools completos, con seis slots intercalados y un RNG/contexto compartidos. Orden externo explícito; no completa el scheduler original. |
| Dependencia ADSR: inicialización y reinicio | Callbacks y rampa de etapa 6 → 7 → ataque contrastados: 327.680 muestras / 2.560 registros. Conservación de nivel/ganancia y almacenamiento de palabras pendientes. No completa su consumo ni el ruteo con Cluster. |
| Preparación/activación desde VoiceCtl | Tabla de 128 notas y preparación de pitch/velocidad/actividad contrastadas, con afinación explícita. 131.072 muestras posteriores con incremento mantenido por el harness. Calibración/afinación original y proveedor de pitch por muestra pendientes. |
| Consumidor de campos ADSR pendientes | Referencias candidatas localizadas en L1 parcialmente cifrada; faltan instrucciones verificadas del relevo. No se ejecuta ese tramo ni se supone su flujo. |
| Integración en el selector del plugin | Pendiente; el núcleo aún no se expone como motor original completo. |

El siguiente paso de **este bloque** es resolver la procedencia/inicialización de
las tres entradas restantes de normalización, la calibración de afinación y el
relevo L1 pendiente, y contrastar el contrato completo
antes de conectarlo al plugin. [Avance de normalización/inicialización](cluster246-normalization.md) ·
[Inicialización compartida y apoyo de REA](cluster246-shared-init.md) ·
[Disparo, liberación y seis voces intercaladas](cluster246-core-routing.md) ·
[Transiciones del ADSR común](cluster246-envelope-transitions.md) ·
[Preparación de nota y bloqueo del relevo L1](cluster246-note-activation.md) ·
[Contrato y reproducción del núcleo](cluster246-kernel.md).

La búsqueda de afinación ha identificado un inicializador en `0x01a866e4` que
construye **2.400 palabras de cuatro bytes** desde `0x027fc720`, usando el callback
`0x01a84388`: este solo escribe cero. El bloque LDR que cubre esa memoria también
es un fill a cero. Ese callback no proporciona una afinación válida. La
investigación posterior de Dr. Wave ha localizado y contrastado el productor en
la cola `0x01a85454`: la tabla común ya se genera, sin completar el setup previo
ni una nueva validación de su consumo por Cluster. Las tres normalizaciones y
el relevo L1 cifrado siguen pendientes; Dr. Wave se mantiene como bloque activo.

## Cola de componentes

La [matriz completa](original-op1-coverage.md) conserva el estado individual. El
orden puede ajustarse si una dependencia cifrada bloquea un componente; se registra
el motivo y el siguiente bloque activo, manteniendo un solo frente.

| Área | Trabajo pendiente que debe contrastarse |
| --- | --- |
| Motores | Completar Dr. Wave; retomar Cluster cuando se resuelvan sus dependencias; después FM y demás motores. Resolver las tablas L1 necesarias; no sustituir un motor por otra síntesis con el mismo nombre. |
| Filtros | Filtros internos de cada motor, ruta Filter/Nitro y etapas de salida/decimación. Cada ruta requiere coeficientes, estado e impulsos/barridos contrastados. |
| Envolventes | Completar cuatro campos ADSR auxiliares, ruteo y modos de ejecución; envolventes específicas de D-synth/D-box/drum. El ADSR común ya contrastado no las completa. |
| Moduladores | Extender Value a rutas pendientes; reconstruir Random/Tremolo y después Element/MIDI/Bend/Crank, con sus destinos, reinicios y fase libre originales. |
| Integración de voz | Preparación de pitch, poly/mono/legato/unison/portamento, asignación y reloj originales, además de mezcla/salida. |

## Aplazado

Identificación adicional y afinado de presets, bancos de samples, recetas de
sonidos, comparación con el vídeo y cambios visuales ajenos a una integración DSP
necesaria. No se añaden presets para presentar avances de esta etapa.

La coincidencia con el modelo GNU documentado prueba los casos aislados bajo ese
modelo. No certifica por sí sola la máquina completa ni temporización/ASIO reales.
