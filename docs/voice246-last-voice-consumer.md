# Rev246: consumidor gráfico de la consulta de última voz

El candidato **`0x01a4feaa` pertenece a una ruta con evidencia estática de
visualización**. No se ha identificado aquí una dependencia nueva necesaria para
ejecutar el DSP de Dr. Wave. Se documenta el descarte para no volver a perseguir
este recorrido como proveedor de pitch o relevo ADSR. No se implementa la UI.

## Límites y clasificación por inspección

`0x01a4feaa` es una llamada a la [consulta ya contrastada](voice246-last-voice-query.md)
`0x01a863a8`, seguida en `0x01a4feae` por una llamada al accesor
`0x01a4eec0`. **No es una entrada de función** y no se ejecuta desde esa posición.

El contenedor empieza en **`0x01a4f8a8`** con prólogo y marco de pila de
216 bytes. Su intervalo es **`[0x01a4f8a8, 0x01a4fef8)`**, 1.616 bytes:
el retorno común en `0x01a4fe14` precede a bloques alternativos y stubs de
error pertenecientes a la misma rutina. En `0x01a4fef8` comienza otra entrada.
Los límites se apoyan también en referencias alineadas a las entradas, en
`0x01911910` y `0x01000380`, respectivamente. Los nombres utilizados por REA
y el ELF de investigación son sintéticos; no se ha recuperado un nombre de clase.

El contenedor prepara transformaciones de nueve palabras, maneja pares de
coordenadas y actualiza campos de objetos subordinados. Llama a los recorridos
geométricos `0x01a072e8`, `0x01a070a4` y `0x01a06f98`. Su salida común consume
cuatro bytes de `0x01baaf58`, compatibles con un color, y los pasa con geometría
a `0x01a24a4c`, que utiliza el contexto de transformaciones `0x01de5e68`.
El conjunto sustenta la clasificación gráfica, sin identificar un modo comercial,
una pantalla concreta ni el contrato completo del callback.

## Uso observado del índice

La llamada `0x01a27e48`, desde `0x01a4fb32`, inspecciona seis **bytes** en
`0xff80105c`: devuelve cero si todos son cero y uno si encuentra alguno no nulo.
El resultado no nulo conduce en `0x01a4fb38` al bloque `0x01a4fe88`.
Esto es inspección estática: no se ejecutan ese gate ni el contenedor.

En ese bloque hay 16 iteraciones. **Cada iteración vuelve a llamar a la consulta
y al accesor**, suma un desplazamiento de bytes que empieza en cero y aumenta
de dos en dos, y lee un **signed16** de la dirección resultante. No conserva
una sola selección de voz para todo el bucle. Con estado estable, lee los
primeros 32 bytes del bloque seleccionado; la concurrencia original no se conoce.

El valor leído se desplaza aritméticamente cinco bits. Se combina con el campo
anterior mediante productos y desplazamientos, y se escriben **16 palabras de
32 bits** en los offsets **`+0x80` a `+0xbc`** del objeto del contenedor. El helper
`0x01aa5d78` es multiplicación multipalabra, no división: los factores visibles
son 102 y 51.200, con recombinaciones tras desplazamientos de diez bits. No se
convierte esta operación en un filtro DSP ni se implementa su recurrencia.
El bloque se reúne con el camino común en `0x01a4fc24`.

En este tramo el bloque seleccionado es una **fuente de lectura**, no un destino.
No se ha seguido su productor, cadencia, formato completo o relación con la
salida de audio. La consulta tampoco identifica por sí misma un instrumento.

## Accesor contrastado numéricamente

La función completa **`[0x01a4eec0, 0x01a4eee4)`**, 36 bytes, recibe una
palabra raw en R0 y devuelve en R0:

```text
(0x010225ec + raw_u32 × 0xc000) módulo 2^32
```

Usa las dos mitades del argumento. **No comprueba el índice ni desreferencia la
dirección calculada**, no consulta globales y no escribe memoria compartida.
Solo utiliza su marco normal de llamada para retornar; registros de trabajo y
pila no se presentan como una imagen completa de máquina verificada.

| Índice devuelto por la consulta | Dirección del accesor |
| --- | --- |
| 0 | `0x010225ec` |
| 1 | `0x0102e5ec` |
| 2 | `0x0103a5ec` |
| 3 | `0x010465ec` |
| 4 | `0x010525ec` |
| 5 | `0x0105e5ec` |

La separación es **49.152 bytes**, equivalente a 24.576 halfwords. Como apoyo
estático, `0x01a4ef64` comprueba un índice signed 0–5 y conserva una dirección
con esa misma disposición. El inicializador `0x01a4f3e0` referencia la base,
147.456 elementos de tamaño dos y un callback que escribe un halfword cero.
Estos otros recorridos no se ejecutan ni implementan; no establecen valores
predeterminados de un motor ni que los seis bloques sean buffers de audio.

