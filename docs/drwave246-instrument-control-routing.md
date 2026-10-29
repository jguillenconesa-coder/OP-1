# Rev246: destino instrumento para los controles de Dr. Wave

Se reconstruye el destino **instrumento** del callback `0x019cce68`, en el
contexto explícito del **tipo interno 2 (Dr. Wave)**. El recorrido incluye el
helper SDRAM `0x01a1dfec` y sus rangos generados por `0x01a1e914`; termina en
el render Dr. Wave original `0x0197c870`, que consume directamente la imagen de
parámetros modificada en `0xff801724`. La implementación propia está en
`Source/DrWave246ControlInstrument.h` y permanece fuera del plugin.

## Contrato acotado y datos originales

Con `kind == 0`, destino `controls[4+i] >> 11 == 0` y selector signed16
no negativo, el callback prepara la fuente low16 restando 64 con wrap y
multiplicando por 1024 con saturación signed16. Destino **2** desactiva el canal.
Los cuatro canales se aplican en orden; se excluyen los destinos ENV,
volumen/pitch y otros tipos de módulo. El adaptador valida el conjunto antes de
mutar, incluidos los primeros cuatro parámetros dentro de sus rangos originales.
Son guardas propias, no emulación de asserts.

El callback lee el sonido seleccionado en `0xff8016d4` y su tipo en la tabla
`0xff8016e4`. En el ensayo se recorren los ocho slots y se suministra **2** en
el elegido. La bandera del tipo se lee del SDRAM original en `0x018e1712` y
vale **0**: selecciona desplazamiento de doce bits para el selector. No se
sobrescribe esa bandera ni se generaliza a tipos que usan otro desplazamiento.
Se compara preservada junto a los otros quince bytes de la tabla de banderas.

El inicializador original genera ocho descriptores de cuatro palabras para el
tipo 2 en `0x01de5910`. El ensayo captura y compara los ocho antes y después de
las llamadas. Los cuatro primeros son:

| Control | Mínimo | Máximo | Escala Q15 | Modo |
| --- | --- | --- | --- | --- |
| Azul | 0 | 24575 | 4915 | Wrap |
| Verde | 0 | 16383 | 3276 | Wrap |
| Blanco | 0 | 16383 | 3276 | Wrap |
| Naranja | 0 | 32767 | 6553 | Clamp |

Los selectores **0–3** actúan sobre esas cuatro palabras. Los **4–7** actúan
otra vez sobre las mismas palabras, desplazando aritméticamente la modulación
**cuatro bits antes de multiplicar**; no escriben cuatro parámetros adicionales.
El helper consulta primero el modo del descriptor del selector original y
luego usa el rango/escala del selector reducido por cuatro. Por eso las escalas
cero de los descriptores 4–7 no hacen que esas rutas sean inactivas.

Cada canal calcula `combined = saturate16(parameter + multiplyQ15(modulation,
scale))`, con redondeo nearest-even. Para azul/verde/blanco, si cruza el límite
inferior suma `max-min`, y si cruza el superior resta ese mismo valor, conservando
la saturación de cada operación. Es **max-min**, sin añadir uno; no se sustituye
por módulo de `max-min+1`. Naranja aplica clamp. La suma/ajuste se realiza tras
cada canal, incluso cuando varios actúan sobre el mismo control. Las otras
cuatro palabras de la imagen de parámetros se preservan.

El retorno original vale uno cuando se procesa algún canal instrumento,
incluso con fuente neutra, y cero en los casos inactivos cubiertos. El ensayo
usa `kind` 0 y 2; la API no es un dispatcher genérico de motores.

## Ensayo encadenado

Se ejecutan **2.048 decodificaciones originales** hacia las fuentes 1–4 de la
tabla de controles, **512 callbacks instrumento** y **61.568 muestras activas
de Dr. Wave**. Son 256 casos independientes y cuatro secuencias de 64 llamadas
que conservan pool de voces, ondas y cachés. La imagen inicial de parámetros se
suministra de nuevo en cada registro; no se presenta como el productor original.
El render recibe el puntero a la imagen modificada, sin copiarle resultados
calculados por el harness.

Se cubren los ocho selectores, las 16 máscaras de canales, bits inferiores de
selección, canales repetidos, rutas reducidas por cuatro bits, wrap inferior y
superior de los tres primeros controles y ambos clamps de naranja. Hay 436
callbacks aplicados y 76 inactivos; el render hace 240 actualizaciones, 241
lecturas y 31 retornos inactivos.

Se comparan audio, imagen completa de parámetros/guardas, rangos, selección y
tipos/vecinos, banderas, fuentes, mensajes, pool completo de seis voces, ondas,
cachés, incrementos, configuración, puntero, flags, contador y bancos. Los
buffers de pitch/volumen y los cuatro suavizados de volumen se comparan
preservados: este destino no los transforma. Sus entradas y el contador siguen
siendo explícitos. No se recupera su productor ni la cadencia por esta prueba.

Dos reproducciones privadas finales coinciden byte a byte. Pasan **36 suites de
núcleo y 38 con JUCE en Linux**, y ASan/UBSan del comparador (LeakSanitizer
desactivado por ptrace). Windows/macOS quedan para CI; Wine no se ejecuta aquí.
REA 6.8.0 aporta inspección estática del helper mediante `inspect-with-objdump`;
GNU BF524 con ambos parches documentados ejecuta las instrucciones originales
sin modificarlas. No se valida hardware, DAW, latencia o ausencia de dropouts.

```sh
python3 scripts/verify-drwave246-control-instrument.py \
  --ldr /privado/OP1_vdk.ldr --assembler "$POCKET_BFIN_AS" \
  --linker "$POCKET_BFIN_LD" --simulator "$POCKET_BFIN_SIM" \
  --testutils "$POCKET_BFIN_TESTUTILS" --output /privado/instrumento-nuevo
cmake --build build-dsp --target drwave246_control_instrument_tests
ctest --test-dir build-dsp -R '^drwave246_control_instrument$' --output-on-failure
```

Salida nueva y externa al checkout; `--record` solo regenera los dos fixtures
numéricos nuevos. [Evidencia y hashes](firmware/drwave246-control-instrument-validation.json).
Firmware, instrucciones, desensamblados y ELF permanecen privados.

El siguiente contrato puede contrastar destinos instrumento/volumen/pitch
intercalados en una misma llamada de tipo 2. Eso tampoco resolvería el dispatcher,
productores iniciales de buffers, relevo ADSR por voz, asignador, reloj o
mezcla/salida pendientes. No hay integración del instrumento ni cambios de
IDs MIDI/automatización, sesiones, presets, interfaz o instaladores.
