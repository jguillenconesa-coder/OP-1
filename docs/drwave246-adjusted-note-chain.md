# Dr. Wave rev246: ajustes originales antes de los eventos de nota

Este ensayo enlaza el setter **`0x01a8633c`** con constructor **`0x01a85014`**,
activación **`0x01a84414`** o retirada **`0x01a85058`**, y un render ADSR común
**`0x01912128`**. Reutiliza las reconstrucciones independientes fusionadas en
las PR #18–20, sin modificar el DSP del instrumento ni añadir un dispatcher.

El avance concreto es que la tabla de ajustes que consume el constructor se
escribe mediante los **setters originales**, en vez de quedar como una imagen
directa del llamador en ese punto. **Valores deseados y orden de ejecución siguen
siendo del ensayo**: no se recuperan valores predeterminados, inicialización de
arranque, procedencia de ajustes o la cadencia original de los eventos.

## Secuencia y contrato

Al inicio de cada caso independiente y cada stream:

1. Se preparan las regiones vigiladas y vecinos del contexto acotado.
2. Se llena la tabla de ocho palabras con `0xa5a5a5a5`, un patrón de prueba
   ajeno a sus valores válidos. Después se llama al setter original ocho veces,
   una por slot, con ajustes explícitos válidos de -4 a 4.

En cada registro:

1. Se llama de nuevo al setter original con un slot 0–7 y valor -4–4 explícitos.
   La tabla se conserva entre registros del stream.
2. Se captura la tabla completa con sus vecinos, **48 bytes**, inmediatamente
   después del setter y **antes** del constructor.
3. Se construye el argumento original y se pasa sin overrides a activación o
   retirada originales. Se captura la imagen compartida completa de 4.524 bytes.
4. Se ejecuta un ADSR seleccionado explícitamente y se vuelven a capturar imagen,
   salida, guardas, argumento y entradas del llamador.

No se vuelve a escribir directamente la tabla tras los setters. El C++ compara
esta secuencia mediante `voice246SetNoteAdjustment`, constructor y los helpers
existentes de eventos/ADSR. La escritura de un valor idéntico también se ensaya;
no se añade un mecanismo de notificaciones ni se atribuye un bool al retorno
original del setter.

## Cobertura y observaciones

- **768 registros**: 512 independientes y cuatro streams de 64 llamadas.
- **4.896 llamadas al setter**: 4.128 para suministrar las ocho palabras iniciales
  en 516 resets del harness y 768 cambios posteriores. Las 72 parejas slot/ajuste
  aparecen en los cambios; 68 cambios escriben un valor ya presente.
- **384 constructores en bypass y 384 consultas de tabla**. En 48 consultas se
  lee el slot recién escrito; en 336 se lee otro slot. Selección del constructor
  y slot del setter son entradas independientes.
- **384 activaciones**: 277 aceptadas y 107 bloqueadas por diferencia entre
  sonido cargado y seleccionado. Las 128 notas construidas aparecen en
  activaciones aceptadas.
- **384 llamadas de retirada**, con 247 entradas retiradas; se conservan los
  selectores por sonido 0–4 y los casos presentes, ausentes y duplicados.
- **98.304 posiciones ADSR**: 78.720 escritas y 19.584 preservadas. La semilla
  compartida continúa desde los efectos de los eventos anteriores.

Las notas recibidas son palabras raw elegidas para obtener índices construidos
0–127 con las sumas módulo 2³² del constructor; no se presentan como mensajes
MIDI o coordenadas de teclado originales. La velocidad se construye completa y
su conversión posterior sigue perteneciendo a activación. Afinación, controles,
sonidos, acciones, voz a renderizar y volumen son entradas explícitas.

