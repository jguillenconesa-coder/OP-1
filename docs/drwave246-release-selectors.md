# Dr. Wave rev246: retirada de notas con selectores 1–4

Se amplía el contrato de **VoiceCtl `0x01a85058..0x01a8541c`** a los selectores
por sonido **1, 2, 3 y 4**, conservando el contexto externo anterior: flag
`0x01dfe730 = 0`, modo VoiceCtl `0xff801038 = 0`, pools Core/ADSR inicializados y
objetos Dr. Wave. Se ejecuta la rutina original desde su entrada y un render
ADSR posterior elegido por el harness. No se ejecuta L1 cifrada.

Los números de selector identifican ramas internas del firmware. No se deduce
su identidad comercial, un asignador completo, legato, glide o cadencia. Las
rutinas continúan aisladas del Engine y no cambian IDs MIDI, automatizaciones,
sesiones, presets ni interfaz.

## Contrato contrastado

El selector es el word signed16 de `0x01b60ef0 + seleccionado × 16 + 8`,
desplazado aritméticamente 12 bits. Los bits inferiores no alteran la rama.
La lista contiene hasta 128 entradas de 20 bytes en `0x01dfe734`, con contador
en `0x01dff134`. Se compara la palabra de nota completa del argumento.

| Situación | Efecto antes del render posterior |
| --- | --- |
| Selector 4 | Conserva lista, objetos, ADSR, pitch y RNG, incluso si hay coincidencias. |
| Selectores 1–3, nota ausente | Conserva las mismas imágenes. |
| Nota encontrada, pero la última entrada no coincide | Retira la primera coincidencia y compacta. No libera ADSR ni modifica pitch/RNG. |
| La última entrada coincide y quedan otras notas | Retira **la última**, aunque una coincidencia anterior haya iniciado la búsqueda. Prepara pitch desde la nueva última entrada y actualiza uno o seis registros. No reinicia el ADSR ni cambia actividad/velocidad. |
| La última entrada coincide y la lista queda vacía | Solicita release Core y ADSR de la voz 0 en selectores 1/2, o de las seis voces 0–5 en selector 3. Pitch/RNG se conservan antes del render posterior. |

Las ramas 1/2 tienen el mismo comportamiento dentro de este contrato. No se
atribuye esa equivalencia a todos sus usos. Los IDs de voz almacenados en las
entradas no seleccionan las voces de release/retune de estas ramas. La rama
[polifónica 0](drwave246-voicectl-poly-release.md) sigue separada y conserva sus
fixtures; allí sí se consulta la voz de la entrada encontrada.

La compactación copia las palabras de offsets 0, 4, 12 y 16 y el byte 8;
conserva el relleno 9–11 del destino y no limpia el antiguo último slot. Cuando
se retira la última entrada solo disminuye el contador.

## Preparación de pitch y efectos compartidos

Se usa la palabra signed32 de pitch explícito de la **nueva última entrada** si
es distinta de cero; en caso contrario, se consulta su índice en la tabla de
128 notas. Los índices fuera de 0–127 solo se ensayan cuando hay pitch explícito.
La tabla se obtiene con el fragmento de copia original ya contrastado en la
activación, seguido de RTS propio: no se ejecuta el inicializador completo.

El ensayo escribe un coeficiente explícito en la tabla de afinación, índice
1200. La preparación conserva el producto Q31 truncado y el desplazamiento
saturado de un bit. No supone calibración física ni un valor predeterminado.

Por cada registro actualizado, incluso la voz 0 cuyo desvío es cero:

1. Avanza el RNG compartido `seed = seed × 1664525 + 1013904223` módulo 2³².
2. Interpreta su mitad alta como signed16 y la desplaza aritméticamente 9 bits.
3. Multiplica ese valor por el índice de voz y forma el desvío signed32 con
   desplazamiento de 16 bits.
4. Escribe `sat32(base + Q31(base, desvío))` en `0xff8012d4 + voz × 4`.

El selector 3 consume seis pasos; 1/2 consumen uno. No se redondea Q31 ni se
reordena el consumo del RNG. El render ADSR posterior continúa desde esa semilla,
por lo que también se compara el dither afectado. Estos son **registros de
pitch**, no el buffer de render `0xff801950`: su productor continúa pendiente.

