# Audio en tiempo real y latencia

El procesador informa **0 muestras de latencia añadida**. No utiliza look-ahead, remuestreo por bloques ni un buffer de salida adicional. El ataque de la envolvente y el delay elegido son parte del sonido, no latencia comunicada al host. El MIDI del DAW se procesa en su offset de muestra; las notas de la interfaz entran al comienzo del siguiente bloque.

## Cambios en 0.2.2

- Se sustituyó la sincronización de MidiKeyboardState dentro del callback por dos colas SPSC de tamaño fijo y atómicos sin bloqueo. El teclado visual se actualiza en el hilo de interfaz.
- Los punteros a parámetros se resuelven al construir el procesador; el callback solo lee sus atómicos.
- Se eliminó el borrado de buffers de cuerda de 32 KB por nota en motores que no los usan. La cuerda se inicializa únicamente cuando la necesita ese motor.
- Los coeficientes ADSR, la frecuencia raíz del sampler y el filtro se reutilizan cuando no cambian. Los factores de pitch se comparten entre voces en vez de repetir potencias por voz y muestra.
- La cinta solo calcula módulo al cruzar su límite, no en cada muestra.
- Las muestras se decodifican fuera del callback y se intercambian por swap; la liberación del audio antiguo ocurre después de soltar el bloqueo de intercambio.

## Prueba reproducible

`realtime_tests` forma parte de CTest. Mide 42 combinaciones: seis motores más Phase 246 CORE nativo, 48/96 kHz y bloques de 64/128/256 muestras, con ocho voces, nuevas notas/bend, teclado GUI, cinta de cuatro pistas con overdub y FX/LFO. Comprueba audio finito/no silencioso, latencia declarada y ausencia de asignaciones/liberaciones C++ `new/delete` durante los bloques. En Linux también intercepta `malloc/calloc/realloc/free` enlazados desde el código del ejecutable. No es un interceptor universal de cada posible allocator del sistema.

Los tiempos mediana, p99, máximo y bloques que exceden su presupuesto se guardan en `build/realtime-metrics.json`. Las métricas se adjuntan a los artefactos CI. Las aserciones funcionales y de heap sí fallan el test; los tiempos se reportan como diagnóstico, porque el planificador de una máquina compartida puede interrumpir el proceso.

Las pruebas bajo Wine ya pasaron sin llamadas de heap monitorizadas. En una medición hubo un bloque por encima de su plazo, aunque el p99 de cada configuración quedó por debajo del 12% del presupuesto. Ese resultado **no demuestra ausencia de dropouts** ni mide el controlador, Ableton o la latencia de ida y vuelta.

## Configuración práctica

En Ableton/Windows usa el controlador ASIO del fabricante, 48 kHz y 128 muestras como punto de partida. Si hay cortes, prueba 256; 64 exige más al controlador y al sistema. A 48 kHz, esos buffers representan 1,33 / 2,67 / 5,33 ms por bloque, respectivamente; no son cifras de latencia total.

El VST3 utiliza el dispositivo configurado en Live. El standalone incluye WASAPI; este paquete no incorpora el SDK ASIO ni promete ASIO dentro de la aplicación independiente.

Carga bancos, cambia muestras grandes y guarda/abre sesiones con el transporte parado. Las instantáneas de muestra/cinta aún usan el bloqueo de callback para obtener datos coherentes; copiar audio mientras se reproduce puede bloquear brevemente el procesamiento. No se afirma que guardar, cargar o importar sea libre de cortes. La validación final de dropouts necesita el equipo, interfaz y proyecto real del usuario.

## Medición Linux nativa de esta actualización

La prueba Release completó 18.432 bloques en las 36 cargas, sin llamadas de heap monitorizadas y sin bloques por encima de su plazo en esa ejecución. El peor p99 fue el 8,41% del presupuesto. A 48 kHz, el peor p99 entre motores fue 112,03 µs con 64 muestras (presupuesto 1.333,33 µs), 118,76 µs con 128 (2.666,67 µs) y 207,93 µs con 256 (5.333,33 µs). Los datos completos están en [realtime-metrics-linux.json](realtime-metrics-linux.json). Son resultados de una máquina virtual compartida, no una garantía para Windows, el controlador o Ableton.

