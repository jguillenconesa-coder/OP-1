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
| Cadencia externa, voz completa e integración | Siguiente paso: verificar proveedor del contador/cadencia, asignación/pitch/ADSR y contrato de salida antes de integrar el núcleo. Solo se usa desde pruebas; no cambia el instalador. |

[Lectura y mezcla de Dr. Wave](drwave246-reader.md) ·
[Generación de los seis bancos base](drwave246-wavebank.md) ·
[Tres transformaciones de la onda de voz](drwave246-wave-transform.md) ·
[Mezcla espectral y render aislado completo](drwave246-kernel.md).

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
es un fill a cero. No se ha localizado y verificado la rutina que escribe los
coeficientes de uso. Ese inicializador no proporciona una afinación válida.
Junto con las tres normalizaciones y el relevo L1 cifrado, esta dependencia
justifica pasar a Dr. Wave; Cluster mantiene los resultados ya contrastados.

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
