# Investigación DSP — OP-1 revisión 246

Este registro continúa el [inventario con REA](firmware-analysis.md). No hay todavía un port de los motores originales ni una equivalencia sonora validada. La versión 0.5.0 distribuye los motores propios existentes.

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