## Medición 0.3.0 con controles nuevos

La prueba incluye ahora captura de entrada, EQ/master FX, metronomo y SysEx además de la carga anterior. Se procesaron 18.432 bloques sin llamadas de heap monitorizadas; el peor p99 consumió el 7.61% del presupuesto. Se observaron 1 bloques fuera de plazo en esta ejecución compartida. Son interrupciones/tardanzas observadas, no se presentan como una garantía de cero dropouts ni como una medida dentro de Ableton. Los [datos completos actuales](realtime-metrics-linux.json) identifican 0.3.0; las cifras anteriores corresponden a 0.2.2.

Las divisiones de tomas usan almacenamiento reservado antes del procesamiento, incluido el marcado de inicio/final de grabación. Las ediciones destructivas y el undo copian audio fuera del callback, con la cinta parada. Se ignoran SysEx no soportados sin construir mensajes largos dentro del callback.

## Secuenciadores de 0.4.0

El motor de pasos/arpegiador usa almacenamiento fijo para las teclas de 16 canales y los candidatos de hasta tres octavas. El recorrido pseudoaleatorio se deriva del tick del host, sin RNG compartido ni asignaciones. Gate y swing se calculan por muestra. Las 36 cargas de pruebas recorren los siete modos e incluyen la máscara euclídea; no se observaron llamadas de heap monitorizadas. La ejecución Linux registrada tuvo dos overruns y un peor p99 del 20,47 % del presupuesto en la VM compartida: [medidas](realtime-metrics-linux.json). No sustituye una medición de round-trip ASIO en Live.

## Kits de 24 pads de 0.5.0

Las 36 cargas actuales añaden ajustes por pad y notas hasta MIDI 83. En los 18.432 callbacks registrados no hubo llamadas de heap monitorizadas; el peor p99 usó 13.91 % del presupuesto y hubo 0 callbacks por encima del plazo en la VM compartida. La latencia declarada sigue en cero muestras. La medición no sustituye una prueba de Live/ASIO en el equipo del usuario.


## 0.7.0

Los cuatro parámetros contextuales reutilizan los destinos existentes; la purga/importación hace E/S en el hilo de interfaz y solo intercambia buffers bajo el bloqueo breve del callback. La medición sintética final cubre 18.432 callbacks: cero llamadas de heap monitorizadas, latencia declarada de cero muestras, cuatro overruns observados y peor p99 de 21,21 % del presupuesto en esta VM compartida. [Datos de esta versión](realtime-metrics-linux.json). No certifica un controlador ASIO o una sesión de Live reales. El núcleo Pd experimental permanece fuera del audio de producción.


## Phase 246 CORE de 0.8.0

El modo opcional calcula el núcleo Pd a 88,2 kHz internos fijos por voz y adapta
al host con interpolación y un filtro causal propio. No reserva memoria ni
prepara tablas/coeficientes de filtro dentro del callback: se preparan en
`prepareToPlay`. Los coeficientes de controles sí se actualizan durante el audio
con suavizado propio de 12 ms. El filtro añade desfase/retardo de grupo dependiente
de frecuencia; no hay look-ahead ni nueva latencia declarada al host.

Las 42 cargas añaden ocho voces de Phase 246, automatización de sus cuatro
controles y pitch bend al conjunto existente. La ejecución Linux final completó
**21.504 callbacks**, con **0 llamadas de heap monitorizadas**, **4 callbacks
fuera de plazo** y un peor p99 del **22.42 %** del plazo. Para Phase 246,
el peor p99 fue **22.42 %**, con **3** tardanzas. [Datos Linux](realtime-metrics-linux.json).
La medición previa después de compilar tuvo seis tardanzas y un peor p99 del
31,09 % (Phase 17,07 %); se conserva [también su registro](firmware/phase246-linux-initial-metrics.json).
Una prueba inicial mientras se compilaba Windows mostró más tardanzas y no se
interpreta como una medición aislada del motor.

