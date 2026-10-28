# Dr. Wave rev246: auditoría del relevo de pitch

Revisión del estado `87f72c171303db09b9a16765b86f233457b75b3d`, realizada
el 28 de octubre de 2026. Se reproducen las regresiones versionadas; **no se
ejecuta un nuevo oráculo de firmware ni se reconstruye el proveedor de pitch**.
Dr. Wave sigue aislado del instrumento.

## Extremos conocidos y conexión pendiente

| Tramo | Evidencia disponible | Límite |
| --- | --- | --- |
| Activación VoiceCtl `0x01a84414` → `0xff8012d4 + voz × 4` | `Voice246Activation.h`, especialización `DrWave246Activation.h` y fixtures de activación: producto Q31 y desplazamiento saturado, solo tras aceptación Core. | Afinación suministrada explícitamente. No prueba actualizaciones posteriores de pitch. |
| Registro por voz → buffer `0xff801950` | El bucle **propio** `held_pitch` en `research/blackfin/drwave-activation.s.in` copia la palabra preparada a 128 posiciones; `Tests/DrWave246ActivationTests.cpp` hace lo mismo con `pitch.fill`. | Es una conexión del ensayo, no una rutina original recuperada. Tras rechazo se conserva el pitch anterior. |
| Buffer `0xff801950` → render `0x0197c870` | El [contrato del lector](drwave246-reader.md) consume solo la primera palabra. Su fixture contiene otras 127 palabras independientes. `drWave246Block` y `drWave246ReadBlock` pasan `pitch[0]` a `readPrepared`. | Un buffer de 128 palabras no implica que este render consuma pitch por muestra. No identifica quién lo produce ni cuándo. |
| Primera palabra → cinco osciladores | `drWave246Increments` desplaza el pitch signed32 ocho bits a la derecha, con signo; prepara cinco incrementos con desviación del naranja. `readPrepared` usa esos incrementos durante las 128 muestras y conserva fases de 24 bits. | No proporciona glide, modulación, asignación, frecuencia de actualización ni reloj originales. |

Los cinco incrementos se recalculan por llamada activa y pertenecen al contexto
compartido; las cinco fases sí pertenecen al objeto de voz. Con render inactivo
no se recalculan esos incrementos ni se limpia la salida. La ruta que actualiza
la onda sigue siendo distinta de la actualización pendiente del pitch VoiceCtl.

Así, las 61.568 muestras del ensayo de activación comprueban activación seguida
de render **con pitch mantenido por el llamador**. No demuestran el tramo entre
los registros por voz y el buffer ni una interpolación original. Tampoco debe
deducirse una cadencia física del tamaño del bloque.

## Próximo experimento cuando estén disponibles los insumos privados

1. Usar el LDR rev246 cuyo SHA-256 está registrado en
   [la evidencia de activación](firmware/drwave246-activation-validation.json).
   Localizar lecturas y escrituras de los seis registros de pitch y del buffer,
   incluyendo referencias indirectas. No atribuir una dirección a una rutina
   de actualización hasta contrastar sus instrucciones y flujo alcanzable.
2. Separar las rutinas legibles de las franjas L1 cifradas usando el mapa LDR.
   Si una dependencia cae en una franja cifrada, registrar el bloqueo; no
   desensamblarla como código ni sustituirla por una interpolación supuesta.
3. Aislar cada candidato con registros, controles y afinación explícitos.
   Comparar imágenes antes/después de los seis registros, las 128 palabras del
   buffer y sus vecinos, más todo estado compartido que toque la rutina.
   Probar voces intercaladas, aceptación/rechazo, repetición sin nueva nota,
   cambios de destino y extremos signed32 en casos independientes y secuencias.
4. Solo después de contrastar al escritor, llamar al render original con el
   buffer resultante **sin el bucle `held_pitch`**. Comparar audio, fases,
   incrementos y estado completo. Hacer variar la cola del buffer de forma
   independiente para distinguir el contrato del productor del del consumidor.
5. Repetir en un segundo directorio privado nuevo. Publicar únicamente
   observaciones numéricas, hashes y llamadores propios; conservar LDR, imagen,
   instrucciones originales y ELF fuera del checkout. Mantener pendientes la
   calibración, ADSR, contador y salida que este experimento no contraste.

Se necesitan el LDR privado, ensamblador/enlazador Blackfin, fuentes de
`sim/testsuite/bfin` y simulador GNU BF524 con el parche de escritura diferida
documentado en [la metodología](envelope246-research.md). No estaban presentes
en este entorno. Las rutas históricas de los JSON no prueban su disponibilidad
en otra cuenta. No es necesaria la base de presets para este experimento.

## Validación de esta revisión

Con GCC 14.2, CMake 4.4.4 y Python 3.12.14 en Linux:

```sh
cd /workspace/Tiny-one
source scripts/cloud-env.sh
cmake -S . -B build-dsp -DPOCKET_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-dsp --parallel 3
ctest --test-dir build-dsp --output-on-failure
ctest --test-dir build -C Release --output-on-failure
```

Pasaron **26/26 suites de núcleo** y **28/28 suites del build con JUCE**, incluidas
`plugin_smoke` y `realtime`. El segundo comando CTest usa el build Linux completo
preparado en este entorno; no configura JUCE por sí mismo. Los tamaños y SHA-256
de ambos fixtures de activación, y todos los hashes de fuentes enumerados en
su evidencia, coinciden con el registro versionado. Estas son regresiones C++
contra observaciones existentes, no nuevas ejecuciones Blackfin.

La página de PR de GitHub mostraba **0 abiertos** al iniciar esta tarea; `main`
coincidía con la revisión indicada arriba. La consulta mediante `gh pr list`
devolvió `403`, por lo que se comprobó la página del repositorio como alternativa.
No se han vuelto a ejecutar Windows/Wine, sanitizadores ni pruebas físicas de
DAW/ASIO en esta revisión. No cambian código DSP, fixtures, IDs MIDI,
automatizaciones, sesiones, presets ni interfaz.
