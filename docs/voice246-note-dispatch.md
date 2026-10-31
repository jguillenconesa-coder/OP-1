# Rev246: despacho exterior de activación sin notificación

Se contrasta la función completa **`0x01a85750..0x01a85840`**, 240 bytes,
en el recorrido de **discriminador `0x01de4c40 = 0`**, metadata cero y pitch
explícito cero. Delega en `0x01a84604`, ya contrastado para lista vacía/no vacía.
Este bloque añade la preparación exterior, sin render ni transporte.

La base es `8cabef5`, con PR #30 de [secuencias on/off](drwave246-note-lifecycle.md)
y PR #31 de [pitch explícito con lista vacía](drwave246-empty-explicit-pitch.md)
fusionadas. Una reproducción privada independiente de los 3.462 casos de PR #31
coincide con sus fixtures, incluidos 1.491 pares con lookup; no se amplía aquí
ese contrato ni se modifican sus archivos.

## Contrato observado

R0 apunta a un argumento raw de 16 bytes: nota, velocidad, metadata/padding y
pitch explícito. Se prueban notas **signed32 0–127**. Fuera de ese rango, la
inspección muestra un camino de assert, **no un retorno filtrado** como el del
dispatcher de retirada. Ese camino queda excluido del oráculo. Las protecciones
del adaptador rechazan el contexto sin emular dicho assert.

Con nota válida, antes de mirar el discriminador o llamar al recorrido interior:

1. Escribe **un byte 1** en `0x01dfe4dd`. No escribe una palabra completa.
   La función posterior de ese indicador no se identifica aquí.
2. Si la palabra completa de velocidad es **`0xffffffff`**, la sustituye por
   `0x0000007f` **en el argumento fuente**. No normaliza otras palabras cuyo
   halfword bajo sea `0xffff`, como `0x1234ffff`.
3. Con discriminador cero copia por campos a un argumento local de pila:
   palabras completas en offsets 0/4/12 y un byte en offset 8; conserva los
   bytes previos 9–11 del destino. Llama al recorrido interior con esa copia.

Después de la llamada interior vuelve a leer el byte de metadata del argumento
fuente. Si es cero retorna. Si no es cero llama a **`0x019e2e64`** con nota y
velocidad: **ese recorrido de notificación no se ejecuta ni se reconstruye**.
Los discriminadores drum/otros y sus errores también quedan fuera.

La normalización y el byte global ocurren incluso cuando el gate o la búsqueda
de duplicados del llamador interior suprimen la activación. Esos retornos no
restauran el argumento o indicador anterior. El argumento fuente conserva todos
sus otros bytes y ambas guardas; el local refleja la velocidad normalizada.

R1 se suministra con cinco patrones raw (0, 1, `0x7fffffff`, `0x80000000`,
`0xffffffff`). La inspección del recorrido de discriminador cero no muestra
consumo semántico de ese valor y las capturas coinciden con el adaptador sin él.
No se extrapola esa independencia a la rama drum que sí lo utiliza.

## Contexto interior e implementación

`Source/Voice246NoteDispatch.h` envuelve los helpers existentes de
[lista vacía](drwave246-note-modes.md) y [lista no vacía](drwave246-held-note-modes.md),
sin cambiar sus fórmulas. El contexto exige selectores 1–3, modo externo cero,
seis ADSR previamente listos, objetos Core Dr. Wave válidos, seis auxiliares
nulos y secundario nulo. Afinación, sonido cargado/seleccionado, controles,
actividad, gate y orden de llamadas son explícitos.

Desde lista vacía puede preparar ADSR/iniciar Core o solicitar reinicio según
actividad; con lista no vacía conserva el reparto de pitch directo del selector
2 frente a reinicio pendiente del 1/3. El indicador y normalización exteriores
no se confunden con aceptación Core ni con voz libre. Los nuevos estados se
comparan completos con el original; no se añade un relevo propio de pendientes.

El enum y guards son políticas del adaptador, **no valores semánticos del R0
original**. Solo admite metadata cero, pitch cero, discriminador cero y notas
válidas; excluye nuevas entradas a capacidad completa. El original puede mutar
antes de su assert de capacidad; no se emula ese camino como fallo transaccional.
Gate/duplicados a capacidad completa sí se cubren dentro del contexto válido.

