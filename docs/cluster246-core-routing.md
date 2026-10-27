# Cluster: disparo, liberación y estado compartido entre seis voces

Se reconstruyen las rutas de **Core note-on `0x01967854`** y **Core release
`0x019678f0`**, especializadas para Cluster. También se ejecuta la factoría Core
`0x01967960` con el **tipo interno 4** para comprobar sus seis punteros de objeto.
El render ya reconstruido se contrasta ahora intercalando los seis slots, con un
único contexto y RNG compartidos.

**El orden de eventos y renders lo proporciona el harness. No se reconstruyen aquí
el asignador, el reloj, la preparación de pitch, la conexión ADSR ni la mezcla de
salida del instrumento.** Cluster continúa aislado del selector del plugin.

## Reglas observadas

| Ruta | Comportamiento observado |
| --- | --- |
| Note-on | Compara el slot de sonido cargado en `0x01aaac0c` con el seleccionado en `0xff8016d4`. Si difieren devuelve `0`, sin llamar al reset. Si coinciden despacha el callback `+0x1c` y devuelve `1`. |
| Controles de nota | Usa el bloque de ocho palabras del sonido cargado en `0xff8013c4 + slot × 16`. El callback de Cluster lee sus cuatro primeras palabras. |
| Release | Despacha `+0x2c` sobre la voz cargada sin exigir que el sonido seleccionado coincida. El callback establece el flag de release y el campo de 16 bits en offset 86 a `256`. |
| Factoría Core | Escribe seis punteros en `0x01aaad18`: `0xffb00000 + voz × 108`, para índices 0–5. Construye los objetos conservando el relleno ya documentado. |
| Recursos compartidos | Reset y render avanzan el mismo LCG. Pesos y display se comparten entre slots; el orden de los callbacks afecta al estado posterior. |

`Source/Cluster246Routing.h` conserva esa distinción entre disparo y liberación.
Sus guards para índices o controles inválidos son protecciones del adaptador C++;
no emulan los asserts originales. El bool de release también pertenece al
adaptador: no se le atribuye un valor de retorno al callback void del firmware.

## Contraste con instrucciones originales

- **256 casos independientes**, con memoria y RNG explícitos, disparos aceptados
  y rechazados, release y reconstrucción de los objetos.
- **12 secuencias continuas de 64 bloques**, alternando orden ascendente y
  descendente de slots y conservando el pool y el RNG entre eventos. Cada registro
  solicita el render de una voz, no una mezcla polifónica completa.
- **131.072 muestras y 1.024 imágenes completas**: 648 bytes del pool, 32 del
  contexto y cuatro del RNG; además 160 bytes de punteros, IDs y controles fuente.
- **304 disparos aceptados, 152 rechazados y 42 liberaciones con distinto sonido
  seleccionado**. Se comprueban 157 llamadas de factoría dentro de los registros,
  además de la factoría inicial del harness.
- Solo se seleccionan las normalizaciones conocidas de **1, 4, 5, 6 y 7
  osciladores**. Las de 2, 3 y 8 siguen sin recuperarse. Pitch y programación de
  eventos son entradas de prueba; no se deducen presets de ellas.

Dos ejecuciones del oráculo en directorios privados producen los mismos vectores.
El modelo es GNU BF524 con la corrección de registros D documentada anteriormente;
esta coincidencia no prueba temporización ni fidelidad acústica del aparato completo.
[Resultados, hashes y límites](firmware/cluster246-routing-validation.json).

Pasan las **17 suites de núcleo en Linux**, la nueva prueba compilada para Windows
con el toolchain MSVC y ejecutada bajo Wine, y ASan/UBSan del conjunto nuevo.
LeakSanitizer se desactiva por la limitación de ptrace del sandbox. No se ha
realizado una prueba nueva en Ableton/ASIO ni se atribuye a Wine esa validación.

## Apoyo de REA e identificadores

REA 6.8.0 inspeccionó en lectura el esquema de `op1_factory.db` y las columnas
`id`/`type` de `synth_types`, con límite de 32 filas. Devolvió diez filas sin
truncamiento; Cluster figura con **ID de base de datos 1**. Ese ID no es el tipo
interno DSP **4**, comprobado al ejecutar la factoría y sus seis constructores.
Tampoco son IDs de motor los índices de sonido 0–7 ni los índices de voz 0–5.

Se conserva el digest de la base y las evidencias de REA, sin distribuir la DB ni
leer filas de presets para esta ampliación. El resumen público contiene únicamente
la observación de Cluster y su alcance.
[Procedencia REA](firmware/rea-cluster-routing-support.json).

## Reproducir

```bash
python3 scripts/verify-cluster246-routing.py \
  --ldr /privado/OP1_vdk.ldr \
  --assembler /herramientas/bfin-as --linker /herramientas/bfin-ld \
  --simulator /herramientas/run-dreg-deferred \
  --testutils /fuentes-gdb/sim/testsuite/bfin \
  --output /privado/cluster-routing-nuevo

cmake -S . -B build-dsp -DPOCKET_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-dsp --target cluster246_routing_tests
ctest --test-dir build-dsp -R '^cluster246_routing$' --output-on-failure
```

El directorio de oráculo debe ser nuevo y estar fuera del checkout. La entrada
contiene 144 bytes de coeficientes explícitos y registros de 1.340 bytes; la salida,
registros de 1.104 bytes. Solo se publican código/harness propios, observaciones
numéricas y metadatos. No se incluyen instrucciones originales, firmware ni ELF.
No cambia el instrumento ejecutable ni se genera un nuevo instalador por esta
ampliación de investigación. [Siguiente trabajo de Cluster](firmware-dsp-workplan.md).

La siguiente ampliación contrasta las [transiciones del ADSR común](cluster246-envelope-transitions.md)
como dependencia de la voz. Su solicitud de reinicio conserva nivel y ganancia;
el consumo de sus palabras pendientes y la conexión con Cluster siguen abiertos.
