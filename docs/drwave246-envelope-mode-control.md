# Rev246: selector auxiliar ADSR y liberación de seis voces

Se amplía la [entrada ENV de cuatro fuentes](drwave246-envelope-control-input.md)
al color **4**, en el contexto acotado de **tipo interno de módulo 2 (Dr. Wave)**.
El callback `0x019cce68` y el helper `0x019f69b0` se ejecutan completos, sin
parches, seguidos del render ADSR original `0x01912128` sobre una voz elegida
explícitamente. `Source/ControlEnvelope246Mode.h` reconstruye esa ruta fuera del
plugin. Se conserva el comparador anterior de colores 0–3.

## Contrato

Se mantienen los destinos ENV 1/desactivado 2, `kind == 1`, fuentes low16 y
orden de los cuatro canales ya contrastados. Se admiten colores 0–4. Para el
color 4, el inicializador original `0x01a1e914` produce el descriptor
**`[2048,14336,32767]`** en `0x01de5728`: mínimo, máximo y escala Q15.
Se captura esa salida junto a los cuatro descriptores anteriores; el harness
suministra después copias explícitas con guardas distintas en cada registro.

El helper aplica la resta low16 con wrap, desplazamiento saturado y redondeo
Q15 previamente contrastados; suma al valor anterior y limita al intervalo.
El resultado se almacena en la quinta palabra de parámetros `0xff80172c`.
Antes de almacenarlo compara **`old >> 12` con `new >> 12`**, con signo:

- Si coinciden, no ejecuta la liberación rápida por ese canal.
- Si difieren, obtiene las seis envolventes mediante `0x0191264c` y llama a
  `0x0191209c` en cada una: etapa **8**, contador de bloques **0**, preservando
  nivel, palabras pendientes y ganancia.

Cada canal actúa sobre el resultado del anterior. Una llamada puede cambiar
el selector varias veces, incluso terminar en el mismo nibble inicial después
de haber liberado las voces. No se colapsan los canales en una única operación.
Los colores 0–3 pueden intercalarse y conservan su comportamiento anterior.
La función C++ también reinicia su bookkeeping de bloque; esos campos extra no
forman parte de los 24 bytes de estado original.

El helper consulta `0x01de4c40` para seleccionar el tipo de módulo entre
`0x01ab3144` y `0x01ab3148`. En este ensayo se alterna el selector y **ambas
entradas valen explícitamente 2**. El tipo 4 tiene dos callbacks adicionales
que aquí no se ejecutan ni se reconstruyen. El pool está inicializado de forma
explícita (`0x01b6053c = 1`); no se valida su construcción perezosa.
La API propia expresa este contexto mediante el nombre `DrWaveStep`; no debe
usarse como dispatcher genérico de tipos de módulo.

## Estado compartido y comparación

**1.024 llamadas / 131.072 muestras ADSR**: 512 casos independientes y ocho
secuencias de 64 llamadas que conservan pool de seis voces y RNG/dither.
Se compara el pool completo antes y después del render de la voz seleccionada,
así como salida, finalización, parámetros completos, fuentes, controles, objeto,
descriptores, selector/tipos, flag de inicialización y vecinos.

El render escribe un scratch compartido de **128 palabras en `0xff800164`**,
inmediatamente después de los seis estados de 24 bytes en `0xff8000d4`.
Esa primera palabra no es una guarda del pool: se captura como dither generado.
Se comparan las 128 palabras con la secuencia independiente `Adsr246Noise`, el
estado RNG en `0xff802bc8`, el valor anterior en `0x01b60534` y una guarda tras
el scratch. También se preserva una imagen ADSR separada, para detectar el uso
accidental de un estado que no pertenezca al pool real.

Se cubren las 16 máscaras, los cinco colores, los nueve estados ADSR y canales
repetidos en serie. Hay 872 callbacks aplicados y 152 inactivos; en los aplicados,
543 terminan con un nibble de modo distinto del inicial y 329 con el mismo.
La comparación del pool incluye los efectos intermedios que no refleja esa
clasificación final. Todos los estados iniciales, parámetros, fuentes, volumen,
semillas, selección de voz y orden siguen siendo entradas explícitas.

Dos reproducciones privadas finales coinciden byte a byte. Pasan **32 suites de
núcleo y 34 con JUCE en Linux**, además de ASan/UBSan del nuevo comparador
(LeakSanitizer desactivado por ptrace). Windows/macOS quedan pendientes de CI;
Wine no se ejecuta en este entorno. REA 6.8.0 aporta inspección estática mediante
`inspect-with-objdump`, sin truncación ni cambios del ELF privado. La ejecución
numérica corresponde al GNU BF524 con el parche D-register documentado.

```sh
python3 scripts/verify-control-envelope246-mode.py \
  --ldr /privado/OP1_vdk.ldr --assembler /herramientas/bfin-elf-as \
  --linker /herramientas/bfin-elf-ld --simulator /herramientas/run-parallel \
  --testutils /fuentes-gdb/sim/testsuite/bfin --output /privado/env-modo-nuevo
cmake --build build-dsp --target control_envelope246_mode_tests
ctest --test-dir build-dsp -R '^control_envelope246_mode$' --output-on-failure
```

Salida nueva y externa al checkout; `--record` solo regenera los dos fixtures
numéricos nuevos. [Evidencia y hashes](firmware/control-envelope246-mode-validation.json).
Firmware, instrucciones, desensamblados y ELF permanecen privados.

Esto no completa los cuatro campos auxiliares del preset, el relevo de campos
pendientes por voz, el llenado inicial del pitch bloqueado en L1, el scheduler,
mezcla/salida ni integración de Dr. Wave. El render se invoca explícitamente;
no se infiere que la quinta palabra controle por sí sola sus etapas. Tampoco
se valida hardware, Ableton, latencia ni ausencia de dropouts. El próximo límite
legible es el destino de volumen del mismo callback y su buffer ya preparado,
si se puede acotar su procedencia sin sustituir el relevo cifrado.
