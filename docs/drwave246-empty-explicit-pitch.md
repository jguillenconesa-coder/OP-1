# Rev246: lista vacía y palabra de pitch explícito no cero

Se amplía el contraste del llamador completo **`[0x01a84604, 0x01a84ba0)`**,
1.436 bytes, a selectores 1–3 con **lista vacía y argumento raw cuyo pitch
explícito es no cero**. El prólogo tiene un marco de 76 bytes; el siguiente
prólogo empieza en `0x01a84ba0`. Las instrucciones originales se ejecutan desde
la entrada completa sin parches, con seis ADSR ready y módulos auxiliares ausentes.

Este caso estaba excluido de [DrWave246NoteModes](drwave246-note-modes.md).
El nuevo adaptador `Source/DrWave246EmptyExplicitPitch.h` reutiliza los componentes
contrastados sin modificar el helper previo ni sus guardas. Su API exige
nota 0–127 para acotar la ganancia, lista vacía, pitch no cero, slots 0–7 y
selectores 1–3; sus rechazos y resultados son propios, no asserts ejecutados ni
retornos semánticos R0 del firmware.

## Selección antes del producto y prueba del bypass

En **`0x01a84800`** se lee la palabra en offset 12 del argumento. Si es no cero,
el salto de **`0x01a84812`** lleva a **`0x01a84822`**, antes del producto Q31.
El bloque **`0x01a84814..0x01a84821`** contiene la alternativa de lookup y no
se alcanza con estas entradas. La inspección no asigna unidades físicas o un
significado comercial a la palabra explícita.

Se interpreta su patrón raw como **signed32**, se multiplica por el coeficiente
de afinación con truncamiento Q31 y se aplica un desplazamiento saturado de
un bit. La palabra reemplaza la fuente de lookup **antes** de esas operaciones;
no es un pitch final que se escriba directamente saltándose la afinación.

El índice en `0x018c2184` es explícitamente 1200 y el coeficiente en
`0x027fd9e0` se suministra como entrada. Se cubren siete patrones: cero, uno,
signed máximo/mínimo, -1, +0,5 y -0,5 en Q31. No se afirma haber recuperado
calibración, unidades de frecuencia o valores predeterminados.

Para contrastar la selección se varía la **tabla completa de 128 palabras** en
`0x01dfe530`: ceros frente a palabras sintéticas no cero. En **1.491 pares**
todos los efectos compartidos, argumento y guardas son idénticos, salvo la
vecina real que coincide con la última palabra de la tabla. Se captura y
compara la tabla completa tras cada llamada. Además, una guarda de PC sobre el
bloque de lookup detendría la ejecución si se alcanzase: no se activa en las
3.462 llamadas. No se parchea ninguna instrucción para forzar ese resultado.

## Actividad, Core y diferencias entre selectores

El gate byte `0x01dfe730` no nulo retorna antes del recorrido. Con gate cero y
lista vacía, los selectores **1 y 2 tienen los mismos efectos** y procesan voz 0;
el **3 procesa seis voces** en orden. No se asignan nombres comerciales a modos.

| Actividad de cada voz | Efectos observados |
| --- | --- |
| Cero, Core acepta | ADSR `noteOn` y ganancia baja; resetea las cinco fases y campo32 de Dr. Wave; avanza RNG y escribe pitch, velocidad y actividad 1. |
| Cero, Core rechaza | ADSR `noteOn` y ganancia baja permanecen; conserva objeto Dr. Wave y registros; no avanza RNG para esa voz. |
| No cero | Avanza RNG, escribe velocidad, solicita reinicio ADSR y recalcula ganancia baja; conserva pitch/actividad registrados y objeto Dr. Wave. |

Core acepta cuando cargado y seleccionado coinciden; su rechazo no revierte
ADSR/ganancia. La voz ya activa solicita reinicio aunque esos slots difieran.
Los estados iniciales incluyen las **64 máscaras** de seis actividades, con
bytes activos 1, 128 y 255. Son entradas explícitas, sin inferir su productor.

Por cada inicio aceptado o reinicio se aplica el RNG u32
`seed × 1664525 + 1013904223`, módulo 2³². El halfword alto signed16 se desplaza
aritméticamente ocho bits para voz inactiva aceptada y nueve para voz activa.
Se multiplica por el índice de voz y se desplaza 16 bits con wrap raw32; el
pitch final suma a la base su producto Q31 por esa desviación, con saturación.
La voz cero también avanza RNG. Los rechazos Core omiten ese avance, cambiando
la secuencia posterior de las demás voces en el selector 3.

La velocidad usa el halfword bajo signed16 y shift saturado de ocho bits.
Se conserva la palabra de velocidad recibida al copiar la lista. La ganancia
baja corresponde a `1 − nota/256` en las notas acotadas, con saturación 32767
para nota cero; la mitad alta se conserva.

En voz activa, `0x019120fc` pone etapa 6, contador cero, pitch preparado en
offset 12 y patrón uint16 de velocidad en el halfword alto de offset 16.
Conserva nivel; después se recalcula ganancia baja. En voz inactiva, `noteOn`
pone etapa 1, contador y nivel cero, conservando las palabras pendientes previas.