En la rama de lista vacía se ejecutan los callbacks Core originales de Dr. Wave,
que preservan sus objetos, y el release ADSR con sustain del sonido seleccionado.
La diferencia entre seleccionado y cargado no lo bloquea. El comportamiento del
release ADSR es el previamente contrastado: etapa 4/5 y contador `0x10000000`,
conservando nivel, palabras pendientes y ganancia.

`Source/Voice246ReleaseSelectors.h` implementa solo este contexto y exige la
afinación como entrada. Sus rechazos y enum son propios del adaptador; no emulan
los asserts ni atribuyen un retorno original. Otros selectores, modos externos,
lazy init y rutas de error quedan fuera.

## Ensayo, imágenes completas y límites

- **516 casos independientes**: los cuatro selectores × los 129 tamaños de lista.
  **Cuatro secuencias de 64 llamadas**, una por selector: total **772 registros**.
- **339 retiradas**, 245 de entradas anteriores; 89 casos retiran la última
  aunque exista una coincidencia anterior. El selector 4 y las búsquedas ausentes
  se comparan sin modificar la implementación anterior.
- **90 retunes**: 50 mediante tabla y 40 con pitch explícito; nueve de estos
  últimos ignoran un índice fuera de rango. **240 escrituras de registro** y
  pasos RNG originales, incluyendo extremos/cero de afinación. **Nueve llamadas
  Core/ADSR de release** repartidas en cuatro eventos que vacían la lista.
- **98.816 posiciones ADSR posteriores**: **88.832 escritas** y **9.984
  conservadas**. Se conserva la salida inicial cuando la etapa no escribe.
- Se compara una imagen de **4.464 bytes antes/después** del render: lista
  completa, contador/vecinos, seis objetos Dr. Wave, seis ADSR, scratch y guardas,
  registros VoiceCtl, parámetros de ocho sonidos, RNG/valor anterior y vecinos,
  ready, sonidos, punteros Core, índice y coeficiente de afinación con vecinos.
  Además se comparan salida con guardas, argumento/entradas del llamador y
  retorno ADSR. La tabla de 128 notas se captura completa al principio/final.
- **`0x01dfe72c` es la última palabra de la tabla de notas**, junto al flag/lista;
  se conserva su valor original `610832681`. No es una guarda independiente.
  `0xff800164` vuelve a tratarse como scratch compartido, no guarda del pool ADSR.
- Los streams retienen lista, objetos, pool/scratch y RNG/valor anterior. Los
  demás registros, controles, afinación, sonidos, notas y voz/volumen del render
  son entradas explícitas por llamada. El orden no demuestra el scheduler.

Dos reproducciones privadas coinciden byte a byte. Pasan **42 suites de núcleo**
y **44 con JUCE** en Linux, más ASan/UBSan del nuevo comparador (`detect_leaks=0`
por la restricción ptrace). Windows/macOS quedan pendientes de CI de esta rama.
No se valida integración en DAW, latencia, dropouts ni temporización de hardware.

REA 6.8.0 inspecciona la rutina legible mediante `inspect-with-objdump`; no se
atribuye descifrado ni comparación numérica a REA. El oráculo usa GNU BF524 con
los dos parches documentados de writeback y comprueba los paquetes halfword
propios antes de ejecutar el firmware. No demuestra un pipeline físico completo.
Firmware, instrucciones, desensamblados y ELF permanecen fuera del repositorio.

```bash
python3 scripts/verify-voice246-release-selectors.py \
  --ldr /privado/OP1_vdk.ldr --assembler /herramientas/bfin-as \
  --linker /herramientas/bfin-ld --simulator /herramientas/run-halfword-v2 \
  --testutils /fuentes-gdb/sim/testsuite/bfin --output /privado/selectores-nuevo
cmake --build build-dsp --target voice246_release_selectors_tests
ctest --test-dir build-dsp -R '^voice246_(poly_release|release_selectors)$' --output-on-failure
```

El siguiente contrato legible puede centrarse en la preparación del argumento
de nota que consumen estas rutas (`0x01a85014`), manteniendo aparte el dispatcher
MIDI y el asignador. Siguen pendientes los productores de buffers, el consumidor
de campos ADSR, reloj, mezcla y salida. No se suple L1 con interpolación inventada.

[Evidencia y hashes](firmware/voice246-release-selectors-validation.json) ·
[Retirada polifónica](drwave246-voicectl-poly-release.md) ·
[Plan activo](firmware-dsp-workplan.md).
