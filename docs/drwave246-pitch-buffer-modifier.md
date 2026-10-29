# Rev246: modificación del buffer de pitch antes de Dr. Wave

La rutina SDRAM legible **`0x019cce68..0x019cd01c`** (fin exclusivo) modifica
las 128 palabras del buffer `0xff801950` según cuatro entradas de control.
Su ruta acotada de pitch está reconstruida en `Source/ControlPitch246.h` y
contrastada seguida del render original completo de Dr. Wave. Sigue aislada
del plugin; no cambia instaladores, IDs MIDI, sesiones, presets ni interfaz.

**Consume un buffer ya lleno.** No resuelve el productor que conecta los seis
registros VoiceCtl `0xff8012d4` con ese buffer. Las referencias candidatas de
ese relevo siguen en L1 parcialmente cifrada; no se ejecutan ni se completan
por interpolación supuesta. No se dispone de hardware ni de una imagen L1
descifrada. Este avance verifica una transformación posterior, sin atribuirle
el llenado inicial ni una cadencia original.

## Contrato acotado

El llamador suministra ocho controles signed16 y una palabra `kind`. Los cuatro
primeros controles seleccionan el objetivo mediante desplazamiento aritmético
de doce bits; los cuatro últimos seleccionan el destino desplazando once bits.
Los valores inferiores a esos campos se varían también en los ensayos.

| Destino | Objetivo | Resultado cubierto |
| --- | --- | --- |
| 2 | Cualquiera | Canal desactivado; no consulta su fuente para modificar pitch. |
| 3 | 1 | Con `kind == 0`, transforma en orden las 128 palabras del buffer. Con otro `kind`, conserva el buffer. |

Las cuatro fuentes son palabras de 32 bits en **`0x01dff13c`**; solo se consume
su mitad baja con signo. No se reconstruye todavía quién actualiza esas fuentes.
El adaptador valida todos los canales antes de mutar y rechaza los demás
destinos/objetivos. Esa validación es propia: no emula asserts ni las rutas
originales de instrumento, envolvente o volumen. No se atribuye una identidad
comercial al módulo ni se presenta un LFO completo.

Cada canal activo calcula:

```text
scaled = saturate16(signed16(source.low16) * 256)
square = multiplyQ15_nearest_even(scaled, scaled)
cube = multiplyQ15_nearest_even(square, scaled)
coefficient = signed32(uint32(uint16(cube)) << 16)
for each word in the existing pitch buffer:
    word = saturate32(multiplyQ31_floor(signed32(word), coefficient) * 8)
```

Los canales se aplican **en serie** sobre el resultado del anterior, conservando
redondeos y saturación entre canales. Una fuente cero puede borrar el buffer;
no representa bypass. El retorno original vale uno cuando se aplica algún canal
de pitch, aunque su coeficiente sea cero, y cero en las rutas inactivas cubiertas.

## Evidencia y reproducción

El llamador propio ejecuta el callback original completo sin parches y después
el render de Dr. Wave, seleccionando únicamente esas ramas. Reutiliza los 512
registros del ensayo de render: 256 casos independientes y cuatro secuencias
de 64 llamadas que conservan el estado de síntesis. El pitch de entrada, fuentes,
controles, eventos y contador se suministran explícitamente en cada llamada.

Se comparan **65.536 palabras de pitch** y **61.568 muestras activas** de audio:
436 callbacks aplicados, 76 inactivos; 240 actualizaciones de onda, 241 lecturas
y 31 renders inactivos. Se cubren las 16 máscaras de canales, de cero a cuatro
activos, fuentes signed16 en límites y valores aleatorios, palabras altas
independientes, resultados negativos y saturación en ambos extremos.

Además del audio se compara el pool de seis voces, ondas, cachés, configuración,
incrementos, puntero, flags y contador. Las guardas del buffer, fuentes y vecinos,
el estado de volumen `0x01dd5b3c`, controles y objeto del callback se conservan.
Los bancos generados coinciden antes y después. El render de Dr. Wave consume
la primera palabra de pitch en el contrato ya contrastado; las 128 palabras
modificadas se comparan por separado con la salida original.

Dos ejecuciones privadas nuevas coinciden byte a byte. Pasan **30 suites de
núcleo en Linux**, el nuevo comparador compilado para **Windows bajo Wine** y
**ASan/UBSan** (LeakSanitizer desactivado por la limitación de ptrace del entorno).
No se validan hardware, DAW/ASIO, latencia, tasas ni temporización.

REA **6.8.0**, con `inspect-with-objdump` y el proveedor `gnu.objdump`, conserva
el desensamblado del callback delimitado por un símbolo sintético en un ELF
privado. El comando `function` no admite este ELF Blackfin: no se atribuye a REA
decompilación ni descifrado L1. La ejecución numérica usa el GNU BF524 con el
parche documentado de escritura diferida D-register. Las instrucciones,
desensamblados, imagen SDRAM y ELF originales permanecen privados.

```sh
python3 scripts/verify-control-pitch246.py \
  --ldr /privado/OP1_vdk.ldr --assembler /herramientas/bfin-elf-as \
  --linker /herramientas/bfin-elf-ld --simulator /herramientas/run-parallel \
  --testutils /fuentes-gdb/sim/testsuite/bfin --output /privado/control-pitch-nuevo
cmake --build build-dsp --target control_pitch246_tests
ctest --test-dir build-dsp -R '^control_pitch246$' --output-on-failure
```

La salida debe ser nueva y externa al checkout. `--record` solo regenera los dos
fixtures nuevos. [Evidencia y hashes](firmware/control-pitch246-validation.json).
El siguiente paso del mismo bloque es contrastar otra dependencia legible del
buffer o del relevo ADSR. El productor inicial, asignador, scheduler y salida
completa siguen pendientes antes de integrar Dr. Wave en el instrumento.
