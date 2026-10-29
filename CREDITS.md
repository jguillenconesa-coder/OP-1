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


## Phase 246 integration research (0.8.0)

[BertBrandstifter/op1-sdk](https://github.com/BertBrandstifter/op1-sdk/tree/ae1945a9c61468f0e0d7cfdd5dd60c2cf9731ff1)
(MIT) supplied community documentation of the rev246 voice clock, range-table
addresses and control/pitch contracts. We independently tested isolated original
range initialization and frequency conversion. [U-Boot's Blackfin bootrom.h](https://github.com/u-boot/u-boot/blob/v2013.10/arch/blackfin/include/asm/mach-common/bits/bootrom.h)
supplied flag definitions; [sualk/op1-decryptor](https://github.com/sualk/op1-decryptor/tree/20f86baa116b4c7b1da7e0d4e3a24f171c76767b)
was consulted for the encryption layout only (no declared license; no code copied).
None is linked or bundled. Pocket One's reconstruction, adapter, GUI and tests
are independently written. Original firmware/decryption keys are not distributed.


## Envelope research and simulator correction (0.9.0)

GNU GDB 13.1's Blackfin simulator is Copyright (C) 2005–2023 Free Software
Foundation, Inc.; contributed by Analog Devices, Inc.; GPL-3.0-or-later.
`research/blackfin/gnu-13.1-dreg-writeback.patch` is a local modification under
those terms, retained separately with the [GPL text](licenses/GPL-3.0.txt).
No GNU simulator binary/code is linked into the instrument. The product's C++
envelope is an independent reconstruction, with numerical coefficient observations
and test traces from the user-supplied firmware. No firmware images/instruction
snippets are packaged. The local patch is not claimed to be upstream-approved or
hardware validated. See [evidence](docs/envelope246-research.md).


## Digital research (0.10.0)

The Digital voice identification was informed by the same pinned MIT op1-sdk
reference credited above and independently tested through isolated original
render/constructor/reset calls. The C++ oscillator, causal adapter, GUI, harness
and tests are independently written. Stored numeric fields and numeric oracle
traces are documented research observations, not a distributed firmware image.
GNU model limitations and original-preset omissions remain explicit in
[the Digital method](docs/digital246-research.md).


## Punch, manual and preset inventory (0.11.0)

The user-supplied Teenage Engineering OP-1 v2.2 manual defines the documented
control names, octave behavior and Helicopter/Singing Birds exercise targets.
REA's read-only SQLite inspector independently inventories the rev246 factory DB;
evidence and the source digest are recorded. No manual images, original database,
audio or firmware instructions are redistributed in this work. Punch kernel,
adapter, visual drawing, harness and tests are independently written; numeric
coefficients/traces are documented observations. The GNU tool patch retains its
separate terms. Existing reference attributions above continue to apply.

## Value FX reconstruction (0.12.0)

Value246's independently written FX kernel, clock adapter, display drawing and
harness are AGPL-3.0-only project work. The user-supplied rev246 image supplies
numeric observations; GNU BF524 executes the isolated FX path with the documented
local model patch. REA's read-only SQLite inspector confirms seven LFO type/default
records. The OP-1 manual §16.3 informs the additional Helicopter study, whose raw
positions and output remain uncalibrated against hardware. Original firmware,
opcodes, DB, manual illustrations and recordings are not distributed. Public
op1-sdk references and all previous tool attributions continue to apply.

## Value instrument / ADSR routes (0.13.0)

The user-supplied rev246 image provides numeric observations for independent
Digital/Phase Value routing and common ADSR modulation. GNU BF524 with the
documented local model patch executes the isolated originals; no original
instructions or simulator are distributed. The per-voice clock/allocator, UI,
harness and tests are independently written project code. Four additional
variants use inventoried factory controls; missing original FX and mix/play-mode
behavior remain explicit. The supplied preset-demo MP4 could not be transferred
because of the tool's 32 MiB limit; no audio/screens are copied or claimed as
verified for the active reference. Superseded segmented batches are excluded at
the user's request. Existing dependency/reference notices remain valid.

## Preset listening reference index (2026-10-27)

The user supplied 21 definitive excerpts of the demo linked at
<https://www.youtube.com/watch?v=P84SBtIDdSM>, with the supplied filename
“Every Single OP-1 Preset (2021)”. The [screen index](docs/preset-video-index.md)
records 169 reviewed name/category observations and their times, correlated to
rev246 inventory names. It does not claim identical parameters, acoustic matching
or drum-kit coverage. No video, decoded audio or screenshots are distributed.

The local analysis tool uses separately installed FFmpeg/ffprobe, Tesseract,
Python, NumPy and Pillow. These research tools are not added to the plugin or
bundled in its installers. See the index for reproduction and review limits.

## Conditional spectral pilot and study renderer

The Sugar/Pure [pilot](docs/preset-audio-pilot.md) publishes computed magnitudes
and an offline renderer for our current Engine/core integration, not original
audio. It uses the definitive user-supplied reference above and the rev246 numeric
preset observations already credited. It does not establish complete patch or
recording-chain equivalence. The analysis additionally uses separately installed
SciPy and Matplotlib; these are research dependencies, not plugin runtime or
installer additions.

## Cluster rev246 isolated kernel

The independently written [Cluster reconstruction](docs/cluster246-kernel.md)
uses the operator-supplied rev246 firmware only as a private instruction oracle.
The committed fixtures contain numerical test results and scalar observations,
not the firmware, original instructions or L1 payload. The same documented GNU
BF524 simulator patch and source notices apply. The count-normalization values
in the probes are explicitly synthetic caller inputs; the original L1 table and
complete instrument integration are not claimed. No preset/video data is used
for this component. See the [active firmware DSP workplan](docs/firmware-dsp-workplan.md).

## Cluster normalization layout and factory delta

The [initialization delta](docs/cluster246-normalization.md) uses the documented
OP-1 partial-encryption layout described in
[sualk/op1-decryptor's README](https://github.com/sualk/op1-decryptor/blob/20f86baa116b4c7b1da7e0d4e3a24f171c76767b/README.md)
and the previously credited op1-sdk internals. No decryptor implementation is
copied or bundled, and no encryption key is recovered or distributed. Five
unchanged scalar words are converted using the original helper in the private
oracle; the remaining three entries stay unknown. The independent constructor
and fixture harness additionally verify the original six-slot object factory.

## Cluster shared initialization and fractional conversions

The [shared-initializer reconstruction](docs/cluster246-shared-init.md) and
binary32-to-Q15/Q31 helpers are independently written from private instruction
observations of the operator-supplied firmware. Inputs are explicit numerical
test values; no three missing normalization entries are inferred. REA 6.8.0 was
used for read-only auxiliary SQLite schema/sample inspection. Its ELF provider
was unavailable; GNU Binutils checked the private harness layout instead. The
support summary preserves the distinction and does not attribute Blackfin
decompilation or L1 decryption to REA. GNU model outputs with and without the
previously credited local patch agree for these new cases; this is not hardware
validation. Original firmware/ELF, databases and instruction bytes stay private.

## Cluster Core routing and shared six-slot state

The [Core callback reconstruction](docs/cluster246-core-routing.md) independently
implements Cluster note-on/release behavior observed in the operator-supplied
rev246 firmware. Its own harness explicitly schedules six interleaved slots;
it does not implement the original allocator, clock, ADSR routing or mixer.
Only the five previously verified normalization counts are selected. REA 6.8.0
read-only SQLite evidence distinguishes the factory DB's Cluster row ID 1 from
runtime engine type 4, independently checked through the original Core factory.
No presets are read for this delta. Source databases, instructions, firmware and
ELF remain private; only own code and numerical fixtures/metadata are published.

## Common ADSR transition dependency

The independently written [ADSR transition reconstruction](docs/cluster246-envelope-transitions.md)
uses private original callback/render calls from the user-supplied rev246 image
and the previously credited GNU model. Pending words and event order are explicit
test inputs; their complete runtime consumers, MIDI mapping, Cluster envelope
routing and mixer are not inferred. No preset/video data is used. Original
instructions, firmware and ELF are not included; the new header is not yet used
by the plugin. Existing ADSR kernel/provenance notices continue to apply.

## Cluster VoiceCtl activation and note-table observations

The [activation reconstruction](docs/cluster246-note-activation.md) independently
implements an original SDRAM VoiceCtl route specialized to Cluster. The numeric
128-note table is checked through an unchanged bounded original copy fragment in
a private probe, not through the complete initializer. Tuning is an explicit
caller input; no original tuning calibration is claimed. Following renders use
the harness's held-increment schedule. The L1 handoff remains blocked by mixed
ciphertext, using the previously credited community layout model only. No key,
firmware, original instructions, ELF or L1 image is included, and no preset/video
data is used. The new headers are not linked into the plugin.

## Four-input pitch-buffer modifier

The independent [four-input pitch-buffer modifier](docs/drwave246-pitch-buffer-modifier.md)
and its probe/comparator are AGPL-3.0-only project work. The user-supplied rev246
image provides private original-instruction observations under the credited GNU
BF524 deferred D-register model. REA 6.8.0 `inspect-with-objdump`, using the GNU
Blackfin tool and an own synthetic symbol, retains private static evidence;
REA's `function` command does not support this Blackfin ELF. No L1 decryption,
complete LFO identity, original buffer producer or hardware timing is claimed.
Only numeric inputs/results and independently written source are published.

## Dr. Wave reader and five-oscillator reconstruction

The independent [Dr. Wave reader](docs/drwave246-reader.md) is checked against
unmodified SDRAM instructions from the operator-supplied rev246 firmware using
the previously credited GNU BF524 model and local D-register patch. All six wave
buffers are synthetic test inputs authored here, not recovered factory resources.
The numerical reciprocal seeds, object layout and callbacks are private firmware
observations; no original code, firmware image or ELF is published. The probe
verifies only the reader route outside the scheduled wave-update slot and the
inactive early return. Wave generation, complete scheduling and plugin integration
remain pending. The community op1-sdk provided candidate addresses; its WHEEL DSP
is not copied or presented as the original Dr. Wave. No new REA decompilation or
L1 decryption is claimed for this delta.

## Dr. Wave generated base banks

The [base-bank reconstruction](docs/drwave246-wavebank.md) independently computes
six banks of harmonic-sum tables from the previously verified sine. The private
oracle executes the original shared initializer and complete bank builder from
the user-supplied rev246 image using the credited GNU BF524 model and D-register
patch. Only own source/harnesses, numerical test inputs/outputs and metadata are
published. Factory presets, sample recordings, firmware, original instructions
and ELF are not included. Non-default gains are explicit stress inputs. The
scheduled control-driven wave update and full engine integration remain pending;
these generated base tables alone do not implement that route. No WHEEL synthesis
code or new REA decompilation/decryption is used for this delta.

## Dr. Wave wave-buffer transforms

The [three transform reconstructions](docs/drwave246-wave-transform.md) are
independently written from private observations of the operator-supplied rev246
SDRAM routines, executed unmodified using the credited GNU BF524 model and
D-register patch. Internal Q16 words, synthetic wave buffers and callback order
are explicit inputs; no knob mapping, spectral mixer or complete voice update
is inferred. Numerical fixtures include shared caches, lazy flags, guards and
unmodified neighboring voices/configuration. Original instructions, firmware,
SDRAM and ELF remain private. No presets, WHEEL DSP, or new REA L1 decryption
or decompilation are used or distributed for this delta.

## Dr. Wave spectral callbacks and complete isolated render

The [spectral and complete-render reconstructions](docs/drwave246-kernel.md) are
independently written from private observations of the user-supplied rev246
SDRAM routines. Their oracles execute the original base-bank initializer, six
spectral callbacks and entire render unmodified under the previously credited
GNU BF524 model and D-register patch. Only own source/harnesses, numerical test
fixtures and metadata are published. Presets, firmware, original instructions,
SDRAM images and ELF remain private. Full raw-control preparation, internal
update order and shared caches are checked; the external scheduler, allocation,
pitch calibration/provider, ADSR, output and plugin integration are not inferred.
The existing reader shares its inner loop with the complete adapter and retains
its regression oracle. No WHEEL synthesis or new REA L1 decryption is used.

## Dr. Wave counter-consumer reconstruction

The [16-bit dispatch extension](docs/drwave246-counter.md) is independently
checked against every input word, twice with different neighbor halfwords,
by inserting the exact private dispatch fragment in an own bounded caller.
A separate oracle calls the entire original render unmodified across explicit
counter boundaries; previous fixtures remain unchanged. Only numerical results,
own code and metadata are published. The partially encrypted L1 reference is
recorded as a candidate, without inventing its missing pointer/store/flow.
The 8-of-24 format fact uses the already credited community documentation;
no new REA decryption, recovered clock or hardware timing is claimed.

## Dr. Wave Core event routing

The [Core event adapters](docs/drwave246-core-routing.md) are independently
written and checked against the original Core factory, note-on/release callbacks
and full Dr. Wave render under the previously credited GNU BF524 model. Explicit
event scheduling, pitch and valid synthetic vtable tags are harness inputs;
common ADSR release and complete instrument integration are not inferred.
REA 6.8.0 supplies a fresh read-only SQLite observation of `synth_types.id/type`
to distinguish database ID 3 from runtime type 2. No preset rows, original code,
firmware or private ELF are distributed by this delta. The previously credited
GNU tools and deferred D-register patch remain the instruction-level oracle.

## Dr. Wave VoiceCtl activation and shared register preparation

The [Dr. Wave activation](docs/drwave246-note-activation.md) is independently
checked against original VoiceCtl, Core, constructor/reset and complete render
instructions using the previously credited GNU model. The original bounded
128-word note-table copy is re-executed privately, with an own return appended.
Only numerical observations, own source/harnesses and metadata are published.
Tuning and the held render pitch remain explicit inputs, not recovered original
calibration or scheduling. Shared register preparation is extracted from the
previous Cluster adapter; its unchanged fixtures still pass, including Windows
under Wine and sanitizers. No firmware, original instructions, private ELF, new
presets or L1 decryption are distributed or claimed by this delta.

## Four-source volume/pitch buffer modifier

The independent C++ reconstruction and own harness are checked against private
rev246 observations under GNU BF524. A second local GNU GDB 13.1 patch,
`research/blackfin/gnu-13.1-halfword-writeback.patch`, preserves disjoint halfword
loads in the pending write queue. It retains GPL-3.0-or-later and is separate
from the instrument. Own packet probes and numerical observations are published;
firmware, original instructions and ELF remain private. See the
[contract and model limits](docs/drwave246-volume-buffer-modifier.md).
