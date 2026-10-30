# Rev246: despacho de retirada de instrumento con mapa vacío

## Alcance y entradas

Se contrasta la rutina SDRAM completa **`0x01a85840..0x01a858d6`**: 150 bytes,
fin exclusivo en la siguiente entrada, que empieza con una instrucción antes
de su prólogo. La rama probada requiere la palabra **`0x01de4c40 = 0`**.
Las ramas de discriminador 1/2 y los errores por otros valores quedan fuera.
No se atribuye identidad comercial ni un dispatcher MIDI completo.

R0 apunta al argumento de **16 bytes** ya existente. Se suministra explícitamente;
no se ejecuta el constructor en este ensayo. El primer filtro lee su palabra
de nota **signed32** y solo admite **0–127**. Los demás patrones, incluidos
negativos y palabras altas con byte/halfword bajo aparentemente válido, retornan
sin copiar el argumento ni consultar el discriminador, lista o mapa.

Con nota válida y discriminador cero, prepara un argumento local en la pila:

| Offset | Escritura local |
| --- | --- |
| 0 | Palabra completa de nota. |
| 4 | Palabra completa de velocidad. |
| 8 | Un byte de metadata; se conserva su patrón 0–255. |
| 9–11 | No se copian; conservan el contenido anterior del destino local. |
| 12 | Palabra completa de pitch explícito. |

Llama a la retirada **`0x01a85058`** con ese argumento y después comprueba el
byte 8 del **argumento fuente**. Si vale cero, retorna; si es no nulo, llama a
`0x019e2dc0` con la nota fuente. No borra el byte 1 que puede haber escrito el
constructor. Fuente, relleno y vecinos se capturan completos y se conservan.
No se asigna un valor de retorno semántico a R0 en este dispatcher.

## Dependencia de notificación: condición verificada

La función **`0x019e2dc0..0x019e2e62`** valida la nota y lee un byte en
**`0x01aafd60 + nota`**. Si es cero, retorna por `0x019e2de8..0x019e2df2`, sin
escribir el mapa ni alcanzar preparación o transporte de salida.

El camino de byte no nulo empieza en **`0x019e2df4`**: borra esa entrada y
puede consultar `0x01a69360`, preparar bytes de salida y alcanzar
`0x019e0d18`. Ese camino **no se ejecuta ni se reconstruye**. No se fuerza
el estado del transporte para eludirlo.

Los **128 bytes del mapa son explícitamente cero** al iniciar cada caso o
secuencia y se retienen dentro de las secuencias. Se captura el mapa entero
con guardas y los monitores de salida `0x01aadb1c` (40 bytes) y `0x018b600c`
(4 bytes). No cambian. Esta es una precondición del ensayo, no una demostración
de quién inicializa o mantiene vacío el mapa en el firmware completo.

Además se instalan watches PC del GNU simulator en la entrada del camino de
mapa activo, las dos dependencias de transporte, helper de error, ramas de
discriminador excluidas y rango L1. La ejecución original completa termina con
código cero sin activar estas guardas. Antes se prueban las watches con un
programa **propio** sin firmware: salida normal 0; watch simple, múltiple y por
rango se detienen con código 5. No se parchean instrucciones originales.

## Retirada y adaptador independiente

Se reutilizan sin modificar **`Voice246PolyRelease.h`** y
**`Voice246ReleaseSelectors.h`**. El contexto es el ya contrastado: flags y modo
externo de VoiceCtl cero, ocho selecciones de sonido válidas, pool ADSR listo,
seis punteros Core y objetos Dr. Wave con vtable original. Sustain no negativo;
los coeficientes de afinación, controles y orden de llamadas son entradas.

Se ejercitan los selectores por sonido **0–4**, incluidos sus bits inferiores.
La retirada puede compactar la lista conservando el relleno de cada destino,
liberar una o seis envolventes o preparar pitch/RNG. La lista ausente y el
selector 4 preservan el estado según sus contratos existentes. El callback de
release de Dr. Wave conserva los objetos. **No hay render ADSR ni Dr. Wave**.

`Source/Voice246ReleaseDispatch.h` implementa únicamente ese contexto, conserva
la metadata y la copia por campos, y devuelve un enum **propio**. Rechaza otros
discriminadores, mapa activo o dependencias fuera del contrato; no emula asserts
ni sus retornos. Para notas válidas exige todo el mapa a cero, incluso con
metadata cero: esta protección delimita el adaptador y no amplía la semántica
original. Las notas descartadas no consultan esas dependencias posteriores.