Al terminar se añade una entrada con marcador **`0xfffffffe`**, incluso si Core
rechaza todos los inicios. Conserva el pitch explícito raw completo, palabra de
nota y velocidad y byte de metadata. El padding 9–11 del destino y la cola de
la lista se preservan. El argumento y sus guardas permanecen intactos.

**Escribir un registro o campo pendiente no demuestra controlar el buffer del
render.** No se ejecuta renderer ADSR/Dr. Wave ni se añade un consumidor supuesto,
relevo L1, reloj, scheduler, transporte o integración del plugin.

## Cobertura y comparación

- **3.462 llamadas**: 3.078 independientes y seis streams de 64.
- **71 palabras no cero**: 32 walking bits, sus 32 complementos y patrones
  adicionales signed32 extremos y vecinos de saturación. Se cruzan con siete
  afinaciones, tres selectores y dos tablas: **2.982 casos de matriz**.
- **994 triples controlados de selector** y 1.491 pares de lookup. Los selectores
  1/2 coinciden también numéricamente; se compara estado completo, no solo pitch.
- **3.318 inserciones**, 144 gates, 2.317 inicios aceptados, 2.185 rechazos Core,
  4.351 reinicios y **6.668 pasos RNG**. Se observan 76 resultados saturados
  positivos y 97 negativos entre las voces efectivamente procesadas.
- Metadata 0/1/128/255; velocidades bajas 0/1/127/128/255/32767/32768/65535,
  con bits altos independientes. Las 128 notas aparecen en cada selector.

Se comparan los **4.568 bytes completos de regiones seleccionadas**: lista de
128 entradas y vecinos, contador/gate, seis ADSR y scratch, seis objetos
Dr. Wave completos, registros, ocho sonidos, RNG/vecinos, selección/cargado,
punteros, auxiliares, afinación/discriminador y ajustes. Se añaden el argumento
de 16 bytes con dos guardas de ocho bytes y los 512 bytes de tabla sintética.
La tabla termina en la palabra `0x01dfe72c`, primera de la imagen vigilada;
se respeta ese solapamiento real en carga y comparación. No se afirma comparar
toda la RAM, pila, CPU o mutaciones concurrentes del dispositivo.

Las secuencias retienen estado, registros, RNG y bytes de lista, pero el harness
**suministra contador cero y actividad en cada llamada**. La tabla, controles y
afinación también son entradas. No hay constructor, retirada ni composición de
ciclo de notas natural; el orden es propio del ensayo.

Dos reproducciones privadas nuevas coinciden byte a byte, sin regenerar los
fixtures finales. Pasan **53 suites de núcleo Linux**, comparador Windows con
ABI MSVC bajo **Wine** y **ASan/UBSan** (`detect_leaks=0` por ptrace). CI nativo
Windows/macOS queda pendiente. No se valida DAW, hardware, ASIO, latencia o dropouts.

GNU BF524 usa ambos parches de writeback y preflight halfword. Hay guardas de
PC para lista no vacía, selector cero, lookup, helper de error y código L1. Un
control positivo en `0x01a84822`, antes de las otras cinco opciones de guarda,
se detiene con código 5 y salida vacía antes de ejecutar el producto. REA
6.8.0 / GNU objdump inspecciona el llamador completo privadamente; no descifra
ni realiza la validación numérica.

```sh
python3 scripts/verify-drwave246-empty-explicit-pitch.py \
  --ldr /privado/OP1_vdk.ldr --assembler "$POCKET_BFIN_AS" \
  --linker "$POCKET_BFIN_LD" --simulator "$POCKET_BFIN_SIM" \
  --testutils "$POCKET_BFIN_TESTUTILS" --output /privado/empty-explicit-nuevo
cmake --build build-dsp --target drwave246_empty_explicit_pitch_tests
ctest --test-dir build-dsp -R '^drwave246_empty_explicit_pitch$' --output-on-failure
```

[Preparación DSP](new-account-dsp-setup.md) ·
[Formato propio](../Tests/ReferenceVectors/drwave246-empty-explicit-pitch-format.md) ·
[Evidencia y hashes](firmware/drwave246-empty-explicit-pitch-validation.json).

## Coordinación y límites

Base comprobada **`ac34c32`**, PR #28/#29 fusionadas. La
[PR #30](https://github.com/jguillenconesa-coder/Tiny-one/pull/30), rama
`collab/drwave-note-lifecycle` en `c9234dc`, conserva la composición reservada
con constructor y pitch cero. No se usan sus archivos ni se ejecuta su secuencia,
y no se modifican índices comunes. Se reutilizan solo helpers y generadores ya
fusionados en main como contexto de datos; ninguno se cambia.

Este bloque excluye pitch cero, lista no vacía, otros selectores, modo externo
no cero, nuevos índices de nota fuera de 0–127, ADSR no listos o auxiliares
presentes. Las guardas del adaptador rechazan esos casos antes de mutar y no
se presentan como retornos del original. Los valores raw probados no son presets,
unidades de pitch calibradas o estados render-ready.

Solo se añaden archivos propios y tres líneas de registro CMake. Plan global,
cobertura, README común de fixtures, plugin, MIDI/automatización/sesiones, UI,
presets y bancos permanecen intactos. Firmware, instrucciones originales,
desensamblados y ELF se mantienen privados. Código propio AGPL-3.0-only;
herramientas con las atribuciones de [CREDITS](../CREDITS.md).
