# Dr. Wave rev246: composición del argumento con eventos y ADSR

Se ejecutan conjuntamente el constructor **`0x01a85014`**, activación VoiceCtl
**`0x01a84414`** o retirada **`0x01a85058`**, y el renderer ADSR **`0x01912128`**.
El comparador compone las implementaciones independientes existentes sin añadir
un dispatcher al producto. Se comprueba que el argumento construido, sus campos
y efectos compartidos son compatibles en este contexto.

**El orden de llamadas pertenece al harness.** No se ha recuperado aquí el
llamador original que las agenda. La lista inicial y la voz solicitada son
explícitas: la activación aislada no inserta una nota en esa lista, no asigna
una voz ni inicia el ADSR común. No se presenta esta composición como un
instrumento integrado ni como una nueva ruta original de note-on completa.

## Secuencia contrastada

1. Se prepara el contexto de la llamada y un argumento de 16 bytes con relleno
   previo, nota/velocidad recibidas como palabras raw y globales explícitos.
2. El constructor original escribe nota transformada, velocidad completa, byte 1
   y pitch explícito cero. No hay overrides del argumento tras su retorno.
3. Según la acción explícita del registro, se llama a activación o retirada
   originales **con ese mismo argumento**, sin sustituir su nota ni velocidad.
4. Se captura toda la imagen de estado. Luego se renderiza un ADSR seleccionado
   por el ensayo y se vuelve a capturar la imagen y la salida con guardas.

La implementación C++ compone `voice246ConstructNoteArgument`,
`drWave246ActivateNote`, `voice246PolyRelease` o `voice246ReleaseSelectors` y
`adsr246Block`. No se cambia su comportamiento ni se añade lógica de host.
Los retornos de aceptación/rechazo del C++ siguen siendo propios del adaptador,
no valores de retorno atribuidos a las rutinas originales de eventos.

## Contexto y efectos observados

El sonido seleccionado está en 0–7. El discriminador del constructor se compara
como palabra completa: se ensayan 1, 0, 2 y valores con bits altos y mitad/byte
bajo igual a uno. La tabla de ocho ajustes contiene valores normales y patrones
raw de estrés. Las notas recibidas se eligen con wrap unsigned para que las
**notas construidas sean válidas, 0–127**, en este contrato enlazado. Los valores
fuera de rango siguen siendo casos del constructor aislado, no de activación.

El ensayo ejercita cada nota construida con ambas acciones. Pitch explícito cero
sale del constructor y selecciona la consulta de tabla en las activaciones
aceptadas. La tabla de 128 notas se obtiene con el fragmento original de copia
ya contrastado, terminado con RTS propio; el coeficiente de afinación sigue
siendo explícito en el índice 1200. No se ejecuta el inicializador completo ni
se deduce una calibración predeterminada.

Cuando Core acepta la activación, Dr. Wave borra fases/acumulador según su
callback y VoiceCtl escribe pitch, velocidad y actividad de la voz explícita.
Con sonido cargado distinto del seleccionado conserva el pool y esos registros.
La activación no modifica los estados ADSR en la captura anterior al render.

Para retirada se ensayan los selectores por sonido 0–4 con modo VoiceCtl externo
cero y flag válido. La lista se prepara explícitamente antes de los casos
independientes y al inicio de cada stream; después se retiene. Se conservan las
reglas ya contrastadas de coincidencia, compactación/relleno, voz de la entrada
para selector 0 y última nota/una-seis voces/RNG para selectores 1–3. El selector
4 conserva el estado antes del render. No se atribuye identidad comercial a los
selectores ni se supone que la lista fue creada por las activaciones del ensayo.

## Cobertura y conservación

- **512 casos independientes y cuatro streams de 64 llamadas: 768 registros.**
- **384 activaciones: 277 aceptadas y 107 bloqueadas**; cada índice de nota
  0–127 aparece en una activación aceptada.