El destino local del ensayo se precarga con patrones cuyo relleno difiere del
de la fuente. Un probe de frame **propio**, con el mismo `LINK 0x20`, calcula
dónde observar la copia tras el retorno. Con SP del harness `0x03000000`, la
posición calibrada es `0x02ffffe8`. Es una dirección del ensayo, no un buffer
fijo del aparato. Capturar ambos argumentos verifica también la anchura del
byte y que no se copia el word de metadata entero.

## Cobertura y preservación

Son **1.680 despachos originales desde su entrada**:

- **1.280 casos válidos independientes**: las 128 notas por cada selector,
  tanto metadata cero como no nula, y todos los tamaños de lista 0–128.
- **80 descartes independientes**: 16 patrones signed32 por selector, con
  límites 127/128, negativos y variaciones de anchura/bits altos.
- **Cinco secuencias de 64 llamadas**, una por selector, con lista, pools,
  RNG, mapa y monitores retenidos y orden explícito.

Hay **695 metadata cero / 905 no nula** entre los 1.600 casos admitidos. Se
incluyen bytes 1, 2, 127, 128 y 255. Se observan **1.018 retiradas** y
**582 búsquedas ausentes o desactivadas**, con duplicados al principio/final,
retirada de última nota y consultas presentes/ausentes.

Se compara una imagen de **4.656 bytes**: lista completa y contador/vecinos,
seis ADSR y scratch/guardas, seis Dr. Wave, registros VoiceCtl, parámetros de
ocho sonidos, RNG y dither previo/vecinos, ready, sonidos, punteros Core,
índice/coeficiente explícito de afinación, discriminador, mapa y monitores de
transporte. Se comparan también los 32 bytes de argumento fuente con guardas,
los 16 bytes locales y las 128 palabras de tabla de notas antes/después.
`0x01dfe72c` sigue siendo el último word de esa tabla, no una guarda ajena.

Las imágenes de objetos son estrés preservado con el contexto/vtables indicado,
no una afirmación de voces listas para render. La pila completa, temporización
y estado del hardware quedan fuera. REA 6.8.0 / GNU objdump aporta inspección
estática del dispatcher y la dependencia; GNU BF524 con ambos parches de
writeback realiza el oráculo numérico. REA no descifra L1 ni valida el DSP.

Dos ejecuciones privadas nuevas coinciden byte a byte. Pasan **49 suites de
núcleo Linux**, el comparador nuevo compilado para **Windows bajo Wine** y
**ASan/UBSan** (`detect_leaks=0` por la limitación de ptrace). Los probes de
watches retornan 0/5/5/5 como se espera y ambas ejecuciones originales terminan
con cero sin alcanzar las rutas vigiladas. CI nativo Windows/macOS queda
pendiente; no se añaden mediciones de audio o temporización física.

## Reproducir y coordinar

Usa [la guía de preparación](new-account-dsp-setup.md) con ambos parches. Cada
salida debe ser nueva, privada y externa al checkout:

```sh
python3 scripts/verify-voice246-release-dispatch.py \
  --ldr /privado/OP1_vdk.ldr --assembler /herramientas/bfin-elf-as \
  --linker /herramientas/bfin-elf-ld --simulator /herramientas/run-parallel \
  --testutils /fuentes-gdb/sim/testsuite/bfin --output /privado/dispatch-nuevo
cmake --build build-dsp --target voice246_release_dispatch_tests
ctest --test-dir build-dsp -R '^voice246_release_dispatch$' --output-on-failure
```

`--record` regenera solo los dos fixtures propios. Formato en
`Tests/ReferenceVectors/voice246-release-dispatch-format.md`; hashes, watches y
validación en `docs/firmware/voice246-release-dispatch-validation.json`.

La rama parte de `origin/main` **`4806844`**. La ruta de seis voces ocupadas de
`0x01a84604` y su helper `0x01a844d8` siguen reservados a la coordinadora; no se
desarrollan ni modifican aquí. Planes globales, cobertura, README común y helpers
existentes permanecen intactos. No se completan transporte activo, salida MIDI,
discriminadores alternativos, asignación, productor de buffers ni scheduler.
No cambian MIDI/automatización, sesiones, presets, bancos, interfaz o ejecutables.

Código y harness propios son AGPL-3.0-only; GNU/REA tienen las atribuciones de
[CREDITS](../CREDITS.md). El rev246 del propietario se usa solo privadamente.
Firmware, instrucciones, desensamblados y ELF no se publican. El ensayo no
certifica hardware, DAW/ASIO, latencia ni ausencia de dropouts.
