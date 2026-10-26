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
