# Pocket One 0.2

**Sintetizador VST3 y aplicación independiente inspirados en el Teenage Engineering OP-1.** Interfaz gris, pantalla animada, cuatro mandos azul/verde/blanco/naranja y teclado de dos octavas. Implementación independiente; no contiene firmware, muestras de fábrica ni gráficos originales.

[English manual](README.en.md) · [Créditos](CREDITS.md) · [Licencia](LICENSE) · [Notas de versión](docs/releases/v0.2.0.md)

![Interfaz real de Pocket One](docs/interface.png)

## Descargar

| Paquete | Descarga |
| --- | --- |
| Instalador Windows 10/11 x64 | [Pocket-One-0.2.0-Windows-x64-Setup.exe](https://github.com/jguillenconesa-coder/OP-1/raw/refs/heads/main/downloads/v0.2.0/Pocket-One-0.2.0-Windows-x64-Setup.exe) |
| Windows portable: aplicación y VST3 | [ZIP](downloads/v0.2.0/Pocket-One-0.2.0-Windows-x64-portable.zip) |
| Linux x86_64: aplicación y VST3 | [TAR.GZ](downloads/v0.2.0/Pocket-One-0.2.0-linux-x86_64.tar.gz) |
| Fuentes completas con JUCE | [Código fuente correspondiente](downloads/v0.2.0/Pocket-One-0.2.0-corresponding-source.tar.gz) |
| Kit de compilación del proyecto | [ZIP](downloads/v0.2.0/Pocket-One-0.2.0-Windows-build-kit.zip) |
| Integridad de las descargas | [SHA256SUMS.txt](downloads/v0.2.0/SHA256SUMS.txt) |

También se ha configurado la creación de una [release de GitHub](https://github.com/jguillenconesa-coder/OP-1/releases) al publicar la etiqueta `v0.2.0`. Los archivos versionados de `downloads/v0.2.0` permiten descargarlos directamente desde el repositorio.

**Versión preliminar.** El instalador no tiene firma digital. Se probaron los ejecutables Windows y su instalación/desinstalación bajo Wine; queda pendiente probar el instrumento en un DAW Windows real. La release no incluye un instalador de macOS; CI genera sus artefactos de compilación.

## Instalar en Windows

1. Cierra el DAW y ejecuta el instalador. Requiere Windows 10/11 de 64 bits y permisos de administrador.
2. Elige VST3, aplicación independiente o ambos. El VST3 se instala en `C:\Program Files\Common Files\VST3\Pocket One.vst3`.
3. Abre tu DAW, vuelve a buscar plugins e inserta Pocket One en una pista de instrumento/MIDI. La aplicación independiente aparece en Inicio.
4. Para desinstalar, usa Configuración de Windows → Aplicaciones → Pocket One.

La versión portable se extrae sin instalador; copia el paquete `.vst3` completo a una carpeta que tu DAW explore. Un binario Linux no funciona en un DAW de Windows. [Instrucciones detalladas](docs/windows.md).

## Qué incluye

- **Cinco motores y ocho voces:** FM, distorsión de fase, cuerda pulsada, sampler WAV/AIFF y batería de bombo/caja/hi-hat.
- **Páginas de cuatro mandos:** SYNTH, ENV (ADSR), LFO, FX (delay/drive) y TAPE.
- **MIDI y automatización:** velocidad, sustain CC64, pitch bend en motores compatibles y sonidos iniciales.
- **Secuenciador:** 16 pasos, edición de notas y seguimiento del tempo, posición y saltos del host.
- **Cinta:** cuatro pistas mono, bucles de 4/8/16 segundos, grabación, overdub, mezcla, velocidad/reversa y exportación WAV de cuatro canales.
- **Sesiones:** muestras y cinta incorporadas al estado del DAW y a archivos `.pocketone`.

![Página real de cinta](docs/tape.png)

## Uso rápido

Selecciona un motor y toca el teclado o envía MIDI. OCT -/+ cambia la octava visible. Arrastra los mandos; el doble clic recupera el valor por defecto. Las pestañas cambian los parámetros que controlan los cuatro mandos.

**Sampler:** elige SAMPLER → LOAD. Importa los primeros 16 segundos del WAV/AIFF; C4/MIDI 60 es la nota raíz. START y LENGTH recortan la ventana; LOOP repite mientras mantienes la nota. Las entradas estéreo se convierten a mono.

**Secuenciador:** PLAY lo activa; en un DAW también debes iniciar el transporte. Pulsa pasos para activarlos/desactivarlos. Toca una nota y haz **Shift-clic** en un paso para escribirla. STOP detiene secuencia/cinta y libera el sonido.

**Cinta:** elige pista y duración en TAPE, arma REC, pulsa PLAY y toca. OVER suma material; desactivado, reemplaza lo que pase bajo el cabezal. La grabación funciona a velocidad normal; la reproducción permite reversa y congelación. CLEAR borra la pista seleccionada sin undo. EXPORT escribe un WAV de cuatro canales con las pistas sin aplicar mezcla/velocidad/master.

**SAVE / OPEN** guardan y recuperan sesiones completas. Guarda antes de cerrar la aplicación independiente. Haz las operaciones de sesión con el transporte detenido: copiar y codificar audio puede interrumpir brevemente el procesamiento.

[Manual completo en inglés](README.en.md) · [Cambios de la versión](docs/change-log.md)

## Compilar y probar

C++17, CMake 3.22+, Git y JUCE 8.0.6, fijado a `51a8a6d7aeae7326956d747737ccf1575e61e209`. CMake descarga JUCE; puedes usar `-DJUCE_PATH=/ruta/a/JUCE`. Windows requiere Visual Studio 2022 con herramientas C++ y NSIS 3.11+ para generar el instalador; JUCE 8 no admite MinGW.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

En Windows configura con `cmake -S . -B build -A x64`. `packaging\windows\build.cmd` compila, prueba y genera el instalador. El archivo de fuentes completas permite compilar seleccionando el JUCE incluido sin descargarlo de nuevo.

- [Dependencias y comandos por plataforma](README.en.md#build-from-source)
- [Compilación cruzada y validación Windows](docs/windows-cross-build.md)
- [Entorno en la nube](docs/cloud.md)
- [Pruebas realizadas y límites de la validación](docs/validation.md)
- [Cómo contribuir](CONTRIBUTING.md)

## Alcance, créditos y licencia

Pocket One se aproxima a la distribución de controles y al flujo creativo del OP-1; **no es una emulación completa ni se ha demostrado equivalencia sonora**. Faltan cinta larga con edición/undo, grabación de entrada/micrófono, más motores y mejoras de antialiasing. No importa proyectos/presets originales. El fundamental del motor de cuerda no responde todavía a pitch bend.

[Créditos completos](CREDITS.md): Teenage Engineering como inspiración; OTTO/TOP-1 como referencias; JUCE, Steinberg y los proyectos de herramientas/bibliotecas utilizados. No hay afiliación ni recursos originales copiados.

Código original: **AGPL-3.0-only**, con [licencia completa](LICENSE). La distribución abierta usa la opción AGPLv3 de JUCE y GPLv3 del SDK VST3. Los componentes externos conservan sus licencias y avisos: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
