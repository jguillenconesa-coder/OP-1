# Rev246: eventos con metadata original y salida explícitamente deshabilitada

Se contrasta el recorrido **constructor → dispatcher exterior on/off →
notificación → ADSR**, conservando el byte de metadata **1** que emite el
constructor original. La palabra de salida `0x01dfe03c` se suministra como
**cero**. Es una precondición explícita del ensayo, no un valor de fábrica
recuperado ni un sustituto del transporte.

La base es `41ad48b`, con [PR #33, despacho metadata cero](voice246-note-dispatch.md)
y [PR #32, pitch explícito polifónico](drwave246-poly-explicit-pitch.md)
fusionadas. La PR #32 también se reproduce privadamente desde nuestro LDR:
4.552 casos coinciden con sus fixtures, sin modificar sus helpers o archivos.

## Dependencias legibles y contrato de notificación

| Entrada completa | Fin exclusivo | Contexto contrastado |
| --- | --- | --- |
| `0x019e2e64` | `0x019e2f84` | Notificación on, nota 0–127, palabra de salida cero. |
| `0x019e2dc0` | `0x019e2e64` | Notificación off, nota 0–127, palabra de salida cero. |
| `0x01a69360` | `0x01a6936c` | Getter que devuelve la palabra completa de `0x01dfe03c`; aquí se ejecuta con cero. |

El mapa está en **`0x01aafd60`**, un byte por nota. La notificación on examina
la **palabra completa de velocidad**, después de la normalización exterior de
`0xffffffff` a 127:

- Velocidad no cero y entrada cero: escribe **1** en esa entrada y consulta el
  getter de salida. Al devolver cero retorna antes de preparar paquetes.
- Velocidad no cero y entrada no cero: conserva exactamente el byte, incluidos
  patrones 128/255, y retorna sin consultar salida.
- Velocidad cero y entrada no cero: escribe cero y consulta salida; con salida
  cero retorna. Con entrada ya cero retorna directamente.

La notificación off deja la entrada a cero; si ya era cero retorna antes de
consultar salida. No cambia otras notas. La rama de velocidad cero del on
solo describe la notificación: **el dispatcher interior sigue siendo on** y
no se sustituye su DSP por una retirada.

Los dispatchers exteriores llaman a notificación después del recorrido
interior si metadata es no nula. Por ello el mapa puede cambiar con un on
bloqueado por gate, un duplicado suprimido o una retirada ausente. No se hace
rollback del mapa según la aceptación DSP/Core. El indicador byte y la
normalización de velocidad de PR #33 siguen ocurriendo antes del on interior.

Los caminos con salida no cero que preparan paquetes, escriben el indicador
`0x018b600d` o llaman a `0x019e0d18` **no se ejecutan ni se implementan**. No se
parchean instrucciones o retornos. El getter original lee la palabra cero
suministrada; la imagen conserva esa palabra y ambos vecinos.

## Secuencias y estado conservado

Son **12 secuencias de 128 eventos**, cuatro por selector 1–3, con el mismo
ciclo A/B/C/X del [ensayo de eventos previo](drwave246-note-lifecycle.md):
on A, on A duplicado, on B, off A, on C, off C, off B, off X ausente. Se ejecuta
un render ADSR explícito de 128 muestras después de cada evento.

Nueve secuencias permiten la activación interior. En tres se suministra gate
no nulo en cada on para comprobar notificación incluso con DSP bloqueado.
**Cada off requiere gate interior cero**: la rutina original entra en assert
si ese gate no es cero. Una watch del helper de error detectó esa precondición
al preparar el ensayo; la salida fallida se mantuvo privada y no es un fixture.
El gate se suministra por evento conforme a ese contrato, sin eludir el assert
con instrucciones inventadas.

Durante cada secuencia se conservan sin overrides **lista/contador, actividad,
pitch/velocidad, seis ADSR, seis Dr. Wave, RNG, mapa, palabra de salida y
monitores de transporte**. El mapa inicial usa bytes 0/1/128/255, con guardas
propias. No se fuerza a cero entre eventos. Controles, afinación, selección,
gate interior, voz/volumen de render y orden son entradas explícitas.

Ocho setters originales inicializan ajustes cero tras poison A5 al empezar
cada secuencia; un setter original mantiene un ajuste a cero por evento.
El constructor original precede todos los on/off: produce notas 0–127,
**metadata 1 y pitch explícito cero**. El harness no borra metadata ni escribe
un pitch nuevo tras el constructor. En 96 on se suministra la velocidad raw
`0xffffffff`; otros 96 on tienen palabra de velocidad completa cero.

Los seis ADSR están ready=1, hay Core Dr. Wave válido y auxiliares nulos. La
inicialización diferida del pool está reservada a la otra cuenta y no se duplica.
La actividad no se limpia al vaciar la lista ni se inventa el consumidor de
pitch/velocidad pendientes en L1.

## Implementación y contraste

`Source/Voice246QuietNoteDispatch.h` implementa los dos efectos de mapa con
salida cero y compone los helpers existentes de lista vacía/no vacía y retirada.
Conserva la metadata fuente y copia por campos al argumento local, sin cambiar
los helpers anteriores. Guards, enums y booleanos son políticas del adaptador,
no valores de retorno originales o emulación de asserts. El discriminador cero,
selectores 1–3, afinación explícita, pool listo y salida cero delimitan el contexto.
Una salida no cero se rechaza incluso cuando algún retorno temprano original
podría evitar consultarla: no se amplía el contrato probado.

Las **1.536 llamadas originales** coinciden con la composición independiente:

- 768 on: **432 inserciones, 192 bloqueados por gate y 144 duplicados**.
- 768 off: **432 retiradas y 336 búsquedas ausentes**.
- **999 cambios de byte en el mapa** a lo largo de las secuencias.
- Las 128 notas en los on de cada selector, seis voces de render y estados
  completos intercalados. La lista acaba vacía tras cada ciclo.
- 196.608 posiciones ADSR: **156.032 escritas / 40.576 conservadas**.
  No se añaden muestras Dr. Wave a su total de quince oráculos.

Se comparan imágenes de **4.760 bytes** antes y después del render: las regiones
anteriores, mapa/vecinos, monitores `0x01aadb1c` (40 bytes) y `0x018b600c` (4),
y palabra de salida con vecinos (12). Los monitores de transporte no cambian.
Además se compara argumento fuente completo, snapshot de ajustes, muestras,
guardas de audio, retorno ADSR y **16 bytes de la copia local del evento**,
capturados antes de que el render reutilice la pila. El padding de cada destino
se precarga con un patrón distinto del argumento fuente y se conserva.

Un probe propio calibra el local `FP-0x10 = 0x02ffffe8` con SP `0x03000000`.
No se presenta como buffer fijo del aparato ni como comprobación de toda la pila,
RAM o registros CPU. Dos reproducciones privadas coinciden byte a byte bajo GNU
BF524 con ambos parches documentados y preflight halfword. Watches PC verificadas
con un programa propio detienen preparación de paquetes, transporte, polifonía,
discriminadores excluidos, errores y L1. REA/objdump inspecciona las tres
funciones completas bajo símbolos sintéticos; no descifra ni valida números.

Pasan **57 suites CTest de núcleo Linux**, **59 con JUCE/plugin** y el nuevo
comparador bajo ASan/UBSan (detección de fugas desactivada por restricción ptrace
del entorno). Windows/macOS quedan para CI, sin atribuir ejecución local.

## Formato y reproducción

Entrada: 1.536 registros de **5.072 bytes**: cabecera/argumento 40, imagen 4.760,
audio/guardas 272. Salida: tabla de notas de 512, 1.536 registros de **9.900**,
tabla final de 512. Cada registro contiene dos imágenes, audio/guardas 272,
retorno 4, snapshot de ajustes 48, cabecera/argumento 40 y copia local 16.
Regiones, herramientas y hashes están en
[la evidencia](firmware/voice246-quiet-note-lifecycle-validation.json).

```sh
python3 scripts/verify-voice246-quiet-note-lifecycle.py \
  --ldr /privado/OP1_vdk.ldr \
  --assembler "$POCKET_BFIN_AS" --linker "$POCKET_BFIN_LD" \
  --simulator "$POCKET_BFIN_SIM" --testutils "$POCKET_BFIN_TESTUTILS" \
  --output /privado/quiet-lifecycle-nuevo
```

Destino nuevo, privado y externo al checkout, conforme a
[la preparación](new-account-dsp-setup.md). Firmware, instrucciones,
desensamblados y ELF no se publican. Los fixtures son datos del ensayo propio.

## Límites y continuación

No se completa el instrumento: siguen pendientes consumidor L1 de ADSR,
productor del buffer de pitch, scheduler, mezcla/salida y transporte con salida
habilitada. No cambia plugin, MIDI/automatización/sesiones, UI, presets o bancos.
No se afirma temporización, hardware, Ableton o ausencia de dropouts.
La otra cuenta reserva **`0x0191264c`, acceso e inicialización diferida ADSR**,
en `collab/adsr246-lazy-pool`.