Windows bajo Wine completó los mismos **21.504 callbacks** sin llamadas de heap
monitorizadas: **4** tardanzas, peor p99 global **43.74 %** y Phase
**43.74 %**. [Datos Wine](realtime-metrics-windows-wine.json).
La variación entre ejecuciones refleja que se trata de una máquina compartida.
Estas pruebas no certifican cero dropouts ni miden ASIO/Live 10.2.4 en Windows real.


## 0.9.0: envolvente reconstruida, String y pánico

El benchmark añade ambas envolventes de Phase: **48 cargas / 24.576 callbacks**
por plataforma, con automatización de los cuatro controles de oscilador y ADSR.
No se observaron llamadas de heap monitorizadas en el callback. El resto de la
carga incluye ocho voces, MIDI/GUI/SysEx, captura, cinta, FX/EQ/LFO y secuenciadores.

| Medición final sin compilación simultánea | Linux | Windows bajo Wine |
| --- | --- | --- |
| Peor p99 como % del plazo | 14.20 % | 25.25 % |
| Phase con envolvente rev246: peor p99 | 9.53 % | 23.49 % |
| Callbacks fuera de plazo observados | 4 | 7 |
| Llamadas de heap monitorizadas | 0 | 0 |
| Latencia declarada | 0 muestras | 0 muestras |

[Datos Linux](realtime-metrics-linux.json) · [Datos Wine](realtime-metrics-windows-wine.json).
Las tardanzas observadas se conservan: estas métricas de VM no certifican cero
dropouts, latencia de ida/vuelta ni funcionamiento en Ableton/ASIO Windows real.
No se ha validado aquí el arranque gráfico completo del standalone Windows.

El ADSR avanza por muestra al reloj interno, con coeficientes retenidos en tramos
de 128 muestras del núcleo. El adaptador sintetiza los puntos que necesita el
interpolador; no espera un bloque de audio de entrada. Las notas mantienen sus
offsets MIDI y el audio de la prueba coincide al procesar bloques de 64/128/256.
Los filtros tienen su retardo de grupo. La inserción explícita de FILTER LFO se
suaviza en 12 ms; el retune de String, en unos 3 ms con límite de movimiento.

Los dos delays invalidan su historial con contadores al reiniciar, sin borrar
sus buffers grandes dentro del callback. STOP/CC120 limpian también EQ/historial
master; los cambios normales de preset conservan ese historial. Guardar/importar
sigue teniendo las restricciones de bloqueo descritas anteriormente.

Las colas de la envolvente reconstruida pueden durar decenas de segundos. El host
recibe una estimación conservadora de su release y delays en cascada, en lugar de
usar únicamente el tiempo de la envolvente anterior. Esto no añade latencia de entrada.


## 0.10.0: Digital y dos envolventes

El benchmark incluye ahora los seis motores anteriores, Phase CORE con ambas
envolventes y Digital CORE con ambas: **60 cargas / 30.720 callbacks por
plataforma**, ocho voces y automatización de los cuatro controles raw y ADSR.
Se midieron 48/96 kHz y bloques 64/128/256 sin compilación simultánea.
No se observaron llamadas de heap monitorizadas. No se añade buffering por bloque
al oscilador; notas/bend/pedal/CC120 conservan los offsets y la prueba entrega
audio idéntico con las tres particiones del host.

| Medición final | Linux | Windows bajo Wine |
| --- | --- | --- |
| Peor p99 como % del plazo | 21.71 % | 31.98 % |
| Digital con envolvente rev246: peor p99 | 16.16 % | 29.23 % |
| Callbacks fuera de plazo observados | 2 | 1 |
| Llamadas de heap monitorizadas | 0 | 0 |
| Latencia declarada | 0 muestras | 0 muestras |

