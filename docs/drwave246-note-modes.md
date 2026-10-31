# Rev246: activación de selectores 1–3 con lista vacía

Se contrasta la entrada completa **`0x01a84604..0x01a84ba0`**, únicamente con
lista vacía, modo externo VoiceCtl cero, selectores por sonido **1–3**, ADSR
listos y módulos auxiliares ausentes. Se incluyen el gate de un byte y el
retorno del selector 4. No se asignan nombres comerciales a los selectores.
Las ramas polifónicas previas se conservan en [primera voz libre](drwave246-poly-first-free.md)
y [voz ocupada](drwave246-poly-steal.md).

## Contrato observado

El gate `B[0x01dfe730]` no nulo retorna antes de seleccionar modo. Con gate cero,
la búsqueda de duplicados precede al selector; **la lista de este ensayo está
explícitamente vacía en cada entrada**, por lo que no se contrasta aquí una
búsqueda presente. El selector procede del signed16 del sonido seleccionado,
offset 8 de `0x01b60ef0 + seleccionado × 16`, desplazado aritméticamente 12 bits.
El selector 4 no modifica el estado. Los selectores 1/2 recorren la voz 0 y el 3
recorre las seis voces en orden ascendente.

El argumento tiene 16 bytes. Este ensayo ejecuta primero los setters originales
y el constructor `0x01a85014`, que produce un índice final 0–127 y **pitch
explícito cero**. El pitch base usa la tabla de 128 notas y el coeficiente de
afinación explícito del índice 1.200: producto Q31 y desplazamiento saturado de
un bit. No se contrasta en este bloque un argumento con pitch explícito no cero.

La rama elegida para cada voz depende de su byte de actividad, sin exigir que
las demás voces tengan una actividad coherente con la lista vacía:

| Actividad al entrar | Efectos observados |
| --- | --- |
| Cero | `noteOn` ADSR y escritura del halfword bajo de ganancia; despacho auxiliar nulo y solicitud Core de inicio. Si Core acepta, resetea Dr. Wave, avanza RNG y escribe pitch, velocidad y actividad 1. |
| No cero | Avanza RNG, escribe velocidad, solicita reinicio ADSR y recalcula la ganancia. Conserva pitch/actividad VoiceCtl y todo el objeto Dr. Wave. No consulta la aceptación de inicio Core. |

La ganancia baja es `32767` para nota cero y `32768 − nota × 128` en las demás
notas probadas; conserva el halfword alto. Equivale aquí al recorrido original
binary32 `1 − nota/256` y su conversión Q15, con entradas 0–127 exactamente
representables. La velocidad es el halfword bajo signed16 desplazado ocho bits
con saturación signed16; los bits altos de la palabra fuente se conservan en
la lista, pero no participan en esa conversión.

Core acepta cuando sonido cargado y seleccionado coinciden. Un rechazo **no
revierte** `noteOn` ni ganancia ADSR y no avanza RNG ni escribe registros de esa
voz. En una voz ya activa se produce el reinicio pendiente incluso si los
sonidos cargado/seleccionado difieren.

Por cada voz que avanza RNG se aplica `seed × 1664525 + 1013904223` módulo 2³².
Se toma su halfword alto signed16 y se divide con redondeo hacia menos infinito:
**256 para el inicio de una voz inactiva, 512 para una voz ya activa**. Ese paso
multiplicado por el índice de voz y desplazado 16 bits, con wrap, da el desvío
Q31. El resultado de sumar al pitch base su producto Q31 con el desvío satura
signed32. La voz 0 también avanza RNG aunque su desvío sea cero.

En una voz activa, `adsr246RequestRestart` escribe etapa 6, contador cero,
pitch completo en `field12` y el patrón de velocidad en el halfword alto de
`field16`. Conserva nivel; la ganancia baja se recalcula después. **No consume
esos campos pendientes ni actualiza por ellos el buffer de pitch.**

Al terminar se añade una entrada de 20 bytes con voz **`0xfffffffe`**, incluso
si Core ha rechazado todos los inicios. Se copian nota y velocidad completas,
un byte de metadata y pitch explícito cero; se conservan los bytes de padding
9–11 y toda la cola de la lista. El contador pasa de cero a uno. El valor -2
es una etiqueta observada; no se interpreta como un índice de pool.

## Contraste numérico