- **384 llamadas de retirada**, con **247 entradas retiradas** en el contexto
  del ensayo. Se ejercitan los cinco selectores, búsquedas presentes/ausentes,
  duplicados y listas conservadas entre eventos.
- **384 construcciones en bypass y 384 consultas de ajustes**. La velocidad
  conserva su palabra completa al construir; la activación aplica después su
  conversión previamente contrastada, sin asignarla al constructor.
- **98.304 posiciones ADSR posteriores: 78.720 escritas y 19.584 conservadas.**
  El RNG/dither continúa desde los efectos de las retiradas.
- **4.524 bytes de imagen completa antes y después**: lista, contador/vecinos,
  seis objetos Dr. Wave y vecinos posteriores, seis ADSR, scratch y guardas,
  registros VoiceCtl, parámetros de ocho sonidos, RNG/valor anterior y vecinos,
  ready, sonido seleccionado/cargado, punteros Core, afinación/índice y vecinos,
  discriminador y tabla de ajustes con vecinos. El argumento construido y las
  otros 16 bytes de entrada del llamador se capturan, junto a salida/guardas y retorno
  ADSR. La tabla de notas se compara completa al principio y al final.

Los objetos Dr. Wave se precargan con vtables válidas y otros campos de estrés;
no se presentan como estados listos para su render de audio. El pool ADSR tiene
su flag ready explícitamente activado y no se ejecuta su construcción diferida.
Los streams retienen lista, objetos, ADSR/scratch, registros VoiceCtl y RNG/valor
anterior; selección, ajustes, controles, afinación y acción siguen siendo entradas
por llamada. `0x01dfe72c` conserva la última palabra de la tabla de notas, no una
guarda independiente. `0xff800164` es scratch compartido, no guarda del pool ADSR.

Los fixtures contienen 768 registros de **4.828 bytes de entrada** y
**9.356 bytes de resultado**, con 512 bytes adicionales de tabla al principio y
al final de la salida. Solo se publican números propios del ensayo; firmware,
instrucciones, desensamblados y ELF permanecen privados.

## Validación y coordinación

Dos reproducciones privadas coinciden byte a byte. Pasan **44 suites de núcleo**
y **46 con JUCE** en Linux, más ASan/UBSan del comparador (`detect_leaks=0` por
la restricción ptrace). Windows/macOS se verifican mediante CI de la rama; no
se han ensayado localmente ni se ha validado DAW, ASIO o temporización física.
El oráculo usa GNU BF524 con los dos parches documentados de writeback y su
preflight halfword. REA sirve para inspección; no descifra ni compara muestras.

La investigación paralela del **setter `0x01a8633c`** y su enlace al constructor
está reservada a la otra cuenta. Este ensayo mantiene su tabla como entrada
explícita y no modifica el setter ni sus archivos. El plan común se coordina
antes de integrar los resultados de ambas ramas.

```bash
python3 scripts/verify-drwave246-note-event-chain.py \
  --ldr /privado/OP1_vdk.ldr --assembler /herramientas/bfin-as \
  --linker /herramientas/bfin-ld --simulator /herramientas/run-halfword-v2 \
  --testutils /fuentes-gdb/sim/testsuite/bfin --output /privado/eventos-nuevo
cmake --build build-dsp --target drwave246_note_event_chain_tests
ctest --test-dir build-dsp -R '^drwave246_note_event_chain$' --output-on-failure
```

Siguen pendientes el dispatcher/asignador y su cadencia, productores de buffers
de pitch/volumen, consumidor de campos ADSR, mezcla y salida. No se llama al
render de audio de Dr. Wave ni se añaden muestras a su recuento de render aislado.
No se conectan estas pruebas al instrumento, MIDI, automatización o sesiones.

[Evidencia y hashes](firmware/drwave246-note-event-chain-validation.json) ·
[Constructor](voice246-note-argument.md) ·
[Activación](drwave246-note-activation.md) ·
[Retirada](drwave246-release-selectors.md) ·
[Plan](firmware-dsp-workplan.md).