[Datos Linux 0.10.0](releases/v0.10.0-linux-realtime.json) ·
[Datos Wine 0.10.0](releases/v0.10.0-windows-wine-realtime.json).
Las tardanzas se conservan en el informe: no se certifican cero dropouts ni se
mide la latencia de ida/vuelta ASIO. El usuario ha confirmado que el VST3 anterior
funciona en Ableton en lo probado, sin especificar versión/buffer; esta nueva
entrega requiere su propia comprobación en Windows real.


## 0.11.0: Punch y rutas combinadas

**72 cargas / 36.864 callbacks por plataforma**, con Digital/Phase + envolvente
rev246 + Punch, automatización de los cuatro controles FX y las cargas anteriores.
Se midieron 48/96 kHz y bloques 64/128/256 sin compilación simultánea. No se
observaron llamadas de heap monitorizadas. El test de audio mantiene exactamente
los offsets MIDI/pedal/bend y el silencio post-CC120 con las tres particiones.

| Medición final | Linux | Windows bajo Wine |
| --- | --- | --- |
| Peor p99 como % del plazo | 14.50 % | 28.67 % |
| Rutas con Punch: peor p99 | 14.50 % | 25.68 % |
| Callbacks fuera de plazo observados | 2 | 7 |
| Llamadas de heap monitorizadas | 0 | 0 |
| Latencia declarada | 0 muestras | 0 muestras |

[Datos Linux 0.11.0](releases/v0.11.0-linux-realtime.json) ·
[Datos Wine 0.11.0](releases/v0.11.0-windows-wine-realtime.json).
Estas cifras no certifican cero dropouts ni latencia física cero. Punch usa
procesamiento por muestra con coeficientes/feedback retenidos en bloques internos
de 64 muestras, sin acumular un bloque del host. A tasas distintas de 44,1 kHz
la interpolación causal y los filtros tienen retardo de grupo. Se añaden DC a
5 Hz, limitación de salida y un fade de inserción de 12 ms, todos propios.

El feedback original puede sostener oscilación: Punch activo declara una cola
potencialmente infinita, compatible con el contrato VST3 de JUCE. STOP/CC120
reinician el estado y la integración queda en silencio hasta nuevo audio. No se
recorta artificialmente esa cola a una release finita. El modelo/kernel original
y la integración propia se distinguen en [la guía](punch246-native.md).

## Medición 0.12.0: Value FX

Se ejecutaron **84 cargas / 43.008 callbacks por plataforma**, a 48/96 kHz y
64/128/256 muestras, con ocho voces, automatización de Value/Punch/osciladores/ADSR,
MIDI, secuenciación, cinta y efectos. Las mediciones finales Linux y Wine fueron
secuenciales, sin compilar ni ejecutar otras pruebas del proyecto en paralelo.
No hubo llamadas de heap monitorizadas en los callbacks; latencia declarada: 0.

| Entorno | Peor p99 / presupuesto | Peor p99 con Value | Callbacks por encima del plazo |
| --- | --- | --- | --- |
| Linux, VM compartida | 27,88% | 23,16% | 5 |
| Windows x64 bajo Wine, misma VM | 41,54% | 41,54% | 5 |

Los sobrepasos quedan registrados; no se afirma ausencia de dropouts. Tampoco se
mide aquí Ableton/ASIO ni latencia de ida y vuelta. Value utiliza un reloj propio
de control 44.100/64, independiente del bloque del host, sin añadir un bloque de
audio de espera. Se verifican notas con offset, pánico y audio idéntico al cambiar
el bloque 64/128/256. El reloj/alineación de la máquina física siguen pendientes.

[Datos Linux 0.12.0](releases/v0.12.0-linux-realtime.json) ·
[Datos Wine 0.12.0](releases/v0.12.0-windows-wine-realtime.json) ·
[Evidencia completa](releases/v0.12.0-validation.json).