## Ensayo acotado y efectos vigilados

El probe propio llama a las entradas completas de consulta y accesor, en ese
orden, **sin ejecutar el bucle gráfico**. Emplea GNU BF524 con ambos parches de
writeback documentados y el probe halfword previo. REA 6.8.0 / GNU objdump
inspecciona las dos rutinas sintéticas completas; no descifra ni aporta el
contraste numérico.

- **80 llamadas raw** al accesor: 0–7, límites de halfword/word, bits altos,
  walking bits y sus complementos. Las direcciones fuera de rango nunca se leen.
  La fórmula general se deriva de las instrucciones; el ensayo no recorre los
  2³² argumentos.
- **1.024 enlaces consulta → accesor**, reutilizando sin regenerar los fixtures
  de la consulta: listas de longitud 0–128, los seis retornos, descartes signed32,
  duplicados, orden inverso, fallback válido y ocho secuencias de 64 llamadas.
  La consulta mantiene exactamente sus entradas, recargas y estado retenido.
- Por enlace, se compara el retorno de la consulta y su imagen compartida
  completa de **3.740 bytes** contra el oráculo existente, además de la dirección
  devuelta por el accesor. Se cubren lista y vecinos, contador, palabra
  persistente y vecinos, VoiceCtl, ADSR, RNG y otros globales seleccionados.
  Solo cambia la palabra persistente conforme al contrato previo de consulta.
- Se carga un patrón propio en los seis bloques y guardas de ocho bytes a cada
  lado: **294.928 bytes desde `0x010225e4`**, comparados completos antes y después.
  No se usan contenidos originales como fixtures ni se leen punteros raw inválidos.

Dos ejecuciones privadas nuevas coinciden byte a byte en entradas, patrones y
**4.428.448 bytes de salida**. Las guardas de PC detienen la ejecución si se entra
en el contenedor gráfico, el helper de error `0x01919aa8` o el intervalo L1 de
código `0xffa00000..0xffafffff`. Todas las entradas se comprueban previamente
para excluir el fallback inválido de la consulta.

El contenedor tiene además llamadas indirectas a `0xffa0a1e0` en
`0x01a4fd66`, `0x01a4fe84`, `0x01a4fee8` y `0x01a4fef4`. Esas rutas, sus
precondiciones completas y sus efectos no se ejecutan ni se sustituyen por
retornos inventados. El ensayo no establece seguridad ni preservación de memoria
del contenedor completo, una cadena de audio, scheduler, temporización o hardware.

## Reproducción y cierre del bloque

Sigue [la preparación entre cuentas](new-account-dsp-setup.md), con la opción
`--halfword-writeback` al preparar GNU. El script solo admite el LDR inventariado
y una salida nueva y privada **fuera del checkout**:

```sh
python3 scripts/inspect-voice246-last-voice-consumer.py \
  --ldr /privado/OP1_vdk.ldr \
  --assembler "$POCKET_BFIN_AS" --linker "$POCKET_BFIN_LD" \
  --simulator "$POCKET_BFIN_SIM" --testutils "$POCKET_BFIN_TESTUTILS" \
  --output /privado/last-voice-consumer-nuevo
```

El formato de salida es propio: imagen inicial de 294.928 bytes; 80 pares
little-endian de entrada/dirección (ocho bytes cada uno); 1.024 registros de
3.748 bytes (retorno consulta, dirección accesor, imagen compartida en el orden
de regiones del verificador de consulta); imagen final de 294.928 bytes.
Las regiones, hashes, herramientas e inspección REA constan en
[la evidencia](firmware/voice246-last-voice-consumer-validation.json).

Pasan las **49 suites CTest de núcleo Linux** de la base actual. Este bloque no
añade implementación en Source, suite CMake, fixtures públicos ni cambios al
plugin. No cambia MIDI, automatización, sesiones, presets o UI. La PR #25 está
fusionada y no se repite; la PR #26 y continuación de la coordinadora quedan
fuera del alcance. Planes, cobertura e índice común de fixtures no se modifican.

**Se detiene aquí la investigación de este consumidor:** los hechos sobre la
disposición de memoria son verificables, pero su ruta gráfica no resuelve el
productor de pitch, el relevo ADSR o la integración DSP pendiente. Firmware,
instrucciones originales, desensamblados y ELF permanecen privados. El probe
propio es AGPL-3.0-only; las herramientas conservan las atribuciones de
[CREDITS](../CREDITS.md).
