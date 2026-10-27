# Isolated original-instruction reference vectors

## Dr. Wave reader delta

`drwave246-reader-input.bin` has 6,144 bytes of six own synthetic 256-word wave
buffers, followed by 2,048 records of 1,072 bytes. Each record contains action,
voice, nonnegative signed16 update counter, enabled and reserved uint32 words;
four signed16 controls; 128 pitch words; a 216-byte six-object pool; a 272-byte
output image with 8-byte guards on either side; five shared increments; an init
flag and 16 seed bytes; and one shared pointer. Action bits reload state,
construct the current slot, reset and release, in that order. Reload happens for
the first 1,024 independent cases and the start of each subsequent 64-block stream.

`drwave246-reader-output.bin` has 2,048 records of 536 bytes: guarded output,
216-byte pool, five increments, init flag, 16 seed bytes, shared wave pointer and
factory return pointer (zero when not requested). The final 6,144 bytes verify
that all synthetic waves remain unchanged. There are 1,890 rendered blocks and
158 inactive returns. The wave-update slot (`counter % 6`) is always different
from the rendered object's slot. Wave generation is **not** exercised.

These fixtures use the documented GNU D-register deferred-writeback model.
[Contract, reproduction and limitations](../../docs/drwave246-reader.md).

## Earlier vectors

Generated with GNU Binutils 2.44 (bfin-elf) and GNU GDB 13.1 Blackfin simulator,
BF524 model in user mode with explicitly mapped RAM. The matching original LDR
SHA-256 is recorded in `evidence.json`; original code snippets are not included.
Inputs are synthetic. Numeric outputs were independently compared to
`Source/ReferenceDsp.h` and later independent kernels. Pd, ADSR and Digital
now feed optional experimental plugin modes; early field-only probes remain
research evidence. Each section identifies its own simulator model and scope.

| File | Format, all little endian | Cases |
| --- | --- | --- |
| fm-fields.bin | int32 selector + three uint32 table addresses, 16 bytes/case | 65,536 |
| q15-input.bin | two int16 operands, 4 bytes/case | 65,536 |
| q15-output.bin | int16 result | 65,536 |
| pd-input.bin | 132 control bytes + int32 initial phase + uint32 increment | 1,024 |
| pd-output.bin | 128 int16 outputs per case | 131,072 samples |
| synthetic-sine.bin | 8,192 int16 synthetic shifted LUT entries | 8,192 |
| pd-transient-input.bin | 132 control bytes + int32 phase + uint32 increment + int32 secondary phase + uint32 counter + uint32 active/reset selector | 8,192 |
| pd-transient-output.bin | 128 int16 samples + uint32 phase, secondary phase, counter, active | 8,192 blocks / 1,048,576 samples |
| verified-sine.bin | 8,193 int16 mathematical sine entries, matched to LDR source including fills | 8,193 |
| lut-copy-output.bin | int16 output of isolated original table-copy initializer on GNU simulator | 8,193 |
| pd-stream-input.bin | 132 control bytes + 128 uint32 increments + uint32 reset flag, 648 bytes/block | 2,304 |
| pd-stream-output.bin | 128 int16 samples + uint32 phase, secondary phase, counter, active | 294,912 samples + 2,304 final states |

FM: the packed word and selector word both equal the counter, covering every
16-bit value of each word, not all 2^32 independent pairs. A dummy table base
0x02000000 replaces the runtime pointer, so addresses establish offsets only.
Q15: 81 boundary pairs followed by deterministic LCG pairs, default simulator
rounding. Pd: secondary branch inactive, no signed phase wrap, zero other
controls, caller-supplied constant increments and synthetic shifted sine LUT.
The additional Pd transient suite covers all 8^4 boundary-control combinations
and 4,096 deterministic random cases; seven increments from zero to INT32_MAX,
random phases, inactive/active secondary states and 2,730 original reset calls
(selector 2 asks the harness to call original reset). Samples and all four final
state fields match. There are 130,485 main and 77,472 secondary signed cycle wraps
in the independent reconstruction over these fixtures. Original raw controls,
constant increments per block and explicitly loaded mathematical table are used;
control scaling, preset decoding, frequency preparation and host audio mixing
are not established by this suite.

Six LDR fill blocks complete the source sine table; its 8,193 low words match
the mathematical sine exactly. GNU's original parallel copy emits a one-entry
shift, repeating the last zero. This apparent parallel load/store timing issue
is recorded separately and remains unresolved against actual hardware. The
transient suite loads the verified **source** table explicitly, not the shifted
simulator initializer output. The optional 0.8.0 native mode explicitly uses the
verified source table, not the shifted result; it does not claim that the original
hardware initializer has been resolved.

