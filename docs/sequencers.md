# Secuenciadores — Pocket One 0.4.0

Abre **SEQ SETTINGS** (el botón **SEQ: …** abajo a la derecha), abajo a la derecha, o **Shift + SEQ**. El botón muestra el modo seleccionado. SEQ muestra/oculta la fila de pasos y **PLAY en SYNTH/DRUM** inicia el motor; en TAPE/MIX, PLAY sigue controlando la cinta. STOP detiene ambos y libera las notas.

| Ajuste | Función |
| --- | --- |
| Forward / Reverse | Recorre los pasos activos desde el primero / último. |
| Pendulum | Ida y vuelta sin repetir los extremos: 1, 2, 3, 4, 3, 2… |
| Random | Selección pseudoaleatoria reproducible: volver al mismo tiempo del host recupera el mismo índice. No evita repeticiones. |
| ARP UP / DOWN / UP-DOWN | Ordena las notas mantenidas del teclado MIDI/en pantalla y las recorre en la dirección elegida. No utiliza las notas de la fila de pasos. |
| Pattern length | 1–16 pasos. En ARP determina el ciclo visual y la longitud de la máscara euclídea; las notas/octavas determinan el recorrido melódico. |
| Rate | Negras, corcheas, semicorcheas o fusas: 1/4, 1/8, 1/16, 1/32. |
| Gate | 10–100 % de la duración de cada paso. Controla cuándo se envía note-off; la cola depende del release/efectos del sonido. |
| Swing | 0–45 %: retrasa el segundo paso de cada pareja y acorta ese segundo paso. La duración total de la pareja permanece constante. No es la escala 50… del firmware original. |
| Arpeggio octaves | 1–3 octavas ascendentes, con notas superpuestas deduplicadas y límite MIDI 127. |
| Velocity | Velocidad fija 10–100 % para las notas generadas; el toque directo mantiene su propia velocidad MIDI. |
| Euclidean rhythm mask | Filtra los disparos con un reparto uniforme de pulsos sobre la longitud del patrón. Funciona también en ARP. |
| Euclidean pulses / rotation | 0–16 pulsos y desplazamiento 0–15 pasos. Los pulsos se limitan a la longitud elegida. Cero pulsos produce silencio. |

En los cuatro modos de pasos, clic activa/desactiva una nota; **Shift + paso** escribe la última nota tocada. Los pasos fuera de la longitud quedan deshabilitados pero conservan sus notas para recuperarlas al alargar el patrón. Con máscara euclídea, los silencios escritos siguen siendo silencios: para oír todos los pulsos, rellena las notas o usa ARP.

En ARP, las notas mantenidas se actualizan con sus offsets MIDI. Los disparos permanecen en la rejilla: una nota pulsada entre pasos entra en el próximo disparo. Soltar todas las notas libera inmediatamente la voz generada, respetando la cola del sonido. Se distinguen los canales al soltar notas, pero los acordes se reúnen en un solo arpegiador. No hay latch ni sustain del arpegiador. El sonido directo de las teclas se sustituye por ARP mientras esté habilitado el secuenciador en ese modo.

El tempo y la posición del DAW tienen prioridad. El motor sigue paradas, reinicios, loops y saltos, también con PPQ negativo. Sin host, usa el tempo interno. Gate, swing y rate se calculan por muestra; no añaden un bloque de latencia. Cambiar el modo libera las voces anteriores. Los ajustes son automatizables y se guardan con el proyecto/sesión; cambiar un preset o un slot de sonido no los sustituye.

Los proyectos anteriores se abren con Forward, longitud 16, rate 1/16, gate 100 %, swing 0, una octava, velocidad 80 % y máscara desactivada. Guardar en 0.4.0 y volver a una versión anterior perderá las funciones que esa versión no conoce.

Estas funciones son propias de Pocket One. No se afirma equivalencia con Endless, Finger, Sketch o Tombola ni identidad con el arpegiador del OP-1. El análisis estático de su base de datos confirma estructuras y nombres, no sus algoritmos de reproducción.
