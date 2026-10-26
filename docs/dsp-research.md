# Investigación DSP — OP-1 revisión 246

Este registro continúa el [inventario con REA](firmware-analysis.md). La versión 0.8.0 conecta una reconstrucción independiente del núcleo Pd al modo opcional [Phase 246 CORE](phase246-native.md). No hay una equivalencia sonora validada del instrumento completo. Las secciones siguientes conservan la cronología y las limitaciones de cada etapa.

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
cargada de forma explícita. La ampliación siguiente comprueba incrementos variables dentro del bloque.
Siguen pendientes el proveedor de frecuencia/controles, presets completos,
efectos, render de voz completo y
comparación con el aparato. El hallazgo sobre la copia paralela refuerza ese
límite: los vectores de un simulador no bastan para certificar el DSP físico.

Los 25 presets propios y el banco de 79 presets del usuario se conservan como
material adicional. Las 23 referencias secas del firmware mantienen el banco
separado **OP-1 firmware reference (local)**. Los futuros patches FM/Pd tendrán
una categoría propia y no sustituirán estos bancos antes de validar su conversión.


## Continuación: streams y banco de estudios Phase

Se añaden **294.912 muestras y 2.304 estados finales** sin diferencias frente al
núcleo original simulado, con estado conservado entre bloques e incrementos que
cambian por muestra. Son 144 secuencias: 16 conjuntos de controles × tres rates
(44,1/48/96 kHz) × nota fija/glide/vibrato. Cada secuencia llama al reset original
y después procesa 16 bloques consecutivos. El total acumulado de los tres juegos
Pd es **1.474.560 muestras**. No se incluyen dos veces los casos del renderizador,
que reutilizan las secuencias de nota fija.

REA 6.8.0, con su proveedor SQLite, vuelve a inspeccionar `synth_presets` del
firmware: 169 filas, muestra completa sin truncamiento. Las 16 entradas Phase y
sus cuatro palabras de control coinciden con el lector independiente.
[Metadatos de esa verificación](firmware/phase-controls-rea-verification.json).
Leer esos campos no demuestra su transformación antes del render; se mantienen
identificados como **entradas crudas de investigación**.

El nuevo programa offline `pd_reference_render` calcula PCM con nuestra
reconstrucción y una convención NCO MIDI externa. El ejecutable coincide con
98.304 muestras originales simuladas a tres rates, tanto en Linux como en
Windows bajo Wine. Permite producir un [banco adicional de 16 estudios Phase](phase-core-studies.md),
con 13 WAV únicos, sin sustituir presets previos ni activar este núcleo dentro
del motor de producción. Los originales con iguales controles pueden diferir
por ADSR/FX/LFO; aquí esas funciones quedan fuera y se declara su omisión.

La ruta de frecuencia/control sigue abierta. Se ha delimitado además el payload
L1 en `0xffa00000`, de 49.076 bytes, con flags bajos `0x2406`. Su desensamblado
crudo contiene numerosos opcodes inválidos: no se asume que el mapa de payload
sea una imagen de ejecución válida, ni se etiqueta como descifrado o
inicialización completa. Antes de usar esa región para identificar funciones
hay que establecer las transformaciones del cargador y el significado de los
flags. La imagen/desensamblado permanecen fuera del repositorio.

El banco se prueba importando y reproduciendo todas sus entradas en el plugin,
y restaurando el sonido activo en otra instancia sin sus archivos instalados.
Los 16 comienzos de WAV coinciden con los oráculos de nota fija; solo se añade
un cierre de 10 ms al final. Dos ejecuciones del generador
dan un ZIP con bytes idénticos con el mismo renderizador.
[Validación y límites](firmware/phase-core-studies-validation.json).


## 0.8.0: rangos originales, reloj y modo nativo

El nuevo harness llama al inicializador `0x01a1e914` sobre SDRAM, sin arrancar el
sistema. En `0x01de5890 + 7*64` produce cuatro entradas `min/max/scale/kind`:
`0/32767/32767/0`, `0/29491/32767/0`, `0/32767/32767/0`, `0/32767/32767/0`.
Son límites/formatos del motor tipo 7 según el perfil rev246 contrastado; no
prueban que la ruta original entregue directamente las palabras JSON al render.

La llamada aislada a `0x01a51a08`, setter de frecuencia del sampler, coincide con
`trunc(float32(88200 / float32(float32(hz) * 16384)) * 2^31)` para siete entradas.
Se observa el literal 88.200, las operaciones y la conversión fraccional en esa
ruta. La documentación comunitaria de **op1-sdk**, revisión
`ae1945a9c61468f0e0d7cfdd5dd60c2cf9731ff1`, describe voces de 128 muestras por
64 frames de salida a 44,1 kHz y el buffer de incrementos en `0xff801950`.
Eso guía nuestro reloj fijo a 88,2 kHz; todavía no sustituye la validación de
la preparación completa de pitch ni una captura del hardware.

