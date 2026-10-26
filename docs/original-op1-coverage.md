# Objetivo: OP-1 original completo

La meta es cubrir motores, filtros, efectos, envolventes, modulación, modos de
ejecución y sonidos documentados. La versión 0.12.0 **todavía no es una réplica
completa**. Esta matriz distingue un núcleo contrastado de una aproximación
propia o una función pendiente; cambiar el nombre de un modelo no completa un port.

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
| 4 | Cluster | Ruta candidata localizada; osciladores/mezcla original pendientes. |
| 5 | Pulse | Pendiente aislar y reconstruir el motor de doble tren de pulsos. |
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
| Value LFO y destino parámetro/color | Ruta FX contrastada: 4.096 estados/display/buffers completos; cuatro colores de Punch disponibles. Destinos de voz/ENV pendientes; reloj del adaptador propio. |
| Random / Tremolo | Pendientes los originales. Hay LFO propio seno/triángulo/cuadrado. |
| Element / MIDI / Bend / Crank | Pendientes algoritmos y ruteo originales. Las fuentes físicas necesitarán una integración definida para un plugin. Recibir notas/CC normales no implementa estos LFO. |
| Retrigger / free | Callback de fase Value FX contrastado e integrado. Pendiente para el resto de LFO/rutas. |
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
y 3 variantes **246 CORE + Punch**, más 3 **246 Value FX Studies**. Los bancos importados son adicionales. Las
entradas anteriores conservan su comportamiento anterior; Punch no se
activa al abrir un proyecto previo; Value también se carga apagado. Los registros pendientes no aparecen como
presets originales supuestamente reproducidos en el navegador.

## Orden de trabajo verificable

1. Calibrar display/raw y cadencia/ruteo de Helicopter; contrastar con audio real.
   Extender Value a voz/ENV y comprobar los demás LFO.
2. Avanzar FM/DrWave/Cluster y filtros internos; mantener explícito el bloqueo de
   datos L1 donde exista. Completar modos de ejecución y controles secundarios.
3. Spring/Tremolo/Tombola para Singing Birds y los demás FX/motores en la matriz.
4. Aplicar cada patch de fábrica al disponer de todos sus componentes; contrastar
   niveles, envolventes, modulación y mezcla, además del núcleo aislado.
5. Comparación acústica con salidas reales del aparato y validación Windows/ASIO.

Una coincidencia de oráculo bajo el modelo GNU documentado no certifica la máquina
completa, el reloj físico, su latencia ni ausencia de dropouts. El proyecto conserva
ambos tipos de evidencia por separado y mantiene sonidos/versiones anteriores.
