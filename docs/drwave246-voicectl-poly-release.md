# Dr. Wave rev246: retirada de nota y release ADSR desde VoiceCtl

Se contrasta la rama polifónica de **VoiceCtl `0x01a85058..0x01a8541c`**,
seguida de un render ADSR común elegido explícitamente por el harness. Se ejecuta
la rutina original completa desde su entrada, con sus condiciones de contexto
satisfechas y el selector por sonido en cero. No se ejecuta L1 cifrada.

El resultado es un contrato aislado de retirada de lista y liberación de la
envolvente. **No conecta aún Dr. Wave al instrumento**, no recupera el productor
del buffer de pitch, el consumidor de campos ADSR pendientes ni el scheduler.
El render de audio de Dr. Wave no se invoca en este nuevo ensayo; sus objetos
sí participan en el callback original de release y se comparan completos.

## Contexto y contrato

| Entrada | Condición del ensayo |
| --- | --- |
| Flag en `0x01dfe730` | Cero; la rama de error queda fuera. |
| Modo VoiceCtl en `0xff801038` | Cero. |
| Sonido seleccionado en `0xff8016d4` | 0–7, explícito y variable. |
| Selector de voz por sonido | Word signed16 en `0x01b60ef0 + sonido × 16 + 8`, desplazado aritméticamente 12 bits: cero. No es el word anterior de modo VoiceCtl. |
| Lista en `0x01dfe734` | 0–128 entradas de 20 bytes; contador en `0x01dff134`. |
| Voz de una entrada | Cualquier valor negativo indica ausencia de voz; las voces no negativas del ensayo son 0–5. |
| Core y ADSR | Seis objetos Dr. Wave con vtable original, seis punteros Core explícitos y pool ADSR ya inicializado (`0x01b6053c = 1`). |
| Sustain de release | Word sin signo en offset 4 del sonido seleccionado; se ensaya 0–32767, con niveles y ganancia no negativos. |

La rutina busca la **primera** entrada cuya palabra de nota coincida con la del
argumento. No exige rango MIDI ni procesa el estado o canal de un mensaje MIDI.
Sin coincidencia deja la lista y los estados intactos.

Si la entrada tiene una voz no negativa, llama primero al release Core
`0x019678f0`, obtiene su ADSR mediante `0x0191264c` y llama a `0x019120d0` con el
sonido **seleccionado**. El callback de Dr. Wave `0x0197a838` conserva sus 36 bytes.
La diferencia entre sonido cargado y seleccionado no bloquea este release.
El ADSR recibe etapa 4 si su nivel es mayor que `sustain × 65536`, etapa 5 en
caso contrario y contador `0x10000000`; nivel, palabras pendientes y ganancia
se conservan. Con voz negativa no se llama a Core ni al ADSR.

Después retira esa primera entrada, desplaza las posteriores y resta uno al
contador. Copia las palabras de offsets 0, 4, 12 y 16 y el byte 8; **conserva los
tres bytes de relleno 9–11 de cada destino**. No limpia el antiguo último slot.
No borra actividad, velocidad ni registros de pitch en este recorrido.

`Source/Voice246PolyRelease.h` implementa únicamente ese contrato, sin acceso al
Engine ni a parámetros del host. Su enum de resultado y sus rechazos de contador,
voz o sustain fuera del contrato son protecciones propias; no emulan los asserts
ni atribuyen un retorno definido a la rutina original. El llamador debe aportar
el contexto polifónico indicado; otros modos no se convierten a esta rama.

## Ensayo y comparación

- **512 casos independientes y cuatro secuencias de 64 llamadas**: 768 registros.
- Todos los tamaños de lista 0–128; búsquedas al principio, interior y final,
  notas duplicadas, ausentes, entradas sin voz y las seis voces asignadas.
- **504 retiradas**, **264 búsquedas ausentes**, **297 releases ADSR**; las otras
  207 retiradas corresponden a entradas sin voz. Se incluyen discrepancias entre
  sonido cargado y seleccionado y estados iniciales de las nueve etapas ADSR.
- **98.304 posiciones** tras el render ADSR: **84.352 escritas** y **13.952
  conservadas**. El buffer de salida comienza con valores explícitos y guardas.
- Se compara una imagen de **4.440 bytes antes y después del render**: lista
  completa, contador, vecinos y flag; seis estados ADSR de 24 bytes; scratch de
  128 palabras y guardas; seis objetos Dr. Wave y dos guardas posteriores;
  776 bytes de registros VoiceCtl y vecinos; ocho imágenes de parámetros;
  RNG, valor dither anterior, ready flag y vecinos; sonido seleccionado/cargado
  y seis punteros Core con vecinos. También se captura el argumento y las
  entradas del llamador de 32 bytes, salida y retorno de finalización ADSR.
- En el pool ADSR, `0xff800164` inicia el **scratch compartido**, no una guarda
  de la sexta voz. Las guardas están en `0xff8000d0` y `0xff800364`.
- Las secuencias retienen lista, objetos, pool/scratch y RNG/valor anterior.
  Controles, registros restantes, sonidos, argumento, voz a renderizar y volumen
  son entradas explícitas en cada llamada. No se deduce un orden original de
  eventos ni una relación entre el ADSR renderizado y el mezclador de Dr. Wave.

Los fixtures numéricos ocupan registros de 4.744 bytes de entrada y 9.188 bytes
de salida. Las instrucciones, desensamblados y ELF permanecen privados. El
modelo es GNU BF524 con ambos parches documentados de writeback; el verificador
comprueba primero los paquetes halfword propios. La coincidencia no demuestra
el pipeline ni temporización del hardware.

REA 6.8.0 inspeccionó privadamente la rutina mediante `inspect-with-objdump` y
GNU objdump Blackfin, con símbolos sintéticos que delimitan el rango. La salida
fue completa y el artefacto no cambió. REA no descifró L1 ni realizó la comparación
numérica: esa comparación usa el simulador y la implementación C++ independiente.

## Reproducción y límites

```bash
python3 scripts/verify-voice246-poly-release.py \
  --ldr /privado/OP1_vdk.ldr --assembler /herramientas/bfin-as \
  --linker /herramientas/bfin-ld --simulator /herramientas/run-halfword-v2 \
  --testutils /fuentes-gdb/sim/testsuite/bfin --output /privado/release-nuevo
cmake --build build-dsp --target voice246_poly_release_tests
ctest --test-dir build-dsp -R '^voice246_poly_release$' --output-on-failure
```

Dos reproducciones privadas coinciden byte a byte. Pasan **41 suites de núcleo**
y **43 con JUCE** en Linux, más ASan/UBSan del nuevo comparador (LeakSanitizer
desactivado por el límite de ptrace). Windows y macOS se verifican en CI; no se
ha ensayado esta rama en DAW, hardware ni un instrumento completo.

El siguiente contrato legible puede ampliar la retirada a los otros selectores
por sonido, manteniendo aparte sus cambios de pitch/RNG y sus condiciones de
lista. Permanecen pendientes los productores de buffers, relevo de campos ADSR,
asignación/cadencia, mezcla y salida. No se usa el inicializador de workspace
como sustituto de su recarga por llamada. Presets e interfaz siguen aplazados.

[Evidencia y hashes](firmware/voice246-poly-release-validation.json) ·
[Release Core previo](drwave246-core-routing.md) ·
[Callbacks ADSR previos](cluster246-envelope-transitions.md) ·
[Plan activo](firmware-dsp-workplan.md).
