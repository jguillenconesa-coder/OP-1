# Formato propio: lista vacía y pitch explícito no cero

Fixtures numéricos little-endian del verificador
`scripts/verify-drwave246-empty-explicit-pitch.py`. No contienen instrucciones,
firmware extraído, gráficos o audio. Solo `--record` escribe estos dos archivos.

## Entrada: 17.725.440 bytes

**3.462 registros de 5.120 bytes**, sin cabecera de archivo:

| Offset | Bytes | Contenido |
| ---: | ---: | --- |
| 0 | 4 | Recarga completa si no es cero; en caso contrario conserva las regiones retenidas. |
| 4 | 4 | Etiqueta propia: 0 llamada normal, 1 gate no nulo. |
| 8 | 8 | Guarda anterior del argumento. |
| 16 | 16 | Nota u32, velocidad u32, metadata byte, padding 9–11 y pitch explícito raw32 **no cero**. |
| 32 | 8 | Guarda posterior del argumento. |
| 40 | 4.568 | Imagen de regiones compartidas, orden inferior. |
| 4608 | 512 | Tabla de lookup sintética completa: 128 palabras raw32. |

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

Tras cargar/conservar regiones, el harness suministra siempre el byte de gate
y las seis actividades de la fila. **Fuerza el contador a cero en cada llamada**
y copia los 512 bytes sintéticos a `0x01dfe530`. Esto sobrescribe la primera
palabra de la imagen: es la última palabra de la tabla en `0x01dfe72c`, vecina
real del gate/lista. El comparador respeta ese solapamiento; no lo trata como
guarda inmutable o entrada de lista.

Los seis objetos Dr. Wave empiezan en offset 3240 y ocupan 216 bytes; los ocho
bytes restantes de esa región son vecinos vigilados. Los seis ADSR de 24 bytes
empiezan en offset 2580, después del flag ready=1. Los demás bytes de esa región
incluyen scratch y vecinos. No se presenta ninguna de estas imágenes como un
instrumento listo para render ni como toda la memoria del dispositivo.

## Orden de los casos

Los primeros **2.982 registros** son **497 grupos de seis**. Cada grupo cruza
una de 71 palabras de pitch con uno de siete coeficientes de afinación. Sus
miembros son selector1/tabla0, selector1/tabla1, selector2/tabla0, selector2/tabla1,
selector3/tabla0 y selector3/tabla1. Solo cambian la palabra del selector y la
tabla. Tabla0 contiene ceros; tabla1 contiene palabras pseudoaleatorias no cero.

Siguen **96 gates independientes** y **seis streams de 64 llamadas**. Cada stream
recarga inicialmente sus regiones retenidas; después conserva lista/padding,
ADSR, Dr. Wave, registros y RNG. El contador y la actividad siguen siendo
entradas explícitas cada llamada. No hay constructor, note-off o ciclo de vida
natural de notas. Los tres primeros streams mantienen selector 1/2/3; los tres
últimos lo alternan. Hay otras 48 llamadas inhibidas por gate en los streams.

## Salida: 17.697.744 bytes

**3.462 registros de 5.112 bytes**, sin prefijo o sufijo de archivo:

| Offset | Bytes | Contenido |
| ---: | ---: | --- |
| 0 | 4.568 | Imagen compartida después del llamador completo original. |
| 4568 | 32 | Guardas y argumento completo, idénticos a entrada 8–39. |
| 4600 | 512 | Tabla sintética completa posterior, idéntica a la entrada. |

La invariancia entre pares de lookup excluye solo la palabra compartida en
offset 0, que cambia por el solapamiento real, y la tabla capturada separadamente.
Todos los demás efectos, padding, argumentos y guardas deben coincidir.

No se capturan muestras, render ADSR o un retorno semántico R0. Enum, contadores
y guards del adaptador son propios. Firmware, instrucciones, desensamblados,
ELF y logs originales se conservan únicamente en una salida privada fuera
del checkout. Los scripts no los publican como artefactos.
