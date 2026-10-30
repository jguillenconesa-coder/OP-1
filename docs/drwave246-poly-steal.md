# Dr. Wave rev246: seis voces ocupadas y solicitud de reinicio

Se amplía la ejecución del llamador **`0x01a84604`** a su rama polifónica con
seis voces ocupadas, que llama al helper **`0x01a844d8..0x01a84604`**. La elección
por contador, solicitud ADSR, desvinculación y nueva entrada se ejecutan dentro
del firmware original. No se parchea ni se extrae la rama del llamador.

Es una **solicitud de reinicio y reasignación**, no una demostración de que el
nuevo pitch haya llegado al renderer de Dr. Wave. El consumidor de los campos
pendientes y los productores de buffers siguen sin recuperarse.

## Contrato observado

Se conservan modos externo/per-sonido cero, ADSR ready uno y auxiliares Core
nulos de la [ruta de primera voz libre](drwave246-poly-first-free.md). Las notas
construidas están en 0–127 y su pitch explícito es cero. Las entradas nuevas se
ensayan con contador menor de 128; los retornos por flag/duplicado también pueden
tener 128 entradas.

El byte de flag no nulo retorna antes de consultar actividad/contadores. Un
argumento cuya palabra de nota ya aparece en la lista también retorna antes de
esa consulta, independientemente de su voz asignada. Se ejercitan esos retornos
con actividad cero y contadores negativos: no se fuerza la rama de error.

Con nota nueva y las seis actividades no nulas, el llamador recorre las voces
en orden y mantiene un máximo **signed32** inicializado a **-1**. Solo cambia la
voz elegida cuando el contador de offset 4 del ADSR es estrictamente mayor.
Por tanto:

- Gana el mayor contador no negativo interpretado como signed32.
- Los empates conservan la primera voz encontrada.
- `0xffffffff` y `0x80000000` no son mayores por tener bits altos; se descartan
  frente a los valores no negativos.
- Si todos los contadores son negativos, la voz queda en -1 y se alcanza una
  ruta de assert. Ese contexto de nota nueva queda fuera del oráculo numérico;
  el adaptador lo rechaza antes de mutar, sin inventar una voz de fallback.

No se deduce de esos contadores una duración física o una cadencia original.

## Helper original de reinicio y lista

`0x01a844d8` prepara el pitch de la nota desde la tabla de 128 palabras y el
coeficiente de afinación explícito, conservando Q31 truncado y desplazamiento
saturado de un bit. Convierte la mitad baja signed16 de velocidad mediante
shift saturado de ocho bits. Después llama a `0x019120fc` con esos valores:

| Campo ADSR | Resultado antes del render |
| --- | --- |
| Etapa / contador | 6 / 0. |
| Offset 12 | Palabra de pitch preparado. |
| Offset 16 | Bits uint16 de velocidad preparada desplazados 16 bits; mitad baja cero. |
| Nivel y ganancia | Conservados completos, incluida la mitad alta de ganancia. |

No se vuelve a calcular aquí la ganancia `1 - nota/256` de la primera voz libre.
Tampoco se llama a Core note-on de Dr. Wave ni se escriben sus fases, pitch,
velocidad o actividad registrados. El sonido cargado/seleccionado no actúa como
el gate de aceptación de la ruta libre en este helper. El despacho auxiliar se
ejecuta pero retorna sin callbacks en el contexto de punteros nulos.

El helper busca la **primera** entrada cuya palabra de voz coincida con la
elegida y escribe `0xffffffff` en su offset 16. No elimina esa entrada, no
cambia su nota ni invalida las otras coincidencias; el byte de metadata no
filtra esta búsqueda. Si no encuentra dueño, igualmente añade la nueva entrada.

Después añade nota, velocidad, byte de metadata, pitch explícito y voz al final
de la lista, conservando los bytes de relleno 9–11 del destino, e incrementa el
contador. En esta cadena, el constructor original produce pitch explícito cero;
la rama del helper para un pitch explícito no nulo no se presenta como validada
por este nuevo ensayo.

`Source/DrWave246PolySteal.h` implementa ese contrato mediante los helpers
numéricos y ADSR ya contrastados. Sus estados de resultado y guards son propios,
no retornos semánticos del original. Presupone modos cero, ADSR inicializado y
auxiliares ausentes; no valida esos globales como parte de su API.

## Ensayo, secuencias y memoria

- **768 registros**: 512 casos independientes y cuatro streams de 64 llamadas.
- **320 nuevas entradas**, incluyendo las seis voces, máximos signed, extremos
  y **126 empates** del máximo.
- **247 desvinculaciones de un dueño existente**; 100 casos tienen dueños
  duplicados y preservan las coincidencias posteriores. En otros 73 casos no
  hay dueño previo y se conserva la lista anterior al añadir la nueva entrada.
