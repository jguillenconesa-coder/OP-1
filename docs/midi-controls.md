# Nombres para asignación MIDI y automatización

Pocket One 0.5.0 muestra nombres agrupados y únicos en los parámetros del host. Los identificadores internos se conservan para mantener proyectos/automatizaciones previos.

| Grupo | Ejemplos visibles |
| --- | --- |
| Sound | Sound: Engine, Sound: Tone, Sound: Color, Sound: Envelope Macro |
| Envelope | Envelope: Attack / Decay / Sustain / Release |
| Sound FX / LFO | Sound FX: Echo Mix / Echo Time / Echo Feedback / Drive; LFO: Rate / Depth / Destination / Shape |
| Sampler | Sampler: Root Note, Sampler: Loop |
| Tape | Tape: Play, Tape: Record Arm, Tape: Record Track, Tape: Track 1 Level…Track 4 Level, Tape: Playback Speed, Tape: Loop In / Out |
| Input / Output | Input: Gain, Input: Monitor, Output: Volume |
| Master | Master EQ: Low Gain / Mid Gain / High Gain / Mix; Master FX: Drive / Echo Mix / Echo Time / Echo Feedback |
| Sequencer / Arpeggiator | Sequencer: Run / Mode / Rate / Pattern Length / Gate / Swing / Note Velocity; Arpeggiator: Octaves |
| Pads | Pad 01: Start / End / Gain / Pitch / Reverse / Gated … Pad 24: … |
| Metronome | Metronome: Enable, Metronome: Level |

El número del pad usa dos cifras para facilitar su ordenación. Las unidades físicas verificadas se muestran cuando procede: Hz, BPM, segundos, dB, semitonos y nota MIDI. Attack/Decay/Release y otros macros siguen siendo controles normalizados: no se etiquetan como milisegundos que no representan.

En Ableton, usa **Configure** para exponer los parámetros que quieras y después **MIDI Map** para asignar tus controles. Toca/mueve el parámetro deseado o elígelo entre los que permite el host; los nombres anteriores ayudan a distinguir nivel general, mezcla, efecto de sonido y pad. Los nombres no fijan números CC ni introducen un segundo mapeo que interfiera con el DAW. El standalone sigue recibiendo notas, sustain y pitch bend; esta versión no añade un sistema propio de MIDI Learn.

Los botones de edición de cinta y de archivos no se convierten en CC por cambiar el nombre de los parámetros. En proyectos existentes, el host puede conservar etiquetas antiguas en su caché hasta volver a abrir el plugin o actualizar los parámetros expuestos.

0.6.0 añade **Sampler: Clean Playback** (botón CLEAN SAMPLE), común a sampler y kit. El parámetro se añade al final de la lista y los controles previos mantienen sus IDs y orden. Los bancos de referencia lo activan para conservar mejor el espectro original.


## Cuatro asignaciones que siguen el visor (0.7.0)

En Ableton, abre **Configure** y mueve una vez los cuatro mandos de color del
plugin en el set 1. Expón y asigna estos cuatro parámetros con **MIDI Map**:

| Control físico | Parámetro fijo del host |
| --- | --- |
| Azul, primero | Screen: Knob 1 Blue |
| Verde, segundo | Screen: Knob 2 Green |
| Blanco, tercero | Screen: Knob 3 White |
| Naranja, cuarto | Screen: Knob 4 Orange |

Al cambiar a los sets 2, 3 o 4 se conservan las asignaciones y cada mando controla
el valor de su mismo color en el visor. No hace falta calcular CC consecutivos
ni crear cuatro bancos en el controlador: se reutilizan las mismas cuatro
asignaciones. T1 es motor/sampler, T2 ADSR, T3 efectos y T4 LFO; también siguen
las páginas de MIX y TAPE. El verde de TAPE sigue la pista seleccionada incluso
si se automatiza su selección. El azul mueve la posición de la cinta.

Las asignaciones antiguas a **Sound: Tone**, **Envelope: Attack**, etc. continúan
controlando ese parámetro concreto. Para usar el comportamiento contextual hay
que reasignarlas **una sola vez** a los cuatro parámetros `Screen: Knob…`.
Guarda el dispositivo/pista como preset en Live para reutilizar su MIDI Map.
El plugin guarda la última página en el proyecto y la conserva al cerrar y
abrir su ventana; con la ventana cerrada los mandos siguen esa última página.
Los proyectos anteriores empiezan en el set 1.

Los cuatro mandos usan valores absolutos normalizados por el host; no añaden
un decodificador de encoders relativos ni MIDI Learn en el standalone. Para
escribir automatización de un destino fijo usa el parámetro descriptivo
original: la automatización de `Screen: Knob…` actúa sobre la página activa.
Los IDs y el orden de todos los parámetros existentes permanecen iguales;
los cuatro nuevos se añaden al final.
