# Formato propio: Dr. Wave rev246, selectores con lista no vacía

Datos numéricos little-endian. Generador/verificador propio:
`scripts/verify-drwave246-held-note-modes.py`. Ningún fichero contiene
instrucciones originales o extractos de ejecutables. No modifica otros fixtures.

## Entrada

`drwave246-held-note-modes-input.bin`: **1.536 × 4.608 = 7.077.888 bytes**.

| Offset del registro | Bytes | Contenido |
| --- | ---: | --- |
| 0 | 4 | Recargar todas las regiones si no es cero. |
| 4 | 4 | Clase esperada: 0 append, 1 gate, 2 duplicado; etiqueta propia. |
| 8 | 8 | Guarda anterior del argumento. |
| 16 | 16 | Argumento raw: nota u32, velocidad u32, metadata byte, padding de tres bytes, pitch explícito raw32. |
| 32 | 8 | Guarda posterior del argumento. |
| 40 | 4.568 | Imagen de regiones en el orden siguiente. |

La columna «retenida» indica que, con recarga cero, se conserva la salida de
la llamada anterior. Gate **byte** y seis actividades se suministran siempre
desde la fila actual, sin recargar sus vecinos ni el resto de registros.
El resto de controles no retenidos se carga en cada llamada.

| Offset en imagen | Dirección | Bytes | Retenida |
| ---: | --- | ---: | --- |
| 0 | `0x01dfe72c` | 2.576 | Sí |
| 2576 | `0xff8000d0` | 664 | Sí |
| 3240 | `0xffb00000` | 224 | Sí |
| 3464 | `0xff801034` | 776 | Sí |
| 4240 | `0x01b60ef0` | 128 | No |
| 4368 | `0xff802bc8` | 8 | Sí |
| 4376 | `0x01b60534` | 8 | Sí |
| 4384 | `0x01b6053c` | 8 | No |
| 4392 | `0xff8016d4` | 8 | No |
| 4400 | `0x01aaac0c` | 8 | No |
| 4408 | `0x01aaad14` | 32 | No |
| 4440 | `0x027fd9dc` | 12 | No |
| 4452 | `0x018c2180` | 12 | No |
| 4464 | `0x01de4c3c` | 12 | No |
| 4476 | `0x01dfe4d8` | 48 | Sí |
| 4524 | `0x01aaad68` | 32 | No |
| 4556 | `0x01aaac40` | 12 | No |

La primera palabra de la imagen es la vecina real `0x01dfe72c`, que coincide
con la última palabra de la tabla de notas. La lista empieza en offset 8,
con 128 registros de 20 bytes; el contador está en offset 2568. El gate está
en offset 4. La región ADSR contiene ready=1, seis estados y scratch vigilado.
Los objetos Dr. Wave tienen vtables válidas y campos de estrés propios; no se
ejecuta su renderer. Índice de afinación 1200 y coeficiente son entradas
explícitas, sin atribuirles valores predeterminados del aparato.

Hay **1.152 casos independientes**: 256 triples controlados (filas `i`,
`i+256`, `i+512`), 192 gates y 192 duplicados. Dentro de cada triple solo
cambia la palabra de selector del sonido seleccionado. Siguen **seis streams
de 64 llamadas**; los tres primeros mantienen selector 1/2/3 respectivamente,
y los tres últimos los alternan. Cada stream comienza con 17 entradas y retiene
la lista. Todas las llamadas entran con lista no vacía; jamás se ejecuta el
contexto reservado de lista vacía.

## Salida

`drwave246-held-note-modes-output.bin`: **7.066.624 bytes**:

1. Tabla de 128 palabras de nota: 512 bytes.
2. 1.536 registros de **4.600 bytes**: imagen completa posterior de 4.568 bytes,
   seguida de los 32 bytes de argumento y guardas.
3. La tabla completa otra vez: 512 bytes, idénticos al prefijo.

No hay render ADSR, muestras de audio ni resultado semántico R0 en este formato.
Los estados `gated`, `duplicate`, `appended` y `unsupported` son del adaptador;
no son valores retornados observados del original.

Firmware, SDRAM extraída, harness ensamblado, instrucciones y ELF solo se crean
en una salida privada nueva fuera del checkout. Sin `--record`, el verificador
compara estos dos fixtures existentes sin regenerarlos. Los guards de capacidad,
nota nueva fuera de 0–127 y demás contextos excluidos pertenecen al adaptador;
no se presentan como asserts originales ejecutados.
