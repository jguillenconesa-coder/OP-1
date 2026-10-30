# Rev246: inicialización Core de la imagen de trabajo y límite del llamador

La búsqueda del llamador que prepara `0xff801724` identifica un inicializador
SDRAM completo **`0x01968774..0x019687c0`**, reconstruido en
`Source/Core246ControlWorkspace.h`. Se contrasta aislado y seguido del
[callback tipo 2 y sus renderers](drwave246-control-env-dispatch.md).

**Es inicialización a cero. No recupera la recarga de parámetros por llamada
ni el scheduler original.** Las referencias candidatas de esa recarga siguen
junto a franjas L1 protegidas; no se ejecutan ni se completan con constantes.
La reconstrucción permanece aislada del plugin.

## Contrato completo del inicializador

El original hace tres operaciones fijas:

| Memoria | Escritura |
| --- | --- |
| `0xff8016dc` | Cero en una palabra de 16 bits; conserva la palabra vecina en `0xff8016de`. |
| `0xff801724` | Ocho palabras de 16 bits a cero, incluida la imagen de trabajo común ya observada. |
| `0xff801774` | Otras 32 palabras de 16 bits a cero; no se atribuye todavía una función comercial a este segundo workspace. |

Las dos matrices se inicializan mediante el helper original `0x01aa3862` y el
callback de una palabra `0x01968768`. Se ejecuta **toda** la rutina inicializadora
sin sustituir esos helpers. La huella es de **82 bytes** dentro de una imagen
observada de **228 bytes**, desde `0xff8016d8` hasta `0xff8017bc` (fin exclusivo).
Los otros **146 bytes** se conservan, incluidos huecos, tipos/vecinos y guardas.
No se limpia una estructura Core completa ni se reinician voces por deducción.

Hay una referencia estática al inicializador en `0x01000110`. Esa referencia no
prueba cuándo se ejecuta en el aparato: no se reproduce el arranque original.
La API propia actúa sobre una imagen de bytes independiente; no accede a MMIO
del ordenador ni se incorpora al motor de audio.

## Dos oráculos

El ensayo aislado incluye **4.096 estados independientes y 256 llamadas
continuas**. Se precargan patrones cero/A5/FF y datos distintos; entre llamadas
continuas el harness modifica explícitamente un byte, recorriendo toda la imagen.
Se comparan los 228 bytes completos y, por separado, **40 bytes de registros de
pitch/guardas, 148 bytes del pool ADSR/guarda y ocho bytes de RNG/vecino**, que
el inicializador preserva. Las mutaciones y llamadas no representan un calendario
original de resets.

El ensayo encadenado ejecuta el inicializador original antes de cada uno de los
**1.506 callbacks** del perfil tipo 2. El callback y el renderer consumen la
imagen que el inicializador ha puesto a cero; **no se recargan valores de
parámetros calculados por el harness después de esa inicialización**. Se conserva
la matriz de 625 combinaciones por `kind` 0 y 1, con otras llamadas inactivas de
`kind` 2 y cuatro secuencias de 64 registros.

Este orden es explícito del llamador propio: **no significa que el firmware
inicialice la imagen en cada bloque**. Los buffers de pitch/volumen, selección,
contador, estados/volumen ADSR, fuentes y eventos siguen siendo entradas de
ensayo. No se usa la puesta a cero como sustituto del productor bloqueado.

Coinciden **91.648 muestras activas Dr. Wave** y **95.104 posiciones ADSR**:
**85.120 escritas y 9.984 preservadas** por inactividad. Se comparan además la
imagen Core completa después del callback/render, parámetros, rangos, fuentes,
mensajes, ambos buffers, suavizados, ambos pools, RNG/scratch/vecinos, tipos,
selección, flags, contador, configuración, ondas, cachés, incrementos y bancos.
La salida ADSR sigue en un buffer propio; no se identifica como productor del
buffer de volumen ni se inventa una mezcla de salida.