**This does not validate full instruments, original preset mappings or audio
against real OP-1 hardware.**
The simulator itself has unimplemented alternate rounding behavior; no real
Blackfin hardware comparison was performed. Exact reproduction:

```sh
python scripts/verify-reference-dsp.py --ldr /private/OP1_vdk.ldr \
  --assembler /tools/bfin-elf-as --linker /tools/bfin-elf-ld \
  --simulator /tools/bfin-run --testutils /sources/gdb-13.1/sim/testsuite/bfin \
  --output /private/new-oracles
```

The output must be a new directory outside this repository. It will contain
private original instruction snippets; do not add it to releases. Normal CTest
uses only the numeric fixtures and needs neither firmware nor GNU simulator.


## Continuous streams and actual renderer (additional research)

The stream suite carries state across 16 consecutive blocks per sequence. It
uses the 16 raw Phase control records in `research/blackfin/phase-controls.json`,
three host sample rates (44100, 48000, 96000), and three external NCO trajectories:
MIDI 60 steady, MIDI 48→60 glide, and MIDI 72 with 8 Hz / 1 semitone vibrato.
There are 144 original reset calls, 2,304 blocks and 294,912 compared samples.
These are **stored knob words applied directly as experimental kernel inputs**;
their original runtime routing/scaling remains unverified. REA independently
confirmed their database extraction; see `docs/firmware/phase-controls-rea-verification.json`.
The NCO trajectory is our test convention, not the original frequency provider.

`pd_renderer` verifies the actual offline `pd_reference_render` executable
against 98,304 samples from the steady segments (all 16 records × three rates ×
2,048 frames). This checks its mathematical lookup generation, tuning convention,
raw control binding, reset, PCM byte order and overwrite protection. The tests
pass with native Linux and cross-built Windows executables under Wine.
The simulator reproduction command above now verifies **twelve** fixture files.

## Phase clock and control contract (0.8.0)

`voice-contract.bin` has seven little-endian uint32 setter results for input Hz
55,110,220,261.625565,440,880,1760, followed by 120 records of four little-endian
int16 values from the original range initializer. The Phase row is type 7,
eight records per type. These are numerical observations, no original opcodes.
`pd-clock-input.bin` / `pd-clock-output.bin` use the existing continuous-stream
format: 16 raw presets × three pitch trajectories × 16 blocks of 128 samples,
now with external NCO increments at 88200 Hz. They contain 98,304 sample outputs
and 768 final states. `verify-phase-contract.py` reproduces all three byte-for-byte.
Original pitch preparation, output filtering and envelope remain outside the oracle.


## ADSR and parallel writeback (0.9.0)

`adsr246-input.bin`: 4096 × 48 bytes. Six uint32 state words (stage, block count,
level bits, field12, field16, gain), four int16 controls, int16 volume + two pad
bytes, uint32 RNG seed, int32 prior dither value, uint32 action. All little endian.
`adsr246-output.bin`: 1024-byte header of 256 Q31 coefficients, then 4096 × 292
bytes: 128 int16 outputs, six final state words, uint32 completion, final seed and
prior dither value. `adsr246-stock-output.bin` records the unmodified GNU hazard
trace separately; it is not the production acceptance oracle.

`adsr246-stream-input.bin`: 16384 records in the same 48-byte format, 16 stored
envelopes × 1024 blocks. Actions 1/2/3 invoke original note-on/release/fast-stop;
0 continues. Action 1 loads the supplied initial state/seed. The 292-byte output
format has no coefficient header. State is carried between blocks. The model
uses the documented local GNU D-register deferred-write patch; instructions
remain original. These numerical traces do not establish hardware equivalence.
[Provenance and reproduction](../../docs/envelope246-research.md).


## Digital 246 (0.10.0)

`digital246-input.bin`: 2,048 × 556 bytes. Each little-endian record has 32 state
bytes, four signed int16 controls, 128 uint32 phase increments and a uint32 action
(0 continue, 1 replace state, 2 original reset, 3 original constructor).
`digital246-output.bin`: 2,048 × 288 bytes: 128 int16 samples then 32 full state
bytes. These include arbitrary initial states, signed/extreme increments, control
boundaries and 16 continuous preset/automation/reset streams. All 262,144 samples
and full states are compared by Digital246Tests.cpp. Generated by isolated
original calls using the documented deferred D-register GNU model. Source/ELF
remain private; see docs/firmware/digital246-validation.json and the reproduction
script. The independently written Digital kernel is used by the optional plugin
mode, unlike the earliest experimental-only probes described above.


## Punch 246 (0.11.0)