Se añaden 98.304 comparaciones Pd a ese reloj con 16 juegos de controles,
trayectorias constantes/glide/vibrato y 768 estados finales, sin diferencias
con el simulador. El total acumulado de muestras de audio Pd es 1.572.864.
La nueva integración conserva el núcleo probado y añade NCO, filtro y adaptador
propios. [Detalle práctico y límites](phase246-native.md).

[Metadatos reproducidos](firmware/phase246-voice-contract.json).

```sh
python scripts/verify-phase-contract.py \
  --ldr /ruta/OP1_vdk.ldr --assembler /ruta/bfin-elf-as \
  --linker /ruta/bfin-elf-ld --simulator /ruta/sim/bfin/run \
  --testutils /ruta/gdb-13.1/sim/testsuite/bfin \
  --output /ruta/nueva-carpeta-privada-fuera-del-repositorio
```

La imagen con fills, ELF y código original quedan exclusivamente en esa carpeta
privada. El repositorio contiene nuestros harnesses y resultados numéricos.

### Cargador y contenido L1

Las cabeceras públicas de U-Boot/Blackfin identifican `0x400` como CALLBACK y
`0x2000` como INDIRECT; el `0x2400` observado deja de ser un flag desconocido.
La referencia pública **sualk/op1-decryptor**, revisión
`20f86baa116b4c7b1da7e0d4e3a24f171c76767b`, describe XTEA con una clave externa:
los primeros ocho bytes de cada grupo de 24 en los bloques CALLBACK. Se consultó
como referencia, sin copiar su código, que carece de licencia declarada.
No se dispone aquí de la clave ni de una imagen L1 descifrada validada.

También se inspeccionó `te-boot.ldr`: contiene una rutina XTEA, pero su llamada
observada pertenece a una ruta de datos de licencia/clave. No se ha probado que
proporcione la clave de los bloques de aplicación. No se ejecuta el arranque ni
se genera firmware para flashear. FM, las tablas de coeficientes en L1 y el
controlador original siguen pendientes; no se interpretan bytes cifrados como
coeficientes musicales.

Referencias externas consultadas (no dependencias del producto):
- [op1-sdk / internals](https://github.com/BertBrandstifter/op1-sdk/blob/ae1945a9c61468f0e0d7cfdd5dd60c2cf9731ff1/docs/internals.md), MIT.
- [bootrom.h de U-Boot v2013.10](https://github.com/u-boot/u-boot/blob/v2013.10/arch/blackfin/include/asm/mach-common/bits/bootrom.h).
- [op1-decryptor](https://github.com/sualk/op1-decryptor/tree/20f86baa116b4c7b1da7e0d4e3a24f171c76767b), solo referencia de formato.


## 0.9.0: envolvente reconstruida y modelo de instrucciones paralelas

La ruta ADSR SDRAM, sus conversiones de tablas y callbacks se han aislado.
**2.621.440 muestras y 20.480 estados completos** coinciden con nuestra
reconstrucción bajo el modelo GNU con escrituras D-reg diferidas. Se conserva
el resultado stock que muestra los hazards de registros; no se incorpora esa
salida al instrumento. Cinco probes independientes documentan el cambio, y los
probes anteriores FM/Q15/Pd conservan sus resultados. La copia LUT coincide
ahora con la fuente sin desplazar en ese modelo; la validación de hardware sigue
pendiente. [Método, archivos y límites](envelope246-research.md).

La integración añade una envolvente opcional al banco Phase, conserva el modo
0.8.0 en proyectos/slots anteriores y expone los cuatro controles por el set 2.
Normalización, mezcla, velocidad y flujo de dither siguen siendo propios.
[Uso](phase246-native.md). String se corrige como modelo propio, sin presentarlo
como un port original. [Estado de motores y funciones](audio-engines.md).


## Digital aislado e integración (0.10.0)

La función Digital, constructor y reset ya tienen una reconstrucción independiente
contrastada con 262.144 muestras y 2.048 estados completos. Usa la tabla de seno
verificada y el mismo modelo GNU documentado. Se integra como opción junto al
FM propio con 16 presets adicionales, sin convertir FX/LFO/octave originales.
[Contrato, oráculos y reproducción](digital246-research.md). Punch se investigó
como candidato, pero todavía no se ha portado ni validado su ruta completa.


## Punch y alcance completo (0.11.0)

El procesador mono 0x019e6260 y reset 0x019e6160 están reconstruidos de forma
independiente. Los oráculos comparan 131.072 muestras, 2.048 estados completos
y valores de display, cuatro rangos y 129 conversiones de coeficientes. Una
segunda reproducción privada coincide con ambos fixtures. La integración
causal/DC es propia y el reloj FX de 44,1 kHz sigue siendo una hipótesis explícita.
[Contrato y uso](punch246-native.md).

REA inspeccionó de nuevo el DB original en lectura sin truncamiento: 169 patches
synth. El inventario propio incluye además 24 kits. Los ejercicios Helicopter y
Singing Birds del manual se registran con sus dependencias; no se les asigna
estado de réplica completa. [Matriz de motores/FX/LFO/envolventes y recetas](original-op1-coverage.md).
