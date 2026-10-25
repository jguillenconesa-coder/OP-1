# Presets y kits de fábrica de Pocket One

El selector **FACTORY SOUNDS** incluye **25 presets originales**: 18 sonidos de síntesis/batería, cuatro instrumentos muestreados y tres kits de ocho muestras. Están incorporados en el VST3 y en la aplicación: no requieren descargas ni localizar carpetas. Elegir un sonido cambia los parámetros del sintetizador; no borra la cinta ni el patrón. Selecciona los sonidos con el transporte parado.

- FM: Soft keys, Glass bells, Digital tine, Hollow bass, Metallic lead, Orbit chimes.
- PHASE: Slow pad, Round bass, Reed keys, Warm organ, Glass sweep.
- STRING: Plucked wire, Nylon cloud, Muted pluck, Steel shimmer, Bowed hybrid.
- DRUM: Synth machine y Bright hats, generados por síntesis.
- SAMPLE: Velvet keys, Glass mallet, Air choir y Low pluck, afinados en MIDI 60 (C4 según la convención del plugin).
- KIT: Studio, Circuit y Dust. Cada uno tiene kick, snare, hi-hat cerrado/abierto, tom, clap, crash y rim.

## Tocar los kits

Selecciona un preset **KIT**. Las notas MIDI **60–67** disparan los ocho sonidos; el patrón se repite cada ocho notas por encima de 60. También se admiten notas GM: 36 kick, 38 snare, 42 hi-hat cerrado, 46 abierto, 45 tom, 39 clap, 49 crash y 37 rim. La denominación de octavas de Live puede variar; utiliza los números MIDI como referencia.

Son muestras de un solo disparo: soltar la tecla no corta el golpe. TONE ajusta la afinación y COLOR la longitud. Volumen, velocidad MIDI y efectos siguen activos. Las ocho muestras y el preset seleccionado se guardan dentro de la sesión; no dependen de rutas externas.

Las 28 muestras WAV se incluyen además en `Assets/Factory`, en los paquetes de fuentes y en **Factory Samples** dentro de la carpeta de instalación Windows. Se pueden arrastrar a otro sampler. Su generador reproducible es `scripts/generate-factory-samples.py`; los WAV son CC0-1.0. No son grabaciones del hardware original.

## Referencias para aproximarse al OP-1 real

| Fuente | Utilidad y límites |
| --- | --- |
| [OP1.fun](https://op1.fun/) | Referencia de patches de la comunidad. El acceso directo desde este entorno está bloqueado; no se ha descargado ni auditado un banco concreto. La existencia del servicio se verifica en su [componente público de preescucha](https://github.com/dustMason/op1-patch-preview). |
| [te-op1](https://github.com/jstnfst/te-op1) | Biblioteca comunitaria, presets y documentación AIFF/APPL del **OP-1 Field**, según su README. Distinguir del OP-1 original. Algunos patches de sampler descargados llevan un tono de sustitución en vez de audio original. No se incorporan sus archivos sin comprobar condiciones por archivo. |
| [OP Patch Utility](https://github.com/AlexCharlton/op-patch-util) | Documenta creación/inspección de patches AIFF de sintetizador muestreado y batería para OP-1/OP-Z, hasta 24 teclas. Referencia de formato, no banco oficial de fábrica. |
| [Manual oficial del OP-1 original](https://teenage.engineering/guides/op-1/original/synthesizer) | Referencia prevista para los motores y controles; la consulta directa está bloqueada en este entorno. |

No se ha localizado y validado un banco oficial de fábrica redistribuible. Un preset de síntesis del hardware contiene parámetros de su motor; **Pocket One todavía no importa esos parámetros ni emula todos esos motores**. Un AIFF de batería puede contener varios golpes y marcas dentro de un solo archivo: el botón LOAD actual lo importa como una muestra melódica, no convierte automáticamente sus slices en un kit. Los presets actuales aproximan familias de timbres FM, fase, cuerda y sampler; no se ha medido su semejanza frente a presets originales.

### Patches concretos localizados

En el directorio público [`te-op1/presets`](https://github.com/jstnfst/te-op1/tree/master/presets) se comprobaron archivos de referencia como:

- [Gentle bass / PHASE](https://github.com/jstnfst/te-op1/blob/master/presets/phase-nitro-element-gentle_bass.json).
- [Realistic cello / FM](https://github.com/jstnfst/te-op1/blob/master/presets/fm-spring-tremolo-realistic_cello.json).
- [Violons / STRING](https://github.com/jstnfst/te-op1/blob/master/presets/string-grid-tremolo-violons.json).
- [Glass harp / DIGITAL](https://github.com/jstnfst/te-op1/blob/master/presets/digital-punch-value-glass_harp.json).

Son referencias comunitarias documentadas en un proyecto para Field; su presencia no demuestra que formen parte del banco de fábrica del OP-1 original ni concede por sí sola permiso para redistribuirlas. En Pocket One, Round bass y Bowed hybrid ofrecen familias de timbre relacionadas. DIGITAL no está implementado como motor propio y el pad de fase no lo sustituye de forma equivalente. Los parámetros numéricos de esos patches no se han copiado ni convertido automáticamente.
