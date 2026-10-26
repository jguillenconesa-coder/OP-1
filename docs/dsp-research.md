# Investigación DSP — OP-1 revisión 246

Este registro continúa el [inventario con REA](firmware-analysis.md). No hay todavía un port de los motores originales ni una equivalencia sonora validada. La versión 0.7.0 distribuye los motores propios existentes.

## Avance de análisis nativo

Se compiló GNU Binutils 2.44 para `bfin-elf` en el área de investigación. El desensamblado Blackfin se hace con ese proveedor externo: no se afirma que REA haya descompilado los LDR ni que se disponga del código fuente original. Los binarios reconstruidos y el ensamblador del firmware permanecen fuera del repositorio.

El perfil de cargador observado usa cabeceras de 16 bytes, firma alta `0xAD`, XOR de cabecera cero y bloques con el bit `0x100` sin payload. Con esas reglas se recorren exactamente **801 bloques y los 2.171.624 bytes** de `OP1_vdk.ldr`. La interpretación completa de flags, fills, overlays y secuencia de arranque queda pendiente. El lector no ejecuta el firmware.

La imagen candidata de SDRAM combina payloads por orden de archivo y permite localizar referencias de código a nombres de unidades DSP:

| Evidencia estática | Observación delimitada |
| --- | --- |
| Cadena que menciona `Fm.cpp` en `0x018dc4f8` | La función candidata en `0x019a3d38` comprueba un índice contra 0 y 6; la rama de error carga esa dirección. En la ruta válida se observa un stride de 60 bytes sobre `0xffb00000` y una llamada a `0x019a21d8`. Es una pista de inicialización/estado por voz, no la función de render identificada. |
| Cadena que menciona `Pd.cpp` en `0x018f2de0` | La función candidata en `0x01a0bed8` contiene el mismo límite de seis índices, un stride de 28 bytes y una llamada a `0x01a0bc14`. La asociación con el motor Phase todavía debe corroborarse siguiendo el despacho del motor. |
| Código de estas vecindades | Se decodifican instrucciones Blackfin de llamadas, multiplicación, desplazamiento, operaciones empaquetadas y bucles hardware. Decodificar bytes de datos como instrucciones también produce resultados: una salida de objdump por sí sola no valida una función. |

Los límites observados sugieren seis voces en esas rutas del firmware; Pocket One conserva ocho. No se reduce su polifonía ni se cambian sus motores basándose únicamente en esa observación.

[Metadatos del mapa](firmware/op1-246-ldr-map.json). Las direcciones se refieren a la imagen candidata, no a una captura de ejecución. No se incluyen bytes de firmware ni listas de instrucciones en este registro.

## Reproducir y continuar

Con el LDR extraído previamente y Python 3:

```sh
python scripts/map-op1-ldr.py /ruta/OP1_vdk.ldr /ruta/nuevo-mapa.json
```

Para una investigación local se puede añadir `--private-image /ruta/nueva-imagen.bin`; contiene recursos/código del firmware y se mantiene fuera de paquetes publicados. El mapa advierte que los huecos contienen ceros de relleno del analizador y que no se interpretan todos los segmentos. No usar esa imagen para modificar o actualizar el aparato.

Con un `objdump` construido para Blackfin:

```sh
bfin-elf-objdump -D -b binary -m bfin --adjust-vma=0x1000000 /ruta/imagen.bin > /ruta/analisis-local.asm
```

La dirección base debe ser la `sdram_start` del informe, no una suposición para otros archivos. Este desensamblado es una ayuda de análisis estático; no prueba ejecución ni exactitud de los algoritmos.

Antes de implantar un motor equivalente quedan por resolver:

1. Separar inicialización, actualización de controles y bucles de audio siguiendo tablas de despacho y llamadas.
2. Determinar escalas, signo, saturación, redondeo, interpolación y rutas de operadores de FM/Phase.
3. Construir casos de prueba para cada operación y comparar una implementación independiente con resultados de referencia.
4. Contrastar audio, envolvente y espectro con la máquina o una ejecución validada del DSP antes de afirmar fidelidad.

Cambiar el FM actual por una fórmula supuesta y cargar los números de los presets no cumpliría esas verificaciones. Se mantienen los motores estables mientras la reconstrucción se investiga por separado.

## Continuación: estructuras y banco de referencia (0.6.0)

La ruta de inicialización FM en `0x019a21d8` llama a `0x019f15f8` con direcciones del objeto más 4, 12, 20 y 28. La ruta Pd `0x01a0bc14` usa offsets 4 y 12. La identidad matemática del subobjeto común no está validada; cuatro inicializaciones no bastan para afirmar «FM de cuatro operadores». Se han delimitado vecindades de procesamiento candidatas en `0x019a23d4` y `0x01a0bc5c`, con bucles hardware y acceso a tablas. El resto del análisis de despacho y aritmética permanece abierto.

Los 14 kits muestreados ofrecen una verificación independiente del formato de posiciones: `round(end * 44100 * 12 / 2^31)` corresponde a `frames - 1` en los 14 archivos RAW. Se interpreta END como inclusivo y se construyen 336 regiones de audio. Los nueve presets sampler aportan sus frecuencias raíz mediante `base_freq`. El banco de referencia privado conserva esas fuentes; los modos, pitch/gain por pad, envolventes y efectos originales no se convierten sin pruebas.

El nuevo modo CLEAN SAMPLE soluciona una diferencia propia del plugin detectada durante esa comparación: TONE controla el inicio del sampler, pero también limitaba su filtro global a 900 Hz con inicio cero. La ruta limpia permite comparar la fuente sin esa coloración ni saturación seca, y conserva la ruta antigua para proyectos anteriores. Esto mejora la reproducción de muestras; no demuestra equivalencia del DSP completo del OP-1.