El constructor habitual ya contrastado emite metadata 1: **no se incluye en este
ensayo ni se borra su byte para simular una cadena completa**. Aquí los argumentos
raw se suministran explícitamente con metadata cero desde el inicio. La ruta
habitual de notificación sigue pendiente.

## Contraste numérico y cobertura

Son **1.536 llamadas desde la entrada completa**: 1.152 independientes y seis
secuencias de 64. Incluyen:

- 1.056 inserciones: 355/349/352 en selectores 1/2/3, las 128 notas en cada uno.
- 384 entradas con lista vacía y entradas no vacías hasta el límite 128;
  240 retornos por gate y 240 por duplicado, con comparación de nota completa.
- 308 palabras de velocidad `0xffffffff`, normalizadas en el origen, frente a
  patrones con bits altos que se conservan. La posterior conversión de velocidad
  sigue usando el halfword signed16 bajo según el helper interior.
- Actividad inicial/per-call 0/1/128/255, coincidencia y rechazo Core, seis voces,
  coeficientes de afinación explícitos de signos/extremos y padding distinto
  entre argumento fuente y local.

Se comparan **4.568 bytes** de regiones seleccionadas: lista/contador/gate/vecinos,
seis ADSR/scratch/guardas, seis Dr. Wave, VoiceCtl, RNG/vecinos, ocho controles de
sonido, punteros y globales, tabla de ajustes con vecinos (incluye `0x01dfe4dd`).
Se comparan además **32 bytes** de argumento fuente con guardas y **16 bytes**
de copia local, incluyendo su padding preservado.

El harness precarga la copia local con patrones propios. Un probe propio con
`LINK 0x10` calibra `FP-0x10 = 0x02ffffe8` con SP `0x03000000`. Es dirección del
ensayo, no un buffer fijo del aparato. Pila fuera de esos 16 bytes, registros CPU
y RAM no enumerada no se presentan como estado completo de máquina verificado.

En las secuencias se retienen lista, pools, registros y RNG; **gate y actividad
se suministran cada vez**, además de controles/afinación/selección. No se recupera
el productor de actividad ni el clock. No hay render ADSR o Dr. Wave en este bloque.

Pasan **55 suites de núcleo Linux**, **57 con JUCE/plugin** y el comparador
bajo ASan/UBSan (detección de fugas desactivada por la restricción ptrace del
entorno). Windows/macOS quedan para CI, sin atribuir ejecución local.

Dos reproducciones privadas coinciden byte a byte. GNU BF524 utiliza ambos
parches documentados y preflight halfword. Watches PC, con controles positivos
y negativos propios, excluyen polifonía, notificación, rama drum, asserts y L1.
REA/objdump inspecciona la función completa bajo símbolo sintético; no descifra
ni proporciona el contraste numérico.

## Formato y reproducción

Entrada: 1.536 registros de **4.608 bytes** (cabecera/argumento/guardas 40 + imagen
4.568). Cabecera +0 marca reset; +4 clasifica el contexto; +12 suministra R1;
+16 empieza el argumento de 16 bytes; las guardas están en +8 y +32.

Salida: tabla de notas de 512 bytes, 1.536 registros de **4.616** y tabla de 512.
Por registro: imagen 4.568, argumento fuente con guardas 32 y copia local 16.
Regiones/hashes/herramientas constan en
[la evidencia](firmware/voice246-note-dispatch-validation.json).

```sh
python3 scripts/verify-voice246-note-dispatch.py \
  --ldr /privado/OP1_vdk.ldr \
  --assembler "$POCKET_BFIN_AS" --linker "$POCKET_BFIN_LD" \
  --simulator "$POCKET_BFIN_SIM" --testutils "$POCKET_BFIN_TESTUTILS" \
  --output /privado/note-dispatch-nuevo
```

Salida nueva, privada y externa al checkout, conforme a
[la preparación entre cuentas](new-account-dsp-setup.md). Firmware, instrucciones,
desensamblados y ELF permanecen privados. Los fixtures contienen datos numéricos
del ensayo propio.

## Límites y continuación

Siguen pendientes notificación de activación habitual, consumidor L1 ADSR,
productor del buffer de pitch, scheduler, mezcla y salida. El bloque no cambia
plugin, MIDI/automatización/sesiones, UI, presets o bancos, ni demuestra latencia,
hardware, Ableton o ausencia de dropouts.

La otra cuenta tiene reservado **selector polifónico 0 con pitch explícito no
cero**, en `collab/drwave-poly-explicit-pitch`. No se duplica ese contexto aquí.
