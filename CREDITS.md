# Créditos

## Proyecto y desarrollo

- **Mantenimiento y dirección del proyecto:** [jguillenconesa-coder](https://github.com/jguillenconesa-coder).
- **Implementación inicial y preparación de compilaciones/documentación:** desarrolladas con asistencia de **OpenAI Codex**, según las indicaciones del mantenedor.
- **Pocket One** es una implementación independiente. El motor de audio y los gráficos propios de este repositorio se escribieron para este proyecto.

## Inspiración y referencias

- **Teenage Engineering — OP-1:** inspiración del instrumento, la disposición de controles y el flujo creativo. Las marcas OP-1 y Teenage Engineering pertenecen a sus respectivos titulares. No hay afiliación, patrocinio ni aprobación. El instrumento no incluye firmware, muestras de fábrica ni logotipos originales. Las reconstrucciones experimentales de DSP se documentan aparte y no se conectan al audio de producción.
- **OTTO y sus colaboradores:** organización por motores y pantallas, controles vinculados a colores y documentación de un instrumento autónomo. Referencia revisada: [antoinehng/OTTO-1](https://github.com/antoinehng/OTTO-1), revisión `ba534c6c1ed93a6b15cd13a93287b9e8f6f8ca6b`.
- **TOP-1 y sus colaboradores, incluido D-I-S-K-U-S por sus notas de diseño visual:** referencias de sampler, grabadora de cuatro pistas y relación entre gráficos y parámetros. Referencia revisada: [AlbertSmit/TOP-1](https://github.com/AlbertSmit/TOP-1), revisión `e641d273fb8d0eb4ff2ad1b4a7cd15b8786b62e0`.

OTTO/TOP-1 son referencias de diseño, no dependencias incorporadas. Sus fuentes y recursos no se copiaron en Pocket One. Sus instantáneas revisadas usan MPL-2.0. Véase [el análisis de referencias](docs/references.md).

## Bibliotecas y herramientas

| Componente | Autor/proveedor | Uso |
| --- | --- | --- |
| JUCE 8.0.6 | Raw Material Software Limited / JUCE y colaboradores | Audio/MIDI, interfaz, VST3 y aplicación independiente |
| VST3 SDK incluido en JUCE | Steinberg Media Technologies GmbH y colaboradores | Integración del formato VST3 |
| NSIS 3.11 | Colaboradores de NSIS | Instalador y desinstalador de Windows |
| LLVM/clang-cl/lld 19 | LLVM Project y colaboradores | Compilación cruzada de Windows |
| Microsoft CRT y Windows SDK | Microsoft | Runtime y API de Windows |
| xwin 0.10.0 | Jake-Shadle y colaboradores | Obtención y verificación del SDK/CRT para la compilación cruzada |
| Wine 10 y Xvfb/X.Org | Wine Project y X.Org y sus colaboradores | Pruebas de los ejecutables de Windows y renderizado en Linux |
| CMake, Ninja y GCC | Sus respectivos proyectos y colaboradores | Configuración, compilación y pruebas |

Las herramientas de compilación/prueba no se distribuyen dentro del instalador. El código fuente de JUCE conserva sus propios créditos y avisos de dependencias. Véanse [avisos de terceros](THIRD_PARTY_NOTICES.md) y [licencia del proyecto](LICENSE).

Factory sound bank: 25 original parameter recipes, 24 original drum one-shots and four pitched samples generated for this project. The WAV assets are CC0-1.0; the code retains AGPL-3.0-only. Format references: [te-op1](https://github.com/jstnfst/te-op1) (Field) and [OP Patch Utility](https://github.com/AlexCharlton/op-patch-util); no source or sound assets were copied from these references.

OLED animation references: OTTO-1 Nuke, TapeScreen and DrumSampleScreen, revision ba534c6c1ed93a6b15cd13a93287b9e8f6f8ca6b (MPL-2.0 upstream). Pocket One implements original JUCE drawing code and geometry; no OTTO source/assets were copied. Hardware layout also references the user-supplied OP-1 user guide v2.2 and photographs, which are not redistributed.

## Firmware research tooling (0.4.0)

The user-supplied OP-1 revision 246 was inspected with [REA 6.8.0](https://github.com/morluto/rea) (MIT), [Unblob 26.6.4](https://github.com/onekey-sec/unblob), official 7-Zip tooling and Python's standard library. [op1repacker](https://github.com/op1hacks/op1repacker) documents the CRC/LZMA/TAR container format. These are analysis tools/references, not linked product dependencies. No original firmware, databases, sounds, graphics or decompiled DSP are included. See [the evidence and limits](docs/firmware-analysis.md).


GNU Binutils 2.44 (bfin-elf) and the GNU GDB 13.1 Blackfin simulator are external research tools, credited to the GNU projects and contributors. They are not included in the plugin or installer. Own probe harnesses use caller-supplied GDB test helpers only in private generated research programs; [scope and reproducibility](research/blackfin/README.md).