El destino común de inicialización `0x019f15f8` se ha acotado a una rutina breve que escribe `0x80000000` en el campo de 32 bits del offset 0 y cero en el campo de 16 bits del offset 4. No establece por sí solo qué variable representa cada campo ni interpreta los bytes de padding; esa identificación sigue pendiente.


## Pruebas de ejecución aislada (0.7.0)

Se ha compilado el simulador Blackfin de **GNU GDB 13.1**, con ensamblador/enlazador
**Binutils 2.44**. Se ejecutan subrutinas aisladas con memoria y entradas controladas;
no se ha arrancado el sistema del OP-1. La reconstrucción independiente vive en
`Source/ReferenceDsp.h`, separada del motor de producción, y tiene pruebas CTest
con [vectores reproducibles](../Tests/ReferenceVectors/README.md).

| Comprobación | Resultado acotado |
| --- | --- |
| Prefijo FM `0x019a23d4` hasta `0x019a2452` | 65.536 casos, cero diferencias. Tres índices `(word >> 12/9/6) & 7`, selector con signo desplazado 11 bits y offsets de tabla 12/28/44 más dos veces cada índice. Las dos palabras recorren juntas 0…65535; no se prueban todas sus combinaciones. |
| Multiplicación en `0x01a0bd1a` | 65.536 pares Q15, cero diferencias, saturación y redondeo al par del simulador. Otras configuraciones de redondeo y hardware físico siguen sin verificar. |
| Render Pd `0x01a0bc5c` hasta `0x01a0be82` | 1.024 bloques de 128 muestras, cero diferencias para una rama estable: estado secundario inactivo, sin wrap firmado, controles restantes a cero, incrementos constantes y LUT sintética suministrada. No corresponde al estado de reset normal ni valida el motor completo. |
| Inicialización `0x019f15f8` | Confirma los campos 32/16 bits descritos antes; los dos bytes de padding no se sobrescriben. |

La vtable de FM en `0x0190ff84` apunta a la rutina de render indicada; la de Pd
está en `0x0191113c`. El selector FM tiene nueve destinos explícitos y una ruta
alternativa que aún debe estudiarse. Los campos decodificados todavía no se
interpretan como ratios musicales ni como nueve algoritmos equivalentes.

Se ha localizado una copia de tabla en `0x01a49b1c`: toma las mitades bajas de
8.193 palabras de 32 bits desde `0x018b6688` y las escribe como 16 bits en
`0x0101e5e8`. El mapa genérico omitía seis bloques de relleno, con argumentos `0x7ffe`,
`0x7fff`, `0x8002` y `0x8001`. Al interpretarlos como palabras little-endian
repetidas, las 8.193 entradas coinciden exactamente con
`round(sin(i * pi / 4096) * 32767)`. La tabla matemática independiente y sus hashes
quedan registrados; no es necesario distribuir la imagen original.

La ejecución de la copia original en GNU GDB 13.1 presenta un desplazamiento de
una entrada y repite el cero final. Se ha localizado la diferencia en el grupo
paralelo que carga R0 y almacena R0.L: el simulador actualiza la carga antes del
almacenamiento. Se registra la discrepancia aparente de temporización como
**pendiente de verificar contra hardware/semántica del procesador**. La tabla
fuente validada y la salida desplazada del simulador se guardan por separado;
las pruebas del núcleo cargan explícitamente la primera.

Los bytes de código y el desensamblado permanecen en el área privada de análisis.
El repositorio añade únicamente aritmética independiente, harnesses propios,
entradas sintéticas, salidas numéricas y metadatos. Los tests de coincidencia con
el simulador no se presentan como equivalencia sonora con el aparato.


### Ampliación: rama secundaria, reinicios y cruces de ciclo

`ReferenceDsp.h` incorpora una reconstrucción funcional independiente del núcleo
Pd, incluida su rama secundaria: Q31 con saturación/truncamiento, contorno,
reinicio, sumas de fase modulares y contador de ciclos. Los cuatro controles son
palabras signed16 crudas; sus nombres musicales y conversión desde el preset no
se dan por establecidos. La tabla se recibe del llamante y permanece explícita.

El nuevo harness llama al render original y, en 2.730 casos, también al reset
original `0x01a0be82`. Compara **1.048.576 muestras y los cuatro campos finales en
8.192 bloques**, sin diferencias con el simulador. Incluye las 4.096 combinaciones
de ocho extremos para los cuatro controles, otras 4.096 entradas deterministas,
siete incrementos de fase (incluyendo cero, uno e INT32_MAX), fases aleatorias y
estados secundarios activos/inactivos. Se ejercitan 130.485 cruces de ciclo
principal y 77.472 secundarios en la reconstrucción.

La coincidencia cubre estas condiciones y el modelo GNU, con la tabla fuente
cargada de forma explícita. Faltan variaciones de incremento dentro del bloque,
el proveedor de frecuencia/controles, presets, efectos, render de voz completo y
comparación con el aparato. El hallazgo sobre la copia paralela refuerza ese
límite: los vectores de un simulador no bastan para certificar el DSP físico.

Los 25 presets propios y el banco de 79 presets del usuario se conservan como
material adicional. Las 23 referencias secas del firmware mantienen el banco
separado **OP-1 firmware reference (local)**. Los futuros patches FM/Pd tendrán
una categoría propia y no sustituirán estos bancos antes de validar su conversión.
