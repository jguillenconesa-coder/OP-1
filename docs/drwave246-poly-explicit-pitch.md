# Rev246: selector 0 con pitch explícito no cero

Se contrasta desde su **entrada completa** el llamador
`[0x01a84604, 0x01a84ba0)`, 1.436 bytes, con selector por sonido **0**, modo
externo cero y argumento raw de pitch explícito no cero. Se incluyen primera
voz libre y seis voces ocupadas. No se extrapola el contrato de los selectores
1–3 de [PR #31](drwave246-empty-explicit-pitch.md).

Los helpers están delimitados por prólogos y flujo legible:

| Helper | Intervalo exclusivo | Bytes |
| --- | --- | ---: |
| Preparación tras solicitud Core | `0x01a84414..0x01a844d8` | 196 |
| Solicitud de reinicio y reasignación | `0x01a844d8..0x01a84604` | 300 |

Se ejecutan las instrucciones originales sin parches. El harness solo llama al
padre; no ejecuta un fragmento aislado desde sus ramas. Hay seis ADSR ready=1,
objetos Core Dr. Wave y auxiliares nulos. Sus campos de estrés no representan
un instrumento completo listo para render.

## Tratamiento del campo explícito

Ambos helpers sustituyen la fuente de lookup por la palabra no cero en offset
12 **antes** del producto Q31 y del desplazamiento saturado de un bit:

- En el helper libre se lee en `0x01a84446` y el salto de `0x01a8444e` lleva
  al producto en `0x01a8445e`. **La aceptación Core precede a esa preparación**;
  un rechazo no alcanza el producto dentro de este helper.
- En el ocupado se lee en `0x01a844fa` y el salto de `0x01a84502` lleva al
  producto en `0x01a84512`. No se consulta allí la aceptación de inicio Core.

El patrón raw se interpreta signed32 para la aritmética. El coeficiente de
afinación es explícito en `0x027fd9e0`, usando índice suministrado 1200 en
`0x018c2184`; no se atribuye calibración de fábrica o unidad de frecuencia.
El producto descarta los bits fraccionarios mediante aritmética signed y después
se aplica el shift saturado. No se escribe la palabra raw directamente como
pitch final saltándose afinación.

Hay guardas PC sobre ambos bloques de lookup originales
(`0x01a84450..0x01a8445d` y `0x01a84504..0x01a84511`): ninguno se alcanza.
Además, **1.988 pares** cambian los 512 bytes completos de tabla entre ceros y
datos sintéticos no cero y conservan los demás efectos. El vecino que coincide
con la última palabra de la tabla se trata como solapamiento real, no como una
guarda inmutable. La tabla completa se captura y conserva en cada llamada.

## Selección, Core y estados resultantes

Antes de seleccionar se comprueba el byte de gate `0x01dfe730`, seguido de la
búsqueda de igualdad de la **palabra completa** de nota en la lista contada.
Gate no nulo o coincidencia retorna antes de mirar actividad/edades. La metadata
y los identificadores asignados no filtran esa búsqueda.

Con nota nueva, se escanean las actividades de las seis voces en orden. La
primera actividad cero elige esa voz libre, incluso con contadores negativos
en las precedentes. Si las seis están ocupadas, gana el máximo **signed32** del
contador ADSR en offset 4, comparado estrictamente contra un máximo inicial -1.
Los empates conservan la primera voz. No se deduce duración física o cadencia.

| Efecto | Primera voz libre | Seis voces ocupadas |
| --- | --- | --- |
| ADSR seleccionado | `noteOn`: etapa 1, contador y nivel cero; conserva palabras pendientes. | Solicitud de reinicio: etapa 6, contador cero; conserva nivel. |
| Ganancia ADSR | Recalcula solo mitad baja; mitad alta conservada. | Conserva la palabra completa. |
| Pitch preparado | Registro VoiceCtl si Core acepta. | Campo pendiente offset 12. |
| Velocidad preparada | Registro VoiceCtl si Core acepta. | Patrón uint16 en mitad alta de campo pendiente offset 16; mitad baja cero. |
| Actividad | Se escribe 1 si Core acepta. | Conservada. |
| Demás registros VoiceCtl | Conservados. | **Todos conservados**, incluida velocidad. |
| Objeto Dr. Wave | Si Core acepta, resetea cinco fases y campo32; conserva demás palabras. | Conservado completo. |
| RNG y vecinos | Conservados. | Conservados. |
| Dueño anterior en lista | No se desvincula. | Primera entrada con identificador completo igual a la víctima pasa a `0xffffffff`. |

La velocidad usa el halfword bajo signed16 con shift saturado de ocho bits.
La palabra completa de velocidad se conserva en la lista. Para notas 0–127,
la ganancia baja es 32767 en nota cero y `32768 − nota × 128` en las demás.

Core acepta cuando coinciden cargado y seleccionado. En la rama libre, un
rechazo **no revierte ADSR ni ganancia**, conserva objetos/registros y aun así
añade la nota. La ruta ocupada se conserva idéntica en **497 pares** que solo
cambian esa coincidencia. Su despacho auxiliar retorna sin callbacks al ser nulo.

**No hay desviación RNG en estas rutas polifónicas.** No se importa el avance ni
el desvío por voz de los selectores 1–3. Escribir el pitch registrado o pendiente
no demuestra producir el buffer del renderer: ese consumidor L1 sigue sin
recuperarse y no se añade un sustituto.

## Lista, conservación y contextos excluidos

Ambas rutas añaden nota, velocidad, metadata byte y pitch explícito raw completo,
con identificador **0–5 de la voz elegida**, y aumentan el contador. No usan el
marcador -2 de selectores 1–3. Se conserva el padding 9–11 del destino y toda
la cola no escrita. El argumento de 16 bytes y sus guardas permanecen intactos.

La desvinculación ocupada afecta solo al **primer** identificador coincidente,
sin eliminar el registro, filtrar por metadata o cambiar las coincidencias
posteriores. Un identificador con los mismos bits bajos y bits altos distintos
no coincide. Si no hay dueño previo, igualmente añade la nueva entrada.

Las nuevas notas se limitan a 0–127, pitch no cero y contador menor que 128.
Si se alcanza la elección con seis voces ocupadas, al menos una edad signed32
debe ser no negativa. Las rutas de capacidad/assert y todas-negativas en esa
selección quedan fuera del oráculo. Sí se prueban seis edades negativas con
voz libre, o gate/duplicado que evita la selección. No se emula el assert con
un supuesto retorno o víctima de fallback.

`Source/DrWave246PolyExplicitPitch.h` reutiliza preparación de registros/Core,
aritmética y solicitud ADSR contrastadas. No modifica sus helpers ni los wrappers
previos que excluyen pitch no cero. Los guards y estados del nuevo adaptador
son propios; no son retornos semánticos originales de R0.

## Ensayo y resultados

- **4.552 llamadas originales**: 4.168 independientes y seis streams de 64.
- 71 patrones de pitch no cero —walking bits, complementos, extremos signed32
  y vecinos de saturación— cruzados con siete afinaciones. Cuatro perfiles de
  ruta/Core y dos tablas: **3.976 casos de matriz**.
- **2.132 rutas libres**, con 1.066 aceptaciones y 1.066 rechazos Core;
  **2.132 ocupadas**, con las seis víctimas, 850 empates válidos, 1.605 primeras
  desvinculaciones y 541 casos con varios dueños coincidentes.
- **144 gates y 144 duplicados**, incluidos contador 128 y seis edades negativas.
  Se cubren todas las longitudes 0–128, las 128 notas en ambas rutas, las seis
  elecciones libres, metadata 0/1/128/255 y velocidades low16 dirigidas con
  bits altos independientes. RNG permanece igual en las 4.552 llamadas.

Se compara la imagen íntegra de **4.568 bytes de regiones seleccionadas**:
lista/contador/gate y vecinos, seis ADSR y scratch, seis objetos Dr. Wave completos,
registros VoiceCtl, sonidos, RNG/vecinos, selección/cargado, punteros/auxiliares,
afinación/discriminador y ajustes. Se suman argumento con guardas de ocho bytes
a cada lado y tabla completa de 512 bytes. No se afirma vigilar toda la RAM,
pila, CPU o mutaciones concurrentes del aparato.

Los streams retienen lista, niveles/ganancias/palabras pendientes, objetos,
otros registros y RNG. **Actividad y seis edades se suministran en cada llamada**,
igual que controles, afinación y tabla. El orden y esos valores son del harness;
no son una reconstrucción del productor de actividad, edades o scheduler.

Dos reproducciones privadas finales coinciden byte a byte sin regenerar fixtures.
Pasan **55 suites de núcleo Linux**, comparador Windows ABI MSVC bajo **Wine**
y **ASan/UBSan** (`detect_leaks=0` por ptrace). CI nativo Windows/macOS queda
pendiente; no se valida hardware, DAW, ASIO, latencia o dropouts.

El oráculo usa GNU BF524 con ambos parches y preflight halfword. Las seis
guardas PC excluyen otros selectores, dispatcher exterior reservado, ambos
lookups, helper de error y código L1. Controles positivos en las dos entradas
del producto detienen con código 5 antes de las seis otras opciones de guarda.

REA/GNU objdump inspeccionó el llamador completo privadamente. La captura REA
del helper libre falló con `cleanup_incomplete` al verificar propiedad de sus
procesos (`EACCES`), por lo que no se cuenta como evidencia REA completada. Se
inspeccionaron ambos helpers directamente con GNU objdump, incluidos NOPs finales,
y se verificó el ELF contra el digest de la inspección válida del padre. El
diagnóstico y el intento fallido quedan privados. REA no descifra ni realiza
el contraste numérico; este incidente no afecta a los oráculos GNU.

```sh
python3 scripts/verify-drwave246-poly-explicit-pitch.py \
  --ldr /privado/OP1_vdk.ldr --assembler "$POCKET_BFIN_AS" \
  --linker "$POCKET_BFIN_LD" --simulator "$POCKET_BFIN_SIM" \
  --testutils "$POCKET_BFIN_TESTUTILS" --output /privado/poly-explicit-nuevo
cmake --build build-dsp --target drwave246_poly_explicit_pitch_tests
ctest --test-dir build-dsp -R '^drwave246_poly_explicit_pitch$' --output-on-failure
```

[Preparación DSP](new-account-dsp-setup.md) ·
[Formato propio](../Tests/ReferenceVectors/drwave246-poly-explicit-pitch-format.md) ·
[Evidencia y hashes](firmware/drwave246-poly-explicit-pitch-validation.json).

Base revisada **`8cabef5`**, PR #30/#31 fusionadas. La rama reservada
`collab/voice246-note-dispatch`, observada en `d1e85c1`, investiga el dispatcher
exterior con metadata/pitch cero. No se ejecuta ese dispatcher, constructor,
retirada o render, ni se usan o modifican sus archivos. No se tocan helpers,
plan, cobertura o README común; CMake solo añade tres líneas para esta suite.
Plugin, IDs MIDI, automatización, sesiones, presets, bancos y UI permanecen
intactos. Firmware, instrucciones, desensamblados y ELF privados. Código propio
AGPL-3.0-only; herramientas con atribuciones en [CREDITS](../CREDITS.md).
