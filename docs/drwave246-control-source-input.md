# Rev246: entrada de control hacia las cuatro fuentes del modificador

La ruta válida del decodificador VoiceCtl **`0x01a84f8c..0x01a85000`** escribe
valores en la tabla de 128 palabras de `0x01dff138`. Sus entradas **1–4** ocupan
`0x01dff13c..0x01dff14b`: las mismas cuatro fuentes del
[modificador de volumen/pitch](drwave246-volume-buffer-modifier.md).
`Source/Voice246ControlInput.h` reconstruye esa escritura y se contrasta también
junto al callback original y el render completo de Dr. Wave. Permanece fuera
del plugin; no cambia el procesamiento MIDI del instrumento.

## Contrato acotado

El decodificador recibe un objeto con punteros de inicio/fin de mensaje. Con al
menos tres bytes legibles y **byte 1 < 128**, hace:

```text
table[unsigned(message[1])] = unsigned(message[2])
```

Es una escritura completa de 32 bits: limpia los bits altos de la entrada
anterior. No escala, no acumula y **no enmascara a siete bits el valor**. El
byte de estado no se consulta aquí; clasificación MIDI, selección de canal,
filtrado y llamada al decoder pertenecen a un dispatcher aún no reconstruido.
Los valores 128–255 se usan como estrés de la lectura unsigned, no se presentan
como datos MIDI válidos.

El original tiene rutas de error para mensajes cortos/índices fuera de rango,
con llamadas fuera del recorrido acotado. No se ejecutan ni se emulan. Los
rechazos de puntero nulo, longitud < 3 o índice >= 128 y el retorno bool son
protecciones propias del adaptador; no reproducen la excepción/retorno original.

## Dos oráculos

El ensayo independiente ejecuta la rutina original desde su entrada en **32.768
parejas**: 128 índices × 256 valores, de las cuales 16.384 son parejas de datos
MIDI de siete bits. Añade **256 llamadas continuas** que conservan toda la tabla.
Compara las **128 palabras en cada llamada** (4.227.072 observaciones), cuatro
palabras de guarda y el objeto/mensaje, incluidos los punteros normalizados.
Las imágenes iniciales son explícitas y distintas, generadas por el llamador;
no se afirma haber recuperado el inicializador original de la tabla.

El ensayo encadenado ejecuta **2.048 decodificaciones originales**: cuatro
mensajes por cada uno de los 512 registros del ensayo de volumen/pitch.
Sus índices son 1, 2, 3 y 4; las palabras fuente se precargan con patrones
`deadbeef/a5a5a5a5/80000000/ffffffff` para detectar escrituras incompletas.
Después se llama al modificador original sin sustituir sus fuentes por copias
calculadas en el harness y se ejecuta el render original Dr. Wave.

Se comparan **65.536 muestras signed16 de volumen, 65.536 palabras de pitch y
61.568 muestras posteriores de Dr. Wave**, además de los cuatro valores fuente,
sus vecinos, mensajes, suavizado compartido y los estados/guardas/bancos del
comparador anterior. Se conservan las 81 combinaciones de canales; hay 459
callbacks aplicados y 53 inactivos. Los buffers entrantes y la programación de
llamadas siguen siendo explícitos. No se mezcla la salida de volumen con el
audio Dr. Wave para inventar una salida original.

Cada oráculo se reproduce en dos directorios privados y coincide byte a byte.
Pasan **35 suites de núcleo y 37 con JUCE en Linux**, y ASan/UBSan de ambos
comparadores (LeakSanitizer desactivado por ptrace). Windows/macOS quedan para
CI; Wine no se ejecuta aquí. REA 6.8.0 aporta inspección estática delimitada con
`inspect-with-objdump`; el oráculo numérico usa GNU BF524. El ensayo encadenado
exige los dos parches documentados y comprueba el probe de cargas halfword.
No se valida hardware, Ableton, temporización, latencia ni dropouts.

```sh
python3 scripts/verify-voice246-control-input.py \
  --ldr /privado/OP1_vdk.ldr --assembler "$POCKET_BFIN_AS" \
  --linker "$POCKET_BFIN_LD" --simulator "$POCKET_BFIN_SIM" \
  --testutils "$POCKET_BFIN_TESTUTILS" --output /privado/control-nuevo
python3 scripts/verify-voice246-control-chain.py \
  --ldr /privado/OP1_vdk.ldr --assembler "$POCKET_BFIN_AS" \
  --linker "$POCKET_BFIN_LD" --simulator "$POCKET_BFIN_SIM" \
  --testutils "$POCKET_BFIN_TESTUTILS" --output /privado/control-cadena-nuevo
```

Salidas nuevas y externas al checkout. `--record` regenera únicamente los
fixtures correspondientes a cada ensayo. El independiente sitúa los datos del
harness fuera del SDRAM original y usa 128 MiB en el simulador por el tamaño de
las capturas completas; no implica esa memoria en el aparato. Firmware,
instrucciones, desensamblados y ELF permanecen privados.
[Evidencia del decoder](firmware/voice246-control-input-validation.json) ·
[Evidencia encadenada](firmware/voice246-control-chain-validation.json).

Se identifica un escritor real de esas cuatro entradas, sin demostrar que sea
su único escritor ni recuperar el dispatcher/scheduler. El llenado inicial del
pitch bloqueado en L1, el productor del buffer de volumen, el relevo ADSR por
voz, asignador y mezcla/salida siguen pendientes. El próximo contrato legible
puede acotarse en el destino instrumento para el tipo interno 2 de Dr. Wave,
manteniendo estos límites y un único componente activo.