`Source/DrWave246NoteModes.h` implementa ese contexto reutilizando los helpers
contrastados. El enum y contadores del adaptador son propios, no el retorno
original. Rechaza dependencias fuera del contrato; no emula sus asserts.

El ensayo tiene **768 llamadas**, 512 independientes y cuatro secuencias de 64:

- 576 inserciones, 128 retornos por gate y 64 por selector 4.
- Las 128 notas en cada selector 1–3, los seis índices y las 64 máscaras de
  actividad. Los bytes activos incluyen 1, 128 y 255.
- 705 inicios aceptados, 106 rechazados después de ADSR y 725 solicitudes de
  reinicio pendiente: 1.430 avances RNG en el llamador.
- Ocho patrones dirigidos de velocidad baja: 0, 1, 127, 128, 255, 32767, 32768 y
  65535, con bits altos independientes. Afinación y campos previos incluyen los
  patrones de estrés del generador preexistente; no son valores por defecto.
- Antes y después del render ADSR explícito, se comparan imágenes completas de
  **4.568 bytes**: lista/contador/vecinos, seis ADSR y scratch, seis Dr. Wave,
  VoiceCtl, controles, RNG, ajustes y globales seleccionados. Se comparan además
  argumento completo, snapshot del setter, retorno de finalización, 128 muestras
  y guardas del buffer. El render escribe 94.592 posiciones y conserva 3.712.

Las secuencias retienen estados, registros, RNG y lista/padding, pero **el
harness suministra contador cero, gate y los seis bytes de actividad antes de
cada llamada**. Eso permite contrastar repetición con estado acumulado sin
entrar en la rama de lista no vacía. No se ha recuperado el proceso original
que mantendría esa combinación de lista y actividad. El orden y la voz/volumen
del render ADSR también son entradas explícitas.

Pasan las **51 suites de núcleo Linux**, las **53 con JUCE/plugin** y el nuevo
comparador bajo ASan/UBSan (detección de fugas desactivada por restricción ptrace
del entorno). Dos reproducciones privadas desde el LDR inventariado coinciden
byte a byte.
El modelo es GNU BF524 con los dos parches documentados. Primero se comprueban
el probe halfword y las watches PC mediante un programa propio. Watches en el
camino de lista no vacía `0x01a84a70`, polifonía `0x01a84690`, helper de error y
rango L1 no se activan. REA/objdump inspecciona la función completa mediante
símbolos sintéticos; no descifra ni proporciona la validación numérica.

Formato: entrada de 4.880 bytes (cabecera 40, imagen 4.568, audio/guardas 272).
Salida: tabla de notas 512 bytes, 768 registros de 9.500 bytes y tabla 512.
Cada registro contiene imagen tras el llamador, imagen tras render, audio y
sus guardas 272, retorno 4, snapshot de ajustes 48 y cabecera/argumento 40.
Las direcciones, regiones, herramientas y hashes están en
[la evidencia](firmware/drwave246-note-modes-validation.json).

```sh
python3 scripts/verify-drwave246-note-modes.py \
  --ldr /privado/OP1_vdk.ldr \
  --assembler "$POCKET_BFIN_AS" --linker "$POCKET_BFIN_LD" \
  --simulator "$POCKET_BFIN_SIM" --testutils "$POCKET_BFIN_TESTUTILS" \
  --output /privado/note-modes-nuevo
```

La salida debe ser nueva, privada y externa al checkout. El script no descarga
ni publica firmware. Las rutas de preparación están en
[la guía entre cuentas](new-account-dsp-setup.md).

## Límites y siguiente bloque

La lista no vacía/selectores 1–3 queda reservada a la otra cuenta. El dispatcher
de retirada ya está contrastado en [PR #25](voice246-release-dispatch.md) y el
consumidor de última voz se [clasificó como gráfico](voice246-last-voice-consumer.md);
no se repiten esas investigaciones.

Siguen pendientes consumidor L1 de campos ADSR, productor del buffer de pitch,
scheduler, mezcla, salida e integración del instrumento. Este bloque no añade
muestras de audio Dr. Wave ni demuestra latencia/dropouts en Ableton o hardware.
No cambia plugin, presets, UI, IDs MIDI, automatización o sesiones. Las pruebas
Windows/macOS corresponden a CI; no se presentan como ejecutadas localmente.