Ambos ensayos se reproducen en dos directorios privados y coinciden byte a byte.
Pasan **40 suites de núcleo y 42 con JUCE en Linux**, más ASan/UBSan de ambos
comparadores (LeakSanitizer desactivado por ptrace). Windows/macOS quedan para CI;
Wine no se utiliza aquí. REA 6.8.0 aporta inspección estática del inicializador
SDRAM; GNU BF524 con los parches documentados ejecuta los oráculos. No se valida
hardware, DAW, latencia ni ausencia de dropouts.

## Referencias del llamador: evidencia acotada

El inventario reproducible busca cargas inmediatas y punteros; no es una búsqueda
exhaustiva de direcciones calculadas ni un análisis completo de flujo. El callback
`0x019cce68` aparece como puntero en `0x01910840`. Las referencias SDRAM a la
imagen común incluyen el inicializador y modificadores ya localizados; estas
observaciones no recuperan su recarga original por llamada.

En el bloque L1 inventariado —cabecera `0x206214`, destino `0xffa00000`,
49.076 bytes, flags `0xaddb2406`— aparecen:

| Candidato | Alcance de la observación |
| --- | --- |
| `0xffa006ca` | Par de mitades de dirección visible compatible con `0xff801724`; tramo vecino protegido en `0xffa006c0..c8` y `0xffa006d8..e0`. |
| `0xffa009ce` | Otro par visible; tramo vecino protegido en `0xffa009c0..c8` y `0xffa009d8..e0`. |
| `0xffa01d22` | Solo la mitad baja es visible completa; no confirma la dirección, porque la mitad alta cruza `0xffa01d28..30`. |

Los rangos se interpretan con fin exclusivo y el perfil comunitario ya documentado
—primeros ocho bytes de cada grupo de 24 protegidos—. Incluso los pares completos
**no demuestran** origen de copia, dirección de transferencia, llamador ni orden.
No se ejecuta L1, no se descifra y no se pide hardware ni un volcado descifrado.
El script de inventario solo publica direcciones/supuestos, sin instrucciones.
[Inventario](firmware/core246-workspace-reference-audit.json).

## Reproducción

El ensayo encadenado exige ambos parches GNU y comprueba el probe halfword.
En un setup nuevo usa `--halfword-writeback` según la
[preparación entre cuentas](new-account-dsp-setup.md).

```sh
python3 scripts/audit-core246-workspace-references.py \
  --ldr /privado/OP1_vdk.ldr --output /privado/referencias-nuevo
python3 scripts/verify-core246-control-workspace.py \
  --ldr /privado/OP1_vdk.ldr --assembler "$POCKET_BFIN_AS" \
  --linker "$POCKET_BFIN_LD" --simulator "$POCKET_BFIN_SIM" \
  --testutils "$POCKET_BFIN_TESTUTILS" --output /privado/workspace-nuevo
python3 scripts/verify-drwave246-control-workspace.py \
  --ldr /privado/OP1_vdk.ldr --assembler "$POCKET_BFIN_AS" \
  --linker "$POCKET_BFIN_LD" --simulator "$POCKET_BFIN_SIM" \
  --testutils "$POCKET_BFIN_TESTUTILS" --output /privado/workspace-cadena-nuevo
```

Salidas nuevas y externas al checkout. `--record` en cada verificador regenera
solo sus propios fixtures numéricos. [Inicializador](firmware/core246-control-workspace-validation.json) ·
[Cadena](firmware/drwave246-control-workspace-validation.json).
Firmware, instrucciones, desensamblados y ELF permanecen privados.

Se mantiene Dr. Wave activo. La preparación por llamada queda bloqueada con estas
referencias inventariadas; no se repetirá como si fuera un productor recuperado.
El siguiente contrato legible puede centrarse en liberación de notas/ADSR desde
VoiceCtl. Productores iniciales de buffers, campos ADSR pendientes, asignador,
reloj y mezcla/salida siguen pendientes antes de integrar el instrumento.
