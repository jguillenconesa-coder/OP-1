# Motores de audio y estado de fidelidad — 0.10.0

| Motor/función | Implementación actual | Verificación y límite |
| --- | --- | --- |
| FM | Síntesis propia con dos osciladores y modulación de fase | Audio, MIDI, bend, extremos y tiempo real. No es el FM completo original; campos de control investigados, tablas L1 pendientes. |
| Digital 246 CORE | Reconstrucción del oscilador Digital y envolvente rev246 opcional | 262.144 muestras / 2.048 estados completos, siete tasas de host; 16 presets adicionales. NCO, asignador, salida, mezcla y LFO propios. |
| PHASE anterior | Distorsión de fase propia | Se conserva para sesiones/presets existentes. |
| Phase 246 CORE | Reconstrucción Pd + envolvente rev246 opcional, a 88,2 kHz internos | Oráculos de instrucciones/estado y siete tasas de host; NCO, salida, mezcla y LFO propios. No se valida el aparato completo. |
| STRING | Karplus–Strong propio con retardo fraccional | Afinación y bend ±2 semitonos a 44,1/48/96 kHz, cuatro notas y reutilización segura del buffer. No es un port del String original. |
| SAMPLER | Reproducción mono interpolada, recorte, raíz y loop | CLEAN conserva la ruta sin filtro implícito/saturación seca. FILTER LFO explícito sí actúa. |
| DRUM | Bombo/caja/ruido propios | No reproduce el motor de batería original. |
| KIT | Hasta 24 pads, trim, gain, pitch, reverse y gate | Audio por pad, bancos, estado embebido, edición e importación. |
| FX / EQ / LFO | Delay/drive, EQ de tres bandas y LFO propios | MIDI, automatización, FILTER en rutas limpias y colas; no se han portado los efectos originales. |

## Cambios de String

El loop entero anterior no afinaba exactamente y el bend solo movía el componente
senoidal mezclado: la cuerda principal quedaba fija. El nuevo retardo fraccional
compensa el medio sample del filtro de promedio y sigue bend/LFO de pitch. Su
retune se suaviza en unos 3 ms y se limita el movimiento del cabezal para no leer
restos de una voz anterior. No se reserva ni borra un buffer entero por nota.

Se prueban 36 combinaciones (cuatro notas, tres tasas y tres bends), con un umbral
de error de tres cents en el estimador por autocorrelación; no es una medición
acústica del OP-1. Se mantienen nombres y ajustes de los presets, pero el timbre y
la afinación de STRING cambian ligeramente por esta corrección.

El buffer de cuerda sigue limitado a 8192 posiciones. Las notas extremadamente
graves a tasas altas alcanzan ese límite; no se promete afinación correcta fuera
de ese rango. No se afirma ausencia de aliasing en todos los motores.

## Pánico y colas

STOP/pánico y MIDI **All Sound Off (CC120)** reinician voces e historial de salida.
Los delays invalidan su historial con contadores, sin borrar sus buffers grandes
en el callback. Los cambios normales de preset reinician las voces y conservan
el historial del efecto master. La cinta grabada y los parámetros de mezcla se
conservan. CC123 / pedal mantienen su comportamiento de liberación de notas.

El host recibe una duración de cola que contempla la envolvente seleccionada y
los delays en cascada. Una release original muy lenta puede durar decenas de
segundos; no es una nota atascada. Pánico corta esa cola.

## Trabajo que continúa pendiente

1. Rutas completas de LFO/ganancia/velocidad y cuatro campos ADSR adicionales del
   firmware; afinación/controlador y mezclador originales.
2. FM completo y otros motores originales, con pruebas por instrucciones antes
   de conectarlos. Las regiones L1 cifradas no se tratan como coeficientes válidos.
3. Efectos originales y comparación acústica de presets completos con el aparato.
4. Ampliaciones de cinta/secuenciadores descritas en [botones](buttons-and-browser.md)
   y validación de arranque, Ableton 10.2.4, ASIO y dropouts en Windows real.

[Guía Digital](digital246-native.md) · [Guía Phase](phase246-native.md) · [Método ADSR](envelope246-research.md) · [Rendimiento](realtime-audio.md)
