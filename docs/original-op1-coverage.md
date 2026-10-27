# Objetivo: OP-1 original completo

La meta es cubrir motores, filtros, efectos, envolventes, modulación, modos de
ejecución y sonidos documentados. La versión 0.13.0 **todavía no es una réplica
completa**. Esta matriz distingue un núcleo contrastado de una aproximación
propia o una función pendiente; cambiar el nombre de un modelo no completa un port.

**Prioridad activa: reconstrucción del DSP del firmware.** Presets, bancos y vídeo
quedan aplazados. El [plan de trabajo por componentes](firmware-dsp-workplan.md)
mantiene un único bloque activo: Cluster, con sus dependencias y pruebas explícitas.

Fuentes: manual v2.2 suministrado por el usuario, 56 páginas, y firmware rev246
inventariado. Las páginas 53–54 del PDF muestran motores y efectos; 47–50, LFO;
51–52, los ejercicios de sonidos. Se revisaron las páginas como imágenes cuando
la extracción de texto omitía los diagramas. No se distribuyen el PDF, firmware,
DB ni instrucciones originales. REA verifica el inventario SQLite; GNU BF524
ejecuta rutinas aisladas para los oráculos de DSP.

## Motores

Los IDs son observaciones/referencias del perfil rev246, no el orden del selector
del plugin. Ese selector mantiene su orden para conservar automatizaciones.

| ID | Motor objetivo | Estado actual |
| --- | --- | --- |
| 0 | FM de cuatro operadores | FM propio de dos osciladores disponible. Se contrastaron campos de control; falta el render original y validar tablas L1 cifradas. |
| 1 | Digital | Núcleo reconstruido, 262.144 muestras / 2.048 estados; integración, mezcla y voz propias. 16 presets CORE. |
| 2 | Dr. Wave | Ruta candidata localizada; render y tablas pendientes de oráculos/port. |
| 3 | String | Modelo Karplus–Strong propio afinado; pendiente el waveguide original y todos sus controles. |
| 4 | Cluster | Núcleo aislado: 262.144 muestras con coeficientes de prueba y 81.920 con cinco normalizaciones legibles convertidas por el helper original. 24 coeficientes de amortiguación y 60 casos de factoría/reset de seis slots contrastados. Faltan tres normalizaciones y la integración completa; [avance](cluster246-normalization.md). |
| 5 | Pulse | Render localizado en L1 cifrada (`0xffa04b88`), sin imagen descifrada validada. Port pendiente; [bloqueo](firmware/pulse246-l1-blocker.json). |
| 6 | Synth sampler | Reproductor propio con CLEAN, raíz/loop/trim; banco de audio importable. No es el render original completo con sus cuatro controles secundarios. |
| 7 | Phase | Núcleo Pd y ADSR contrastados; NCO, salida, mezcla y asignación propios. 16 presets CORE. |
| 8 | Drum sampler | Kit propio de 24 pads con edición y bancos; falta el render/envolvente dinámica original. |
| 9 | D-box | Drum synth propio disponible; pendiente el original con cross-mod y controles secundarios. |
| 10 | LS, identificador interno | Referencia candidata; sin equivalencia comercial confirmada, port ni presets de fábrica encontrados en el DB. |
| 11 | DNA | 16 registros de fábrica inventariados; render original pendiente. No aparece en el diagrama de referencia del manual suministrado. |
| 12 | D-synth | Pendientes dos osciladores, múltiples envolventes y controles secundarios originales. |
| 13 | Voltage | Ruta candidata y 16 presets inventariados; render/filtros originales pendientes. |
| 14 | Iter, identificador interno | Referencia candidata; sin presets en este DB, pendiente. |

Las candidatas proceden del análisis estático y de la referencia pública op1-sdk
citada en créditos. Su localización no es prueba de equivalencia sonora. Los
núcleos Digital, Pd y ADSR tienen evidencia numérica específica; las referencias
a otras funciones no extienden esa evidencia al resto del aparato.

Cluster dispone además de la [inicialización compartida contrastada](cluster246-shared-init.md)
en 256 casos con fuentes explícitas y de 131.072 resultados de los helpers Q15/Q31.
La tabla original sigue incompleta; las entradas de prueba no sustituyen los tres
valores cifrados. REA apoya consultas de lectura/procedencia; la verificación de
instrucciones Blackfin de esta etapa usa GNU y Binutils.

## Efectos y filtros

| Objetivo | Estado |
| --- | --- |
| Delay | Delay propio; pendientes taps/interpolación/feedback/controles originales. |
| Grid | Pendiente la placa de feedback original. |
| Punch | Núcleo mono contrastado con 131.072 muestras, 2.048 estados y display. Integración opcional con conversión de tasa/DC propios. Cola potencialmente sostenida. |
| Phone | Pendientes procesamiento telefónico, buffer y estados originales. |
| Spring | Pendiente el reverb original y sus tablas/controles. |
| Nitro | Pendiente el doble filtro resonante original. |
| CWO | Presente en el firmware/DB; pendiente. No está en los seis diagramas de efectos del manual suministrado. |
| Filter, ruta interna | Función candidata localizada; pendiente reconstruir y contrastar. |
| Filtros internos de cada motor y decimación/salida | Pendientes individualmente. Los filtros de salida/adaptación actuales son propios. |
| EQ y master | Implementación propia; EQ, dinámica y ruteo originales pendientes. |

## Envolventes, modulación y ejecución

