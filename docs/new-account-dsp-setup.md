# Continuar la investigación DSP desde otra cuenta

Un checkout de Tiny-one no incluye los archivos privados de otra sesión. Una
ruta `/workspace/...` tampoco los transfiere entre cuentas. Si el cliente no
permite descargar un archivo local del entorno anterior, no se necesita ese
artefacto: las herramientas pueden reconstruirse desde fuentes públicas y el
firmware debe adjuntarse directamente en la cuenta nueva.

## Entradas necesarias

- Checkout actualizado de `jguillenconesa-coder/Tiny-one`.
- Entorno Linux con Python actualizado (`tarfile.data_filter`), compiladores C/C++,
  Make, `ar`, `ranlib` y `patch`. No se necesita audio físico ni JUCE para los
  oráculos Blackfin.
- El `op1_246.op1` que aportó el propietario, adjuntado al nuevo chat o situado en
  una ruta privada de ese entorno. También se acepta el `OP1_vdk.ldr` ya extraído
  si coincide con el hash inventariado.

El repositorio es público. **No subir firmware, extracciones, ELF ni el paquete
privado de traspaso a commits, releases, PR o artefactos públicos.** Los scripts
de esta guía crean sus salidas fuera del checkout y no realizan subidas.

## 1. Reconstruir las herramientas Blackfin

Desde el checkout, elige un directorio de salida nuevo:

```bash
python3 scripts/setup-blackfin.py --output /workspace/tiny-one-blackfin --jobs 2
source /workspace/tiny-one-blackfin/blackfin-env.sh
```

El script descarga GNU Binutils **2.44** y GNU GDB **13.1** desde `ftp.gnu.org`
por HTTPS y comprueba sus SHA256. Conserva fuentes y builds separados de GDB:
stock y corregido con `research/blackfin/gnu-13.1-dreg-writeback.patch`. Genera
assembler, linker, objdump y ambos simuladores; no instala nada en el sistema.
No requiere firmware para compilar.
Necesita acceso HTTPS a `ftp.gnu.org` desde la política de red del entorno.
Al terminar ejecuta cinco probes propios de paquetes paralelos y comprueba por
separado los resultados stock y corregidos, sin utilizar firmware.

Los logs quedan en `logs/` dentro de la salida. Si falla, consulta el log de la
etapa indicada: no se debe interpretar una configuración a medias como una
instalación válida. El script rechaza salidas existentes para no sobrescribir
trabajo ajeno. Para reintentar en un directorio nuevo sin descargar de nuevo,
puedes pasar `--binutils-archive` y `--gdb-archive` apuntando a los archivos de
`downloads/` anteriores; se vuelven a verificar sus hashes.

Este flujo se comprueba en Linux. Para una computadora Windows, utiliza el
entorno Linux de Codex o WSL2 con las dependencias de compilación; no son los
comandos de compilación Windows/MSVC del plugin.

## 2. Preparar únicamente el LDR privado

Vuelve a adjuntar el archivo original en el nuevo chat e indica su ruta real:

```bash
python3 scripts/prepare-op1-private-input.py /ruta/op1_246.op1 \
  --output /workspace/private-op1-246
```

El script acepta exclusivamente el contenedor rev246 inventariado o su LDR
coincidente. Comprueba hashes y, en el contenedor, CRC y perfil LZMA. Lee solo el
miembro `OP1_vdk.ldr`; no extrae samples, gráficos ni bases de datos. El destino
debe ser nuevo y externo al repositorio, con permisos privados.

SHA256 esperado del `.op1`:
`c5315218f825f143b415ca554516541898abee843d3a236df0b54c04e1fb13a9`.

SHA256 esperado del LDR:
`f79d299f6f9019c998013a4176223a6a1bc999a2d8c54451346861f6ea455c74`.

## 3. Verificar el nuevo entorno

En la misma shell donde hiciste `source`:

```bash
python3 scripts/verify-drwave246-activation.py \
  --ldr /workspace/private-op1-246/OP1_vdk.ldr \
  --assembler "$POCKET_BFIN_AS" --linker "$POCKET_BFIN_LD" \
  --simulator "$POCKET_BFIN_SIM" --testutils "$POCKET_BFIN_TESTUTILS" \
  --output /workspace/private-drwave-activation-check
```

La salida debe ser un directorio nuevo. Sin `--record`, el oráculo compara con
los fixtures versionados y no los reemplaza. En la línea base VoiceCtl se
contrastan **512 registros / 61.568 muestras activas**, con 427 activaciones
aceptadas y 85 bloqueadas. Consulta la documentación actual si un cambio posterior
modifica esa línea base.

Los archivos `sdram.bin`, ELF y logs de la ejecución quedan en esa salida privada.
Objdump está disponible en `$POCKET_BFIN_OBJDUMP` para inspeccionar rangos acotados.
La disponibilidad de estas herramientas no descifra L1 ni completa el scheduler:
se mantienen los límites del modelo GNU ya documentados.

## Prompt para la cuenta nueva

```text
El enlace al artefacto de la otra cuenta no permite descargarlo. Vamos a preparar
las dependencias aquí sin depender de rutas de aquella sesión.

Actualiza la lectura de origin/main respetando mis cambios locales y consulta
docs/new-account-dsp-setup.md. Ejecuta scripts/setup-blackfin.py en un directorio
nuevo fuera del checkout. Cuando termine, carga su blackfin-env.sh.

Usa el op1_246.op1 que adjunto en este chat y prepara únicamente su LDR mediante
scripts/prepare-op1-private-input.py, en otra carpeta privada fuera del repo.
No publiques firmware ni artefactos privados.

Reproduce verify-drwave246-activation.py sin --record para comprobar las
herramientas. Después retoma docs/firmware-dsp-workplan.md: actualización del
pitch de VoiceCtl y su consumo por el render de Dr. Wave.
Si una dependencia falla, informa de la etapa y del error concreto.
```

Fuentes y licencias GNU se conservan junto a los builds; el parche mantiene
GPL-3.0-or-later. No se distribuyen binarios nuevos de esas herramientas en el
plugin. [Método del modelo](envelope246-research.md) ·
[Estado activo](firmware-dsp-workplan.md).

[Validación de descargas, compilación, modelos y oráculo](firmware/blackfin-setup-validation.json).

## Modelo adicional para el modificador de volumen

El [ensayo de volumen/pitch](drwave246-volume-buffer-modifier.md) requiere además
`research/blackfin/gnu-13.1-halfword-writeback.patch`, aplicado después del primer
parche. En un setup nuevo añade `--halfword-writeback` al comando de preparación.
La opción verifica tres paquetes propios de mitades distintas, además de los
cinco probes existentes. Sin ella se conserva el modelo anterior. El verificador
de volumen rechaza modelos que pierden una escritura halfword; no regeneres
fixtures para aceptar esa discrepancia. No se modifica firmware ni se valida L1.