Se comparan los 4.524 bytes antes/después del render: lista/contador y vecinos,
seis objetos Dr. Wave y vecinos posteriores, seis ADSR, scratch y guardas,
registros VoiceCtl, parámetros de ocho sonidos, RNG/valor anterior y vecinos,
ready, selección/sonido cargado, punteros Core, afinación/índice, discriminador y
tabla de ajustes con vecinos. La captura de tabla tras los setters añade una
frontera observable anterior al constructor. Argumento, 24 bytes adicionales de
entradas del harness, guardas de salida y retorno ADSR también se comparan. La
tabla de 128 notas se captura completa al principio y al final.

`0x01dfe72c` se conserva como la última palabra de la tabla de notas, no una
guarda independiente; `0xff800164` es scratch compartido del ADSR. Los objetos
Dr. Wave contienen vtables válidas y otros campos de estrés: no se presentan
como estados renderizables. El flag ADSR ready se suministra explícitamente;
no se ejecuta la construcción diferida ni los caminos de assert.

## Límites y coordinación

El harness sigue eligiendo el orden y sembrando la lista. La activación aislada
no inserta una nota en ella, no asigna voz ni inicia la envolvente común. El
ADSR posterior sigue siendo una llamada explícita. No se ejecuta el renderer
de audio de Dr. Wave ni se añade esta cifra a su recuento de muestras de audio.

Siguen pendientes dispatcher/asignador, productores de buffers de pitch/volumen,
consumidor de campos ADSR, reloj, mezcla y salida. No se sustituye L1 cifrada por
interpolación, una fórmula o un supuesto scheduler. El estado vigilado no es una
captura completa de RAM, pila y CPU del dispositivo.

La otra cuenta tiene reservada la consulta legible **`0x01a863a8`**, con lista y
palabra persistente `0x01dfe4d4`. Este ensayo no la ejecuta ni reconstruye esa
consulta o un asignador completo. Sus archivos quedan separados.

Dos reproducciones privadas coinciden byte a byte. El oráculo aislado de setter
aportado por la otra cuenta también reproduce sus fixtures usando nuestro LDR
y herramientas, sin regenerarlos. Pasan **46 suites de núcleo y 48 con JUCE**
en Linux y ASan/UBSan del comparador (`detect_leaks=0` por el límite de ptrace).
Windows/macOS quedan pendientes de CI. No se valida DAW/ASIO, latencia, dropouts
ni hardware físico.

El modelo es GNU BF524 con ambos parches documentados de writeback; se comprueba
el probe halfword antes del firmware. REA proporciona inspección estática, no
descifrado ni comparación numérica. Firmware, instrucciones, desensamblados y
ELF permanecen privados. No se modifica MIDI, automatización, sesiones, presets,
recursos o interfaz.

## Reproducción y formato

```bash
python3 scripts/verify-drwave246-adjusted-note-chain.py \
  --ldr /privado/OP1_vdk.ldr --assembler /herramientas/bfin-as \
  --linker /herramientas/bfin-ld --simulator /herramientas/run-halfword-v2 \
  --testutils /fuentes-gdb/sim/testsuite/bfin --output /privado/ajustes-eventos-nuevo
cmake --build build-dsp --target drwave246_adjusted_note_chain_tests
ctest --test-dir build-dsp -R '^drwave246_adjusted_note_chain$' --output-on-failure
```

La entrada contiene 768 registros de **4.836 bytes**: cabecera de 40, imagen de
4.524 y salida inicial con guardas de 272. La salida tiene la tabla de notas
(512 bytes), 768 registros de **9.412 bytes** y la tabla final (512 bytes).
Cada resultado contiene dos imágenes de 4.524, salida con guardas (272), retorno
ADSR (4), captura de tabla post-setter (48) y cabecera final/argumento (40).
Solo se publican fixtures numéricos propios y código de ensayo.

[Evidencia y hashes](firmware/drwave246-adjusted-note-chain-validation.json) ·
[Setter aislado](voice246-note-adjustment.md) ·
[Cadena anterior](drwave246-note-event-chain.md) ·
[Plan](firmware-dsp-workplan.md).
