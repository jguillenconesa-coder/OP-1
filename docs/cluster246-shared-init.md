# Cluster: inicialización compartida y conversiones del firmware

Se reconstruye la rutina **`0x019608e0`** que convierte la tabla fuente,
establece una palabra compartida de control y limpia los pesos y el display.
Las conversiones Q15/Q31 se implementan a partir de los bits IEEE binary32,
sin depender del redondeo o de conversiones indefinidas de NaN del host.

**La lógica está contrastada con entradas explícitas de prueba. Las tres entradas
originales de normalización para 2, 3 y 8 osciladores siguen desconocidas.** Esta
ampliación no ofrece una tabla original completa ni integra Cluster en el plugin.

## Estado compartido observado

| Dirección | Efecto del inicializador |
| --- | --- |
| `0x01baa918` | Escribe la palabra signed16 `16384`; no se le atribuye aquí una semántica de master/gain no comprobada. |
| `0x01baa91c` | Convierte ocho palabras float32 de `0xff900524` a ocho valores Q15. |
| `0x01baa92c` | Pone a cero los ocho pesos compartidos. |
| `0x01baa93c` | Pone a cero los ocho valores de visualización. |
| Semilla RNG y tabla de amortiguación | Se conservan, al igual que los bytes vecinos no escritos. |

La API exige proporcionar **las ocho palabras fuente**. Los casos de prueba
incluyen las cinco palabras legibles conocidas, combinadas con valores sintéticos
en los tres huecos, y otras entradas arbitrarias. Esos valores sintéticos no se
presentan como coeficientes recuperados. La consulta de valores originales
observados continúa devolviendo ausencia para los tres casos pendientes.

## Conversiones verificadas

Los helpers originales son **`0x01aa7014`** (Q15) y **`0x01aa6fd0`** (Q31).
La reconstrucción conserva:

- Truncamiento de la magnitud antes de aplicar el signo.
- Cero para magnitudes que no alcanzan un LSB.
- Saturación positiva/negativa para magnitudes de al menos uno.
- Saturación según el signo incluso para infinitos y NaN: es el comportamiento
  observado de estos helpers, no una regla general de aritmética IEEE.

Se contrastan **65.536 patrones binary32**, con **131.072 resultados**. Incluyen
3.072 combinaciones dirigidas de todos los exponentes, ambos signos y mantisas
límite; además, palabras de los coeficientes ya observados y patrones aleatorios.
Los ceros con signo, subnormales, transiciones de exponente, infinitos y NaN están
incluidos. No se afirma haber enumerado todos los 2³² patrones.

En **256 casos del inicializador** se compara una imagen de 64 bytes con guardas,
la semilla de cuatro bytes y 128 bytes de amortiguación. Se conservan los campos
que la rutina original deja intactos. Los dos oráculos privados y el ejecutable
GNU sin el parche local producen las mismas salidas para estas pruebas. El último
es una variante del mismo modelo, no una validación independiente de hardware.

[Resultados, hashes y límites](firmware/cluster246-shared-validation.json) ·
[Contrato de Cluster](../research/blackfin/cluster246-contract.json).

## Apoyo de REA

REA se utiliza en modo de lectura para consultar fuentes auxiliares y conservar
su procedencia. En esta etapa inspeccionó el esquema de `kerntable.db` y cinco
filas: columnas de caracteres y valores de kerning, compatibles con espaciado
tipográfico. No se usa esa muestra como fuente de coeficientes DSP ni se concluye
nada sobre todas las filas o el resto de bases de datos.

El intento de inspección ELF con REA devolvió `provider_unavailable`: su proveedor
pwntools no está configurado. Se conservó ese resultado y se verificó la estructura
ELF con GNU Binutils. No se atribuyen a REA descompilación Blackfin ni descifrado L1.
[Resumen verificable de REA y del recurso alternativo](firmware/rea-cluster-shared-support.json).

## Reproducir

```bash
python3 scripts/verify-cluster246-shared.py \
  --ldr /privado/OP1_vdk.ldr \
  --assembler /herramientas/bfin-as --linker /herramientas/bfin-ld \
  --simulator /herramientas/run-dreg-deferred \
  --testutils /fuentes-gdb/sim/testsuite/bfin \
  --output /privado/cluster-shared-nuevo
```

El directorio debe ser nuevo, privado y estar fuera del checkout. CMake registra
`cluster246_shared`. No se incluyen firmware, ELF ni instrucciones originales.
Las 16 suites de núcleo Linux, las tres suites Cluster en Windows/Wine y
ASan/UBSan del conjunto nuevo han pasado. La asignación de voces, cadencia,
salida completa y normalizaciones cifradas siguen pendientes en el
[plan DSP](firmware-dsp-workplan.md). Presets y vídeo permanecen aplazados.
