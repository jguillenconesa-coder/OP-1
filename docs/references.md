# Design references and implementation boundaries

User-supplied repositories inspected directly:

| Project | Inspected revision | Useful material |
| --- | --- | --- |
| [antoinehng/OTTO-1](https://github.com/antoinehng/OTTO-1) | `ba534c6c1ed93a6b15cd13a93287b9e8f6f8ca6b` (2018-02-09) | Engine/screen separation, four colour-linked encoders, Faust Nuke synthesizer, studio tape module |
| [AlbertSmit/TOP-1](https://github.com/AlbertSmit/TOP-1) | `e641d273fb8d0eb4ff2ad1b4a7cd15b8786b62e0` (2017-12-25) | Four-track tape buffer, synth/drum samplers, modular signal path, visual design guidelines |

Both snapshots use MPL-2.0. Their README/build instructions describe desktop or embedded standalone applications, using JACK and NanoVG, not VST3 plugins. Neither demonstrates bit-accurate OP-1 emulation. The old TOP-1 Faust sources also require a historical compiler/toolchain, so wholesale adoption would introduce a substantial porting task.

Pocket One uses a separate C++ DSP core and a JUCE processor/editor to preserve the useful engine/UI separation while allowing DAW automation, plugin state and sample-offset MIDI. The hardware enclosure and four-colour relationship are shared design ideas. All current graphics are drawn directly in JUCE; no reference artwork, fonts, samples or source files were imported.

The coloured screen trace and parameter readouts should communicate the engine's behaviour, following the explanation in TOP-1's `doc-src/design.md`. Current macros retain one consistent layout rather than reproducing its complete modal interface.

Implemented since the first prototype:

- WAV/AIFF sampler with trim, root-note transposition, looping and embedded project state.
- Separate SYNTH, ENV, LFO, FX and TAPE pages mapped to the four encoders.
- Percussion voices, step-note writing and host PPQ/tempo/seek following.
- Four mono tape tracks with normal-speed recording, overdub, variable/reverse playback, per-track gains and four-channel export. The buffer is a bounded 16-second-per-track in-memory loop, not TOP-1's disk-streamed design.
- Complete session save/load in addition to DAW state.

Remaining work toward the hardware workflow includes long disk-streamed tape, cut/lift/drop and undo, microphone/input sampling, a full drum sampler, more engine-specific controls, measured hardware comparisons and alias reduction. Only measured comparisons can support sonic-similarity claims.

Any future imported MPL-covered files must retain their notices and meet source-availability obligations; fonts/assets require their own licence review. JUCE has separate licensing terms.
