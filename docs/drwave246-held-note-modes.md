# Dr. Wave rev246: selectores 1–3 con lista inicial no vacía

Se contrasta la **entrada completa `0x01a84604`** con lista no vacía y selectores
1–3. El intervalo legible termina en **`0x01a84ba0`** exclusivo: 1.436 bytes,
prólogo con marco de 76 bytes al inicio y siguiente prólogo independiente al final.
La rama por contador no nulo salta de `0x01a8487c` a **`0x01a84a70`**. Los
bloques de retorno y error finales pertenecen al llamador; no se extrae o parchea
la rama para ejecutarla aisladamente.

El contexto de **lista inicialmente vacía**, su selección/activación y sus
retornos están reservados a la coordinadora en `collab/drwave-note-modes`
([PR #28](https://github.com/jguillenconesa-coder/Tiny-one/pull/28)). No se usan
sus archivos ni se desarrolla ese contexto. La base revisada es `a71c033`, con
las PR #25, #26 y #27 fusionadas. Plan, cobertura e índice común quedan intactos.

## Entrada, dependencias y retornos tempranos

El argumento raw tiene 16 bytes: palabras de nota y velocidad, byte de metadata
en offset 8, padding 9–11 y palabra de pitch explícito en offset 12. Se suministra
directamente con guardas: este ensayo no incluye constructor, setter o transporte.
El llamador no escribe en el argumento.

Primero consulta **un byte** en `0x01dfe730`. Si no es cero retorna, antes de
consultar controles, actividad o tabla. Si es cero, busca la palabra de nota
completa entre las entradas contadas de `0x01dfe734`, con paso de 20 bytes. Una
coincidencia también retorna, sin filtrar por metadata o identificador de voz.
Los bits altos importan: igualdad de byte/halfword no es igualdad de nota.

En nuevas entradas, el modo externo `0xff801038` es explícitamente cero y la
palabra de selector se lee signed16 en offset 8 de los 16 bytes del sonido
seleccionado por `0xff8016d4`; se desplaza aritméticamente 12 bits. Se cubren
los selectores 1–3, incluidos residuos 0, 1, 2.048 y 4.095. No se atribuyen
nombres comerciales a esas ramas.

Hay seis ADSR previamente inicializados (`ready=1`), Core Dr. Wave y punteros
auxiliares nulos. Los objetos Dr. Wave conservan vtables válidas y otros campos
de estrés; no se ejecuta su render. La actividad, selección y sonido cargado son
entradas: este recorrido con lista no vacía no busca una voz libre ni usa el
gate de aceptación Core de la primera activación. Los casos aceptados con todas
las actividades cero o cargado distinto de seleccionado siguen este mismo contrato.

## Pitch, RNG y diferencias entre selectores

Si el pitch explícito es no nulo, usa esa palabra signed32; si es cero lee
`tabla[nota]` de `0x01dfe530`. Se mantienen notas nuevas **0–127** para acotar
tanto lookup como cálculo de ganancia. La tabla procede del fragmento de copia
original `0x01a86700..0x01a8672e` más un retorno propio: no se ejecuta ni se afirma
haber recuperado su inicializador completo. Se captura la tabla antes y después.

El índice de afinación en `0x018c2184` se fija explícitamente a 1200; el
coeficiente de `0x027fd9e0` se suministra cada llamada, incluidos cero, signos y
extremos. No se infiere calibración de fábrica. La base es el producto Q31
truncado, seguido de desplazamiento saturado de un bit.

Por cada voz procesada `v`, empezando en cero, se actualiza el RNG u32 en
`0xff802bc8`: `seed = seed × 1664525 + 1013904223`, módulo 2³². La mitad alta
**signed16** se desplaza aritméticamente nueve bits; el resultado se multiplica
por `v` y se desplaza 16 bits, con wrap raw32. El pitch preparado suma a la base
el producto Q31 por esa desviación, con suma saturada. La voz cero avanza RNG
aunque su desviación sea cero. El vecino y el valor previo de ruido no cambian.

| Efecto antes de cualquier render | Selector 1 | Selector 2 | Selector 3 |
| --- | --- | --- | --- |
| Voces procesadas | 0 | 0 | 0–5 |
| Pasos de RNG | 1 | 1 | 6 |
| Registro de pitch VoiceCtl | Conservado | Escribe voz 0 | Conservado |
| Registro de velocidad VoiceCtl | Escribe voz 0 | Escribe voz 0 | Escribe seis |
| Bytes de actividad | Conservados | Conservados | Conservados |
| ADSR | Solicita reinicio voz 0 | Conserva todos | Solicita seis reinicios |
| Ganancia ADSR | Recalcula mitad baja de voz 0 | Conserva todas | Recalcula seis mitades bajas |

La velocidad usa la mitad baja **signed16** de la palabra recibida y shift
saturado de ocho bits; la palabra completa se conserva al añadir la nota.
La solicitud `0x019120fc` pone etapa 6 y contador cero, escribe el pitch pendiente
en offset 12 y la velocidad preparada como uint16 desplazada 16 bits en offset
16. Conserva nivel y después se actualiza solo la mitad baja de ganancia:
`1 - nota/256` convertido a fract16, saturado a 32767 para nota cero. La mitad
alta anterior permanece. El despacho auxiliar retorna sin callbacks en el
contexto nulo. No se llama al note-on Core de Dr. Wave en esta rama.

**No se consume aquí el pitch pendiente.** Tampoco se resetea el nivel, se
confirma el nuevo pitch en los registros de los selectores 1/3 ni se altera
su actividad para simular una integración. El consumidor L1 pendiente sigue
sin recuperarse. No se necesita render ADSR posterior para demostrar estas escrituras.

## Lista, memoria y límites excluidos

Los tres selectores añaden al final nota, velocidad, byte de metadata, palabra
de pitch explícito e identificador raw **`0xfffffffe`**, incrementando el contador.
No se interpreta ese marcador como voz física. Los bytes de padding 9–11 del
destino y todos los registros anteriores de la lista se conservan; no hay
desvinculación o eliminación de un dueño previo en este contexto.

Se comparan **4.568 bytes completos de regiones seleccionadas**: las 128 entradas,
contador/vecinos/gate, seis ADSR/scratch y guardas, seis objetos Dr. Wave, registros
VoiceCtl, ocho sonidos, RNG/vecinos, selección/cargado, punteros Core y auxiliares,
afinación/discriminador y tabla de ajustes con vecinos. Se compara además el
argumento completo, padding y guardas de ocho bytes a ambos lados. Las regiones
se documentan con direcciones/offsets en el formato propio; no se afirma vigilar
toda la RAM, pila, registros de CPU o mutaciones concurrentes del aparato.

Los retornos por gate/duplicado incluyen lista de 128 entradas y palabras de
nota fuera de 0–127, sin alcanzar el lookup. Una nueva nota con lista llena
puede mutar estado antes de llegar al assert de capacidad: **queda excluida del
oráculo**, igual que modo externo distinto de cero, otros selectores, nuevo
índice fuera de rango, auxiliares presentes o inicialización ADSR perezosa.
Las protecciones C++ rechazan antes de mutar y son propias, no retornos originales.
Los estados de resultado del adaptador tampoco son un contrato de R0 original.

## Casos y reproducción

- **1.536 llamadas**: 1.152 independientes y seis secuencias de 64, siempre con
  lista no vacía al entrar. Se cubren todas las longitudes 1–128.
- **256 triples controlados**: solo cambia la palabra de selector. Incluyen las
  128 notas tanto con lookup como con pitch explícito, para cada selector.
- **1.056 inserciones**: 355/349/352 para selectores 1/2/3; **2.816 pasos de RNG**.
  Hay 360 inserciones con todas las actividades cero, 348 con todas no nulas y
  336 con cargado distinto de seleccionado.
- **240 gates y 240 duplicados**; metadata 0, 1, 128 y 255, padding raw,
  velocidades low16 0/1/127/128/255/32767/32768/65535 y bits altos arbitrarios.
  Los duplicados incluyen notas añadidas antes en el mismo stream.

Se retienen lista, ADSR, registros, objetos, RNG y otras regiones entre llamadas
de secuencia. Gate/actividad y controles no retenidos se suministran explícitamente
por llamada. El orden es del harness y no demuestra el dispatcher o scheduler.

Dos reproducciones privadas nuevas coinciden byte a byte. Pasan **51 suites
de núcleo Linux**, el comparador Windows compilado con ABI MSVC bajo **Wine**,
y **ASan/UBSan** (`detect_leaks=0` por el límite ptrace). No se valida Windows
nativo/macOS, hardware, DAW, ASIO, latencia o dropouts; CI nativo queda pendiente.

GNU BF524 usa ambos parches de writeback y preflight halfword. Las guardas de PC
detienen la entrada a la rama vacía, la rama de selector cero, helper de error
y código L1. Un control positivo en `0x01a84a70`, colocado antes de las otras
cuatro guardas, detiene el primer evento con salida 5. REA 6.8.0 / GNU objdump
inspecciona el llamador completo privadamente; no descifra ni valida números.

```sh
python3 scripts/verify-drwave246-held-note-modes.py \
  --ldr /privado/OP1_vdk.ldr --assembler "$POCKET_BFIN_AS" \
  --linker "$POCKET_BFIN_LD" --simulator "$POCKET_BFIN_SIM" \
  --testutils "$POCKET_BFIN_TESTUTILS" --output /privado/held-modes-nuevo
cmake --build build-dsp --target drwave246_held_note_modes_tests
ctest --test-dir build-dsp -R '^drwave246_held_note_modes$' --output-on-failure
```

Preparación: [guía DSP entre cuentas](new-account-dsp-setup.md).
Formato: [documento propio](../Tests/ReferenceVectors/drwave246-held-note-modes-format.md).
Hashes y límites: [evidencia](firmware/drwave246-held-note-modes-validation.json).
Solo `--record` escribe los dos fixtures nuevos.

`Source/DrWave246HeldNoteModes.h` reutiliza aritmética, tabla y solicitud ADSR
existentes sin modificarlas. Solo se registra una suite en CMake. No se integran
render, transporte, UI o plugin ni se cambian MIDI, automatización o sesiones.
Presets y bancos siguen aplazados. Firmware, instrucciones originales,
desensamblados y ELF permanecen privados. Código propio AGPL-3.0-only;
herramientas con las atribuciones de [CREDITS](../CREDITS.md).
