# Dr. Wave rev246: consumo del contador de actualización

El render usa **los 16 bits completos** del contador en `0x01ab8bac` para elegir
el slot que reconstruye su onda. El adaptador aislado antes admitía solo
`0..32767`; ahora acepta `0..65535`. Este cambio evita rechazar la mitad superior
de la palabra al conectar un futuro proveedor de contador. **Todavía no está
integrado en el plugin ni identifica el reloj original.**

## Contrato contrastado

La carga original es signed16, pero las operaciones siguientes consumen su mitad
baja con productos **sin signo**. El resto equivale a `uint16(counter) % 6`,
incluido el bit alto. No se aplica el resto de un entero negativo de C++.

| Palabra raw | Slot que actualiza |
| --- | --- |
| `0x7fff` / 32767 | 1 |
| `0x8000` / 32768 | 2 |
| `0xfffe` / 65534 | 2 |
| `0xffff` / 65535 | 3 |
| `0x0000` / 0 | 0 |

Los saltos de esta tabla describen al **consumidor** si se le suministran esas
palabras. No demuestran que el proveedor real avance de uno en uno o vuelva a
cero en 65535. El adaptador acepta un `int` no negativo hasta 65535 y rechaza
otros valores antes de escribir; el llamador debe representar la palabra raw
sin extender su signo. El lector aislado sigue rechazando su propio slot de
actualización; el [render completo](drwave246-kernel.md) sí lo ejecuta.

El render conserva el contador y la mitad de palabra vecina. Su deshabilitación
sigue teniendo precedencia y no modifica salida ni estado.

## Dos oráculos

1. **131.072 ejecuciones del fragmento original** `0x0197c89a..0x0197c8ca`
   (fin exclusivo): los 65.536 valores posibles, cada uno con la palabra vecina
   a cero y a 65535. Se compara el resto y la conservación del DWORD fuente.
   El fragmento no contiene saltos relativos y se inserta desde la imagen privada
   en un llamador propio, con retorno añadido. Esta prueba no ejecuta el render
   completo ni se cuenta como audio.
2. **512 llamadas adicionales al render completo sin modificar** `0x0197c870`:
   256 casos independientes y dos secuencias de 128. Las secuencias reciben un
   contador explícito por cada seis llamadas, empezando en 32758 y 65526, para
   cruzar tanto el bit de signo como el límite de 16 bits. Este orden de llamadas
   es una condición de prueba, no una cadencia recuperada.

Las **61.568 muestras adicionales** coinciden: 79 actualizaciones, 402 lecturas
sin actualización y 31 retornos inactivos. Hay 239 llamadas activas con bit alto,
39 de ellas con actualización de onda, y tres vueltas del acumulador de inversión.
Se comparan también objetos, seis ondas, cachés, configuración, incrementos,
punteros, flags, guardas y conservación de bancos. Se ejercitan los seis slots,
seis variantes espectrales y tres transformaciones.

El oráculo previo de 61.568 muestras se conserva intacto. Ambos se ejecutan con
el mismo comparador C++, alcanzando **123.136 muestras de render completo**.
Dos reproducciones privadas de los oráculos nuevos coinciden byte a byte.
Pasan las 24 suites Linux, las pruebas de render/lector compiladas para Windows
y ejecutadas bajo Wine, y ASan/UBSan del comparador ampliado. No son pruebas
físicas de Ableton/ASIO ni medidas de latencia.

## Proveedor del contador: dependencia concreta

La búsqueda de materializaciones directas localiza otra mitad baja `0x8bac` en
`0xffa01814`, dentro del payload L1 del bloque LDR `0x206214`: destino
`0xffa00000`, 49.076 bytes, flags `0xaddb2406`.
En tramos legibles próximos aparecen una lectura signed16 mediante `P0`, en
el grupo paralelo de `0xffa01824`, y un incremento de `R0` en `0xffa0182e`.

Es una **referencia candidata**, porque faltan el establecimiento completo de
`P0`, la escritura posterior y el flujo que la alcanza. Según el formato
comunitario documentado —ocho bytes cifrados por cada 24— las franjas
`[0xffa01818,0xffa01820)` y `[0xffa01830,0xffa01838)` están cifradas. No se
decodifican esos bytes como instrucciones ni se ejecuta esa ruta. No se atribuye
al firmware el reloj empleado por las pruebas.

El siguiente paso sigue siendo recuperar y contrastar el proveedor del contador
y su relación con el despacho de voces. Afinación/pitch, asignación, ADSR y
salida siguen siendo dependencias separadas antes de integrar Dr. Wave.

## Reproducción

```bash
python3 scripts/verify-drwave246-counter.py \
  --ldr /privado/OP1_vdk.ldr --assembler /herramientas/bfin-as \
  --linker /herramientas/bfin-ld --simulator /herramientas/run-dreg-deferred \
  --testutils /fuentes-gdb/sim/testsuite/bfin --output /privado/drwave-counter-nuevo

cmake --build build-dsp --target drwave246_kernel_tests drwave246_tests
ctest --test-dir build-dsp -R '^drwave246(_kernel)?$' --output-on-failure
```

El script valida el hash rev246 y reutiliza el fixture de entradas anterior,
con overrides de eventos/slot/contador. Las secuencias de 128 llamadas recargan
estado solo al inicio, conservando sus eventos de constructor/reset/release.
Imagen, instrucciones y ELF permanecen fuera
del repositorio; solo se distribuyen las observaciones numéricas.
[Evidencia y hashes](firmware/drwave246-counter-validation.json) ·
[Plan activo](firmware-dsp-workplan.md).
