# Rev246: secuencias de activación, notas retenidas y retirada

Este bloque contrasta **la composición de adaptadores ya reconstruidos** con
llamadas completas a las entradas originales de activación `0x01a84604` y
retirada `0x01a85840`, más constructor, setters y render ADSR. No añade una
nueva fórmula DSP ni cambia los helpers o el plugin. El orden de los eventos
pertenece al harness: no se ha recuperado un dispatcher/clock de ejecución.

La base es `ac34c32`, con [lista vacía, PR #28](drwave246-note-modes.md) y
[lista no vacía, PR #29](drwave246-held-note-modes.md) fusionadas. La retirada
usa el [dispatcher contrastado en PR #25](voice246-release-dispatch.md), con
discriminador cero y mapa de notificación explícitamente vacío. Una reproducción
privada independiente de los 1.536 casos de la PR #29 coincide con sus fixtures
antes de desarrollar esta composición.

## Secuencia y contrato del ensayo

Se ejecutan **12 secuencias de 128 eventos**, cuatro por selector 1–3. Cada una
contiene 16 ciclos de ocho pasos, con notas distintas A, B, C y X:

| Paso | Evento | Resultado de lista |
| --- | --- | --- |
| 1 | Activar A | Se añade desde lista vacía. |
| 2 | Activar A de nuevo | Se suprime el duplicado completo. |
| 3 | Activar B | Se añade desde lista no vacía. |
| 4 | Retirar A | Se retira la primera entrada y se compacta B. |
| 5 | Activar C | Se añade detrás de B. |
| 6 | Retirar C | Se recupera pitch de B mediante la retirada original. |
| 7 | Retirar B | La lista queda vacía; se solicita release ADSR. |
| 8 | Retirar X | Búsqueda ausente, lista vacía conservada. |

Tras cada evento se ejecuta un bloque ADSR de 128 muestras, con voz y volumen
explícitos. El constructor original `0x01a85014` precede tanto a on como a off:
los argumentos conservan su metadata 1 y pitch explícito cero. La notificación
posterior al off consulta su mapa vacío y retorna antes del transporte; no se
borra metadata ni se sustituye esa dependencia por un retorno parcheado.

Al inicio de cada secuencia se suministran lista vacía, actividad y otros
estados de prueba. Los ocho ajustes se inicializan mediante setters originales
tras poison A5; un setter original por evento mantiene un ajuste a cero. Todos
los ajustes son explícitamente cero en este bloque, no valores de fábrica.
La nota raw se elige para que el constructor produzca A/B/C/X válidas 0–127.

**Durante los 128 eventos se conservan sin overrides lista, contador, gate,
actividad, pitch, velocidad, seis ADSR, seis Dr. Wave, RNG y memoria seleccionada.**
No se vacía la lista entre llamadas, no se repone actividad por evento ni se
simula un commit de campos ADSR pendientes. La lista se vacía por el off original.
Controles, sonido seleccionado/cargado, afinación e índice de afinación, punteros
Core válidos/auxiliares nulos del contexto y orden de render siguen siendo entradas
explícitas. No se prueba concurrencia ni cambios originales de sonido en vivo.

Los selectores 1 y 2 procesan una voz; el 3 procesa seis. Desde lista vacía,
la actividad determina inicio frente a solicitud de reinicio. Con lista no
vacía, el selector 2 escribe pitch directamente y los selectores 1/3 solicitan
reinicio pendiente, conforme a sus contratos previos. Las retiradas recuperan
pitch o solicitan release según lista y selector.

**Retirar la última nota no demuestra una voz libre:** los bytes de actividad
se conservan en el recorrido probado. El siguiente on puede entrar por lista
vacía y actividad no nula, y solicitar reinicio. No se añade una limpieza propia
para evitarlo ni se infiere quién realiza ese relevo en el firmware completo.

## Resultados y comparación independiente

Las **1.536 llamadas originales** producen:

- 768 on: 192 desde lista vacía y 576 desde lista no vacía; 576 inserciones y
  192 duplicados suprimidos.
- 768 off: 576 retiradas y 192 búsquedas ausentes. Cada ciclo termina con lista
  vacía, comprobado por el comparador.
- 8 inicios Core aceptados, 2 rechazados después de preparar ADSR y 1.398
  solicitudes de reinicio pendiente entre los on. Los números dependen del
  estado inicial y el orden explícitos; no representan una tasa del aparato.
- Las 128 notas entre los on de cada selector, seis voces renderizadas,
  velocidades de palabras completas con patrones en ambas mitades, actividad
  inicial con valores 0/1/128/255 y aceptación/rechazo por sonido cargado.
- 196.608 posiciones ADSR: 156.032 escritas y 40.576 conservadas. No se añaden
  muestras de audio Dr. Wave al total de sus quince oráculos previos.

`Tests/DrWave246NoteLifecycleTests.cpp` compone los helpers existentes de
setters/constructor, lista vacía/no vacía y dispatcher de retirada, sin cambiar
sus implementaciones. Compara después de cada evento y render una imagen de
**4.748 bytes**: lista/contador/vecinos, ADSR/scratch/guardas, objetos Dr. Wave,
VoiceCtl, RNG, controles, ajustes y dependencias seleccionadas. Añade el mapa de
notificación con vecinos y monitores de salida de `0x01aadb1c` / `0x018b600c`.
Se comparan también argumento completo, snapshot del setter, muestras, guardas
del audio y retorno de finalización ADSR.

El argumento local en la pila del dispatcher no se captura en este ensayo:
su copia por campos y padding tiene su prueba independiente de PR #25.
La imagen no representa toda la RAM, pila o registros de CPU. Las guardas
vigilan los límites declarados, sin afirmar preservación de memoria no observada.

Dos reproducciones privadas nuevas coinciden byte a byte. GNU BF524 utiliza
ambos parches documentados, con probe halfword previo y watches PC verificadas
con un programa propio. Se detiene si entra en polifonía, camino activo de
notificación/transporte, discriminadores excluidos, helper de error o rango L1.
REA/objdump inspecciona las dos funciones completas mediante símbolos sintéticos;
no descifra ni valida numéricamente.

Pasan **53 suites CTest de núcleo Linux**, **55 con JUCE/plugin** y el nuevo
comparador bajo ASan/UBSan; detección de fugas desactivada por restricción ptrace
del entorno. Windows/macOS quedan para CI, sin atribuir ejecución local.

## Formato y reproducción

Entrada: **1.536 registros de 5.060 bytes**. Cada registro contiene cabecera de
40 bytes, imagen de 4.748 y audio con guardas de 272. El campo +12 es acción:
0 on, 1 off; +0 indica reinicio del contexto solo al principio de cada secuencia.

Salida: tabla de notas de 512 bytes, **1.536 registros de 9.860**, tabla de 512.
Cada registro tiene imagen tras evento, imagen tras render, audio/guardas 272,
retorno 4, snapshot de ajustes 48 y cabecera/argumento final 40. Las regiones y
hashes constan en [la evidencia](firmware/drwave246-note-lifecycle-validation.json).

```sh
python3 scripts/verify-drwave246-note-lifecycle.py \
  --ldr /privado/OP1_vdk.ldr \
  --assembler "$POCKET_BFIN_AS" --linker "$POCKET_BFIN_LD" \
  --simulator "$POCKET_BFIN_SIM" --testutils "$POCKET_BFIN_TESTUTILS" \
  --output /privado/note-lifecycle-nuevo
```

La salida debe ser nueva, privada y externa al checkout. Sigue
[la preparación entre cuentas](new-account-dsp-setup.md). Firmware, instrucciones
originales, desensamblados y ELF no se publican; los fixtures contienen datos
numéricos y capturas del ensayo propio.

## Límites y siguiente bloque

No se completa el instrumento: siguen pendientes consumidor L1 de pitch/velocidad
ADSR, productor del buffer de pitch, reloj, mezcla y salida. El mapa vacío excluye
transporte real. No se prueba temporización, Ableton, hardware ni ausencia de
dropouts. MIDI/automatización/sesiones, presets, bancos e interfaz no cambian.

La otra cuenta tiene reservado **pitch explícito no cero con lista vacía** en
`collab/drwave-empty-explicit-pitch`, con archivos propios. Este ensayo conserva
el pitch cero emitido por el constructor; no duplica ese contexto ni inserta
una palabra explícita tras él.
