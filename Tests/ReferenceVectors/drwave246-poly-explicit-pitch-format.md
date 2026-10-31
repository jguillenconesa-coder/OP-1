# Formato propio: selector 0 y pitch explícito no cero

Datos numéricos little-endian de `scripts/verify-drwave246-poly-explicit-pitch.py`.
No contienen firmware, instrucciones, desensamblados, gráficos o audio. Solo
`--record` escribe estos dos fixtures nuevos; no modifica fixtures existentes.

## Entrada

`drwave246-poly-explicit-pitch-input.bin`: **23.306.240 bytes**, 4.552 registros
de **5.120 bytes** sin cabecera de archivo.

| Offset | Bytes | Contenido |
| ---: | ---: | --- |
| 0 | 4 | Recarga completa si no es cero; de lo contrario conserva regiones retenidas. |
| 4 | 4 | Clase propia: 0 libre, 1 ocupada, 2 gate, 3 duplicado. |
| 8 | 8 | Guarda anterior del argumento. |
| 16 | 16 | Argumento raw: nota u32, velocidad u32, metadata byte, tres bytes de padding y pitch explícito raw32 no cero. |
| 32 | 8 | Guarda posterior del argumento. |
| 40 | 4.568 | Imagen de regiones compartidas. |
| 4608 | 512 | Tabla sintética completa de 128 palabras de lookup. |

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

Tras las recargas, gate **byte**, seis actividades y seis contadores ADSR en
offset 4 de cada estado se suministran siempre desde la fila actual. Los estados
ADSR empiezan en offset 2580; los seis objetos Dr. Wave de 36 bytes en offset
3240. Sus 216 bytes completos y los ocho vecinos posteriores se comparan.

La tabla se carga después en `0x01dfe530`. Su última palabra ocupa
`0x01dfe72c`, primera palabra de la imagen: el comparador respeta el solapamiento.
El contador de lista, en offset 2568, **no se fuerza a cero** ni se recarga por
llamada cuando la región es retenida. Las 128 entradas empiezan en offset 8.

## Casos y secuencias

Los primeros **3.976 registros** son **497 grupos de ocho**, cruzando 71 palabras
de pitch y siete coeficientes de afinación. El orden es:

1. Libre, Core coincidente, tabla cero / tabla sintética no cero.
2. Libre, Core no coincidente, tabla cero / tabla sintética no cero.
3. Seis ocupadas, Core coincidente, tabla cero / tabla sintética no cero.
4. Seis ocupadas, Core no coincidente, tabla cero / tabla sintética no cero.

Cada par de tablas conserva todos los demás inputs. Entre los perfiles Core
coincidente/no coincidente de una misma ruta solo cambia la palabra de sonido
cargado. Los perfiles ocupados mantienen las mismas edades y lista para que
su invariancia no dependa de comparar selecciones distintas.

Siguen **96 gates, 96 duplicados** y **seis streams de 64 llamadas**. Los streams
empiezan con 17 entradas y retienen lista, ADSR salvo contadores suministrados,
objetos, otros registros y RNG. No se ejecutan constructor, retirada o dispatcher
exterior. Actividad y edades son entradas por llamada, no un scheduler recuperado.

Nuevas entradas: contador menor que 128 y, si las seis actividades son no nulas,
al menos un contador signed32 no negativo. Los gates/duplicados incluyen contador
128 y seis edades negativas: retornan antes de selección, sin ejecutar asserts.
No se inventa un retorno para el caso ocupado nuevo con todas las edades negativas.

## Salida

`drwave246-poly-explicit-pitch-output.bin`: **23.269.824 bytes**, 4.552 registros
de **5.112 bytes**, sin prefijo ni sufijo:

| Offset | Bytes | Contenido |
| ---: | ---: | --- |
| 0 | 4.568 | Imagen compartida completa posterior de las regiones seleccionadas. |
| 4568 | 32 | Guardas y argumento completo, idénticos a entrada 8–39. |
| 4600 | 512 | Tabla completa posterior, idéntica a la suministrada. |

La invariancia de lookup excluye solo la primera palabra de imagen, por el
solapamiento de tabla, y compara por separado la tabla íntegra. La invariancia
Core de la ruta ocupada excluye únicamente la palabra de sonido cargado.

No hay audio/render ADSR ni retorno semántico R0. Resultados y guards del
adaptador son propios. RAM, pila y registros completos de CPU no forman parte
de este formato. Firmware, instrucciones, desensamblados, ELF y logs de análisis
solo se crean en una salida privada nueva fuera del checkout.