- **256 retornos por flag y 192 por duplicado**. Los streams incluyen duplicados
  de notas insertadas antes, aunque su asignación previa pueda haberse retirado.
- Velocidades low16 dirigidas: 0, 1, 127, 128, 255, 32767, 32768 y 65535, con
  bits altos arbitrarios. El argumento y la lista conservan la palabra completa;
  solo el productor de offset 16 aplica la conversión observada.
- **98.304 posiciones ADSR**: **94.720 escritas y 3.584 preservadas**. Tras una
  solicitud se ensaya la rampa de etapa 6 → 7, seguida de llamadas inhibidas por
  flag y un duplicado para permitir la continuación del renderer. Su orden es
  explícito; no se presenta como un reloj original.

Se comparan **4.568 bytes completos antes y después del render**: lista/contador
y vecinos, seis objetos Dr. Wave, seis ADSR/scratch y guardas, registros VoiceCtl,
parámetros, RNG/valor anterior, selección, punteros y auxiliares, afinación,
discriminador y ajustes con vecinos. También se captura la tabla de ajustes
post-setter, argumento, otras entradas, salida/guardas y retorno ADSR. La tabla
de notas se compara al principio y al final.

El harness suministra **actividad, seis contadores ADSR y byte de flag por
llamada** para recorrer los casos acotados. Retiene lista, niveles, ganancias,
palabras pendientes, otros registros, objetos y RNG entre llamadas del stream.
No se infiere el productor original de actividad o contadores. Los objetos
Dr. Wave contienen vtables válidas y otros campos de estrés, no estados listos
para su render. La imagen vigilada no cubre toda la RAM, pila o CPU del aparato.

Setters y constructor originales preceden al llamador, y un ADSR explícitamente
seleccionado lo sigue. Ese orden es del harness; el enlace interno de selección,
reinicio y lista sí pertenece al llamador original. El discriminador del
constructor se varía como entrada, sin demostrar que el dispatcher global
seleccionase este motor en todos esos contextos.

## Validación, límites y coordinación

Dos reproducciones privadas coinciden byte a byte. Pasan **49 suites de núcleo**
y **51 con JUCE** en Linux, además de ASan/UBSan del nuevo comparador
(`detect_leaks=0` por el límite ptrace). El oráculo de consulta de última voz de
la otra cuenta reproduce sus fixtures sin cambios con nuestro firmware/herramientas.
Windows/macOS quedan pendientes de CI; no se valida DAW, ASIO ni hardware físico.

REA 6.8.0 inspecciona privadamente el helper completo con `inspect-with-objdump`,
sin truncamiento ni cambio de artefacto. La comparación numérica usa GNU BF524
con ambos parches documentados y preflight halfword. No se atribuye descifrado o
validación de muestras a REA. Firmware, instrucciones, desensamblados y ELF
permanecen privados.

El renderer ADSR conserva las palabras pendientes en estos casos. **No se añade
un consumidor supuesto** ni se escribe el buffer de pitch para hacer sonar el
nuevo valor. Siguen pendientes ese relevo, productores de buffers, otros modos,
auxiliares presentes, scheduler, mezcla y salida. No se ejecuta audio Dr. Wave
ni se cambia MIDI, automatización, sesiones, presets o UI.

La otra cuenta tiene reservado el despachador de retirada `0x01a85840`, con mapa
de notificación vacío como contexto inicial. Esta tarea no lo ejecuta ni cambia
sus archivos. La consulta `0x01a863a8` ya está fusionada y no se vuelve a
reconstruir aquí.

```bash
python3 scripts/verify-drwave246-poly-steal.py \
  --ldr /privado/OP1_vdk.ldr --assembler /herramientas/bfin-as \
  --linker /herramientas/bfin-ld --simulator /herramientas/run-halfword-v2 \
  --testutils /fuentes-gdb/sim/testsuite/bfin --output /privado/poly-ocupadas-nuevo
cmake --build build-dsp --target drwave246_poly_steal_tests
ctest --test-dir build-dsp -R '^drwave246_poly_steal$' --output-on-failure
```

Fixtures: 768 registros de **4.880 bytes de entrada / 9.500 de resultado**, más
tabla de notas de 512 bytes al principio/final de la salida. El formato conserva
las dos imágenes de 4.568, salida/guardas de 272, retorno ADSR de 4, ajustes
post-setter de 48 y cabecera final/argumento de 40 bytes.

[Evidencia](firmware/drwave246-poly-steal-validation.json) ·
[Primera voz libre](drwave246-poly-first-free.md) ·
[Transiciones ADSR](cluster246-envelope-transitions.md) ·
[Plan](firmware-dsp-workplan.md).
