# Interfaz basada en la referencia del OP-1

La distribución 0.2.2 se rehízo usando las dos fotografías y el manual de usuario v2.2 aportados en la conversación. El apartado 2 del manual describe altavoz, volumen, modos, transporte, flechas/Shift, pantalla, T1–T4, encoders, sonidos 1–8 y teclado. Se reproducen su posición relativa y los rasgos de las teclas mediante gráficos vectoriales propios; el PDF y las fotografías originales no se redistribuyen.

- Carcasa gris, rejilla de altavoz, pantalla negra pequeña, cuatro encoders con tapas azul, verde, blanca y naranja.
- Cuatro botones SYNTH / DRUM / TAPE / MIX. MIX utiliza la mezcla de las cuatro pistas implementadas; no añade EQ ni efectos de master que aún no existen.
- T1: motor; T2: envolvente; T3: efectos; T4: LFO. En cinta/mezcla, T1–T4 seleccionan pista y los encoders controlan sus cuatro niveles.
- Botones 1–8: acceso rápido a sonidos. Shift + botón asigna el preset seleccionado a ese acceso mientras el editor está abierto.
- Play inicia el secuenciador en modos de sonido y la cinta en TAPE/MIX; Stop detiene ambos y desarma REC. REC arma la pista seleccionada y arranca la cinta.
- Flechas: octava en modos de sonido; en cinta, la izquierda rebobina y la derecha alterna velocidad x1/x2. No se presentan como una implementación completa de la edición de cinta del hardware.
- Shift muestra los 16 pasos sobre el teclado para editar la secuencia. Shift + paso escribe la última nota tocada.
- Dos octavas, teclas blancas alargadas con relieve y notas alteradas con círculos negros.
- La barra inferior contiene el selector completo de presets, BANK, SAVE/OPEN y controles auxiliares. Evita ocultar las funciones propias del plugin en botones sin correspondencia en el hardware.

La pantalla muestra el motor, envolvente, LFO, efectos o carretes de cinta vinculados a la posición real de reproducción. No es una imagen estática ni una copia del firmware. La rejilla de altavoz y los detalles de micrófono son visuales: no incorporan emulación de altavoz, micrófono o radio.

## Pantallas y animaciones: referencia OTTO

Se consultó OTTO-1 en el commit `ba534c6c1ed93a6b15cd13a93287b9e8f6f8ca6b`:

- [Nuke](https://github.com/antoinehng/OTTO-1/blob/ba534c6c1ed93a6b15cd13a93287b9e8f6f8ca6b/src/engines/synths/nuke/nuke.cpp): lenguaje vectorial de diales, puntos y relaciones de parámetros.
- [TapeScreen](https://github.com/antoinehng/OTTO-1/blob/ba534c6c1ed93a6b15cd13a93287b9e8f6f8ca6b/src/engines/studio/tapedeck/tapescreen.hpp): carretes que giran con la posición, bobinado y timeline.
- [DrumSampleScreen](https://github.com/antoinehng/OTTO-1/blob/ba534c6c1ed93a6b15cd13a93287b9e8f6f8ca6b/src/engines/drums/drum-sampler/drum-sampler.cpp): formas de onda, límites y progreso de reproducción.

La implementación JUCE es propia: reactor FM y órbitas, deformación de fase, cuatro cuerdas vibrantes, waveform del sample con cursor, pads reactivos, indicador móvil ADSR, barrido LFO, ecos y carretes. El sampler y la cinta usan su posición real; la actividad/envolvente, voces, LFO y tiempo de audio se publican como un número acotado de atómicos al final del bloque. No se lee el buffer de muestra ni se bloquea el procesador desde el dibujo.

La interfaz se actualiza a 30 fps y repinta la pantalla y el indicador, sin redibujar toda la carcasa en cada tick. Las formas y coordenadas se redibujaron para Pocket One; no se copiaron los assets ni el código de OTTO (MPL-2.0). Las animaciones de FM y cuerda son representaciones visuales, no mediciones físicas ni un analizador espectral. Los tests rasterizan las once vistas y verifican respuesta a cambios de telemetría.
