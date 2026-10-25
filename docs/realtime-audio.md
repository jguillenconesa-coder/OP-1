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

`realtime_tests` forma parte de CTest. Mide 36 combinaciones: seis motores, 48/96 kHz y bloques de 64/128/256 muestras, con ocho voces, nuevas notas/bend, teclado GUI, cinta de cuatro pistas con overdub y FX/LFO. Comprueba audio finito/no silencioso, latencia declarada y ausencia de asignaciones/liberaciones C++ `new/delete` durante los bloques. En Linux también intercepta `malloc/calloc/realloc/free` enlazados desde el código del ejecutable. No es un interceptor universal de cada posible allocator del sistema.

Los tiempos mediana, p99, máximo y bloques que exceden su presupuesto se guardan en `build/realtime-metrics.json`. Las métricas se adjuntan a los artefactos CI. Las aserciones funcionales y de heap sí fallan el test; los tiempos se reportan como diagnóstico, porque el planificador de una máquina compartida puede interrumpir el proceso.

Las pruebas bajo Wine ya pasaron sin llamadas de heap monitorizadas. En una medición hubo un bloque por encima de su plazo, aunque el p99 de cada configuración quedó por debajo del 12% del presupuesto. Ese resultado **no demuestra ausencia de dropouts** ni mide el controlador, Ableton o la latencia de ida y vuelta.

## Configuración práctica

En Ableton/Windows usa el controlador ASIO del fabricante, 48 kHz y 128 muestras como punto de partida. Si hay cortes, prueba 256; 64 exige más al controlador y al sistema. A 48 kHz, esos buffers representan 1,33 / 2,67 / 5,33 ms por bloque, respectivamente; no son cifras de latencia total.

El VST3 utiliza el dispositivo configurado en Live. El standalone incluye WASAPI; este paquete no incorpora el SDK ASIO ni promete ASIO dentro de la aplicación independiente.

Carga bancos, cambia muestras grandes y guarda/abre sesiones con el transporte parado. Las instantáneas de muestra/cinta aún usan el bloqueo de callback para obtener datos coherentes; copiar audio mientras se reproduce puede bloquear brevemente el procesamiento. No se afirma que guardar, cargar o importar sea libre de cortes. La validación final de dropouts necesita el equipo, interfaz y proyecto real del usuario.