`punch246-input.bin`: 2,048 records of 348 bytes = 208-byte state + four int16
controls + 64 int16 input samples + uint32 action (continue/load/reset).
`punch246-output.bin`: 540-byte header (129 converted int32 coefficients and
four min/max/scale int16 triples), then 2,048 records of 340 bytes: 64 int16
audio values, 208 complete state bytes, uint32 containing the display word.
All little endian. 512 independent arbitrary-state cases and three 512-block
streams include stage automation, input cessation and reset. Valid parameter
ranges only. GNU deferred D-register model; no hardware timing validation.
The test also checks streaming parity at 44.1 kHz with integration DC removal
disabled, all seven adapter rates, and post-panic silence. See reproduction and
hashes in docs/punch246-native.md and docs/firmware/punch246-validation.json.

## Cluster Core routing

`cluster246-routing-input.bin`: 144 bytes of explicit coefficient words (eight
int16 normalization entries and 32 int32 damping entries), then 1,024 little-endian
records of 1,340 bytes. Each record contains uint32 action, voice, loaded-sound
and selected-sound IDs; 648 pool bytes; 32 context bytes; uint32 RNG;
eight sounds × eight int16 controls; and 128 uint32 phase increments.
Action bits: 0 loads pool/context/RNG, 1 calls the six-slot Core factory,
2 calls Core note-on, 3 calls Core release, 5 decays display. Every record renders
one selected voice. Per-record pool/context/RNG bytes are ignored when load bit 0
is clear; sound IDs, controls and phase increments are supplied for every record.

`cluster246-routing-output.bin`: 1,024 records of 1,104 bytes. Each contains uint32
note-on result (`0xffffffff` if not requested), 128 int16 samples, 648 pool bytes,
32 context bytes, uint32 RNG, six uint32 producer pointers, loaded/selected uint32
IDs and the 128 source control bytes. The original release callback is void.
There are 256 independent cases and 12 continuous streams of 64 records. Unknown
normalization entries 2/3/8 are zero placeholders that are never selected. Events
and pitch are external inputs; full original audio scheduling is not claimed.
[Method, hashes and limitations](../../docs/cluster246-core-routing.md).

## Common ADSR transition dependency

`adsr246-transition-input.bin`: 2,560 little-endian records of 56 bytes. Offsets
0–23 contain the six state words; 24–31 four int16 controls; 32 int16 volume;
34 uint16 release-sustain word; 36 uint32 pending word; 40 uint16 pending word;
42 two pad bytes; 44 uint32 RNG seed; 48 int32 prior dither value; 52 uint32 action.
The low action byte is 0 continue, 1 initialize, 2 note-on, 3 release, 4 fast-stop,
5 restart request. Bit 8 loads the record's state/RNG/dither; otherwise they carry.
Controls, volume and callback inputs are supplied for every record. Sustain,
level, low-half gain and volume stay nonnegative; pending fields cover raw bits.

`adsr246-transition-output.bin`: 2,560 records of 316 bytes. Offsets 0–23 hold the
post-callback/pre-render state; 24–279 the 128 int16 outputs; 280–303 the post-render
state; 304 uint32 completion; 308 uint32 RNG; 312 int32 prior dither value.
There are 512 independent cases and 16 streams × 128 records. These are numeric
observations from isolated original callbacks and render under the documented
GNU model. There is no full original scheduler, MIDI route or Cluster mixer.
[Method, hashes and limitations](../../docs/cluster246-envelope-transitions.md).

## Cluster VoiceCtl preparation

`cluster246-activation-input.bin`: 144 bytes of explicit coefficients (eight int16
normalizations, 32 int32 damping words), then 1,024 little-endian 912-byte records.
Offsets: 0 uint32 action (bit 0 load, bit 1 Core factory); 4 voice, 8 loaded sound,
12 selected sound; 16–31 source note (index/velocity/opaque word/explicit pitch);
32 signed32 tuning; 36–683 full pool; 684–715 context; 716 RNG; 720–783 two 32-byte
register images; 784–911 eight sounds × eight int16 controls. Load selects only
the pool/context/RNG/register input; controls, note, tuning and IDs always update.

`cluster246-activation-output.bin`: a 512-byte header of 128 note-table words
observed through the original bounded copy loop, then 1,024 records of 1,020 bytes.
Offsets: 0–255 samples; 256–903 pool; 904–935 context; 936 RNG; 940–1003 register
images; 1004–1019 unchanged source note. The register images come from
`0xff80105c` and `0xff8012d4`, each 32 bytes, including untouched guard bytes.
There are 512 independent cases and eight 64-record streams. The following
Cluster render holds the prepared pitch throughout each block as an explicit
test convention. Tuning is supplied by the caller; counts 2/3/8 are never selected.
[Method, hashes and limitations](../../docs/cluster246-note-activation.md).