| Objetivo | Estado |
| --- | --- |
| ADSR común | Núcleo, tablas y callbacks contrastados; 2.621.440 muestras / 20.480 estados. |
| Cuatro campos ADSR adicionales y ruteo | Inventariados; falta reconstruir su efecto completo. |
| Envolventes D-synth / D-box / drum dinámica | Pendientes. No quedan completadas por portar el ADSR común. |
| Value LFO y destino parámetro/color | FX: 4.096 llamadas contrastadas. Voice: 4.096 llamadas instrument/primeros cuatro ADSR para Digital/Phase; estado independiente por voz. Otros motores, drum y auxiliares pendientes; reloj/allocator propios. |
| Random / Tremolo | Pendientes los originales. Hay LFO propio seno/triángulo/cuadrado. |
| Element / MIDI / Bend / Crank | Pendientes algoritmos y ruteo originales. Las fuentes físicas necesitarán una integración definida para un plugin. Recibir notas/CC normales no implementa estos LFO. |
| Retrigger / free | Callbacks Value FX y synth/ENV contrastados; integración por voz conserva fase al reutilizar, pausa voces inactivas. Resto de LFO/rutas y allocator originales pendientes. |
| Poly / mono / legato / unison / portamento | Polifonía propia de ocho voces. Modos y asignación originales pendientes. |
| Octavas del teclado | Interfaz ampliada a −4…+4, guardada en sesión/slot/clipboard, limitada a notas MIDI 0–127. No altera MIDI externo ni implementa aún octave de todos los patches originales. |
| Secuenciadores | Secuenciación/arpegios propios disponibles; Endless, Pattern, Finger, Tombola, Sketch y Crank originales requieren revisión individual. Tombola bloquea completar Singing Birds. |
| Cinta y mezcla | Cuatro pistas y edición propias; no se afirma equivalencia completa de longitud, varispeed, tape tricks, pan y dinámica originales. |

## Presets y sonidos del manual

El DB rev246 contiene **169 patches de synth**: diez categorías con 16 entradas
cada una y 9 de sampler. También contiene **24 kits**. El inventario guarda
nombres y campos numéricos necesarios, no audio ni el DB original:
[factory-preset-inventory.json](../research/blackfin/factory-preset-inventory.json).
La [comprobación REA](firmware/rea-factory-presets-summary.json) devolvió las 169
filas synth sin truncar y con el hash de la fuente inventariada.

Las recetas del manual son objetivos adicionales; no son una lista de 169 nombres
con parámetros raw. Sus posiciones descriptivas necesitan calibración. Se conserva
esa distinción en [manual-sound-targets.json](../research/blackfin/manual-sound-targets.json).

| Receta | Dependencias completadas | Pendiente para darla por recreada |
| --- | --- | --- |
| Helicopter, §16.3, PDF 51–52 | Digital, ADSR común, Punch y Value → azul; teclado +4. Preset de estudio adicional. | Calibración display/raw, cadencia/alineación original, ruteo/salida y comparación acústica. |
| Singing birds, §16.4, PDF 52 | Infraestructura MIDI/ADSR | FM original, Spring, Tremolo, mono/portamento, Tombola y calibración. |

Actualmente se ofrecen 25 sonidos Factory propios, 16 Phase CORE, 16 Digital CORE
y 3 variantes **246 CORE + Punch**, más 3 **246 Value FX Studies** y 5 **246 Value Voice Studies**. Los bancos importados son adicionales. Las
entradas anteriores conservan su comportamiento anterior; Punch no se
activa al abrir un proyecto previo; Value y Value Voice también se cargan apagados cuando faltan en el estado. Los registros pendientes no aparecen como
presets originales supuestamente reproducidos en el navegador.

## Orden de trabajo verificable

1. Cerrar el contrato de Cluster: normalización L1, inicialización y dependencias
   originales; integrar solo cuando estén verificadas.
2. Continuar los motores y sus filtros/rutas internas por dependencias, con tablas,
   muestras y estado completo contrastados antes de cada integración.
3. Completar envolventes auxiliares/específicas, moduladores y destinos pendientes,
   además de preparación/asignación de voz, modos de ejecución y salida originales.
4. Validar cada integración en el motor de audio: tasas, cambios de controles,
   note-off/pánico, sesiones/MIDI y trabajo del callback, junto con Windows/ASIO.
5. Presets, recetas de sonidos y comparación con vídeo en una etapa posterior.

El [plan DSP](firmware-dsp-workplan.md) define los criterios de cierre y registra
los bloqueos sin sustituir datos desconocidos por valores supuestamente originales.

Una coincidencia de oráculo bajo el modelo GNU documentado no certifica la máquina
completa, el reloj físico, su latencia ni ausencia de dropouts. El proyecto conserva
ambos tipos de evidencia por separado y mantiene sonidos/versiones anteriores.

La [demo de presets aportada](preset-listening-reference.md) ya dispone de un
[índice de 169 selecciones revisadas en 11 categorías](preset-video-index.md),
obtenido del lote definitivo de 21 segmentos. Se ha decodificado su audio y
alineado el vídeo mediante paquetes compartidos. Sigue pendiente la comparación
acústica con notas/FX/LFO documentados; identificar un nombre no completa un port.
Los lotes descartados a petición del usuario permanecen excluidos.

El [piloto espectral Sugar/Pure](preset-audio-pilot.md) mide dos ventanas bajo
hipótesis de nota explícitas y rechaza otra por polifonía. Se conservan diferencias
de hasta decenas de dB en determinados armónicos relativos; la grabación y las
rutas incompletas impiden atribuirlas a un algoritmo concreto. Se añade una
[tabla de dependencias de los 32 presets CORE](../research/blackfin/preset-audio-readiness.json).
No se han retocado motores, filtros ni envolventes a partir de este piloto.
