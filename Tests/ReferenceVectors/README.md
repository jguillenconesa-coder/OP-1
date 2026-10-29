# Isolated original-instruction reference vectors

## Four-input pitch-buffer modifier before Dr. Wave

`control-pitch246-overrides.bin`: 512 little-endian records of 108 bytes,
appended to the unchanged `drwave246-kernel-input.bin` records by the own caller.
Offsets: 0 uint32 kind; 4 eight int16 controls (four targets, four destinations);
20 a guarded 32-byte source image (four uint32 sources start at 28); 52 a guarded
24-byte volume image; 76 a 16-byte callback object; 92 four uint32 pitch guards
(two before the buffer, two after). These inputs reload every call; synthesis
state follows the base fixture's independent/continuous reload actions.

`control-pitch246-output.bin`: generated banks (163,840 bytes), 512 records of
9,404 bytes, same unchanged banks. Offsets 0–8783 retain the complete kernel
result layout; 8784 uint32 callback return; 8788 guarded pitch image of 528 bytes
(128 words start at 8796, tail guard at 9308); 9316 source image (32 bytes);
9348 volume image (24); 9372 callback object (16); 9388 controls (16).
Only disabled/pitch routes run in this bounded original callback oracle.
It consumes an explicit existing buffer, not the missing VoiceCtl-register
producer. [Contract and limits](../../docs/drwave246-pitch-buffer-modifier.md).

## Dr. Wave complete VoiceCtl pitch refresh

`drwave246-pitch-refresh-overrides.bin`: 512 little-endian 2,980-byte records.
Offsets 0–239 retain generated-table activation overrides. Offset 240 holds
the low signed16 mode word in a uint32, 244 the RNG seed, 248 a 160-byte
guarded six-ADSR image, 408 a 2,572-byte guarded list image. List entries start
at 412; count is at 2972, final guard at 2976. Seed/ADSR reload only on action
bit 0; lists and parameters refresh every record. Common sound parameters
start from controls at offset 20, with the selected mode replaced by the word
at 240. The selected Core mode deliberately differs from that common mode.

`drwave246-pitch-refresh-output.bin`: note-table header (512 bytes), base banks
(163,840), 512 × 12,168-byte records, generated tuning table (9,600), same banks.
Each record retains the 9,272-byte indexed-activation result, then guarded ADSR
at 9272, RNG/neighbor at 9432, full guarded list at 9440, six null auxiliary
Core pointers at 12012, ADSR initialization flag at 12036, and preserved common
sound parameters at 12040. The source note at result offset 9228 is a retained
unused harness field; refresh reads the list instead. The RNG neighbor is
`0x31415926`. [Contract, evidence and scope](../../docs/drwave246-pitch-refresh.md).

## VoiceCtl generated tuning table

`voice246-tuning-table-output.bin` contains three little-endian 9,624-byte
records: guard, 2400 generated coefficient words, guard, then a 16-byte
index/neighbor image. Guards and index image preserve the preload pattern
(0, `0xa5a5a5a5`, `0xffffffff`); all table entries agree. The own caller skips
module/event setup and supplies a frame for the original tail at its SDRAM address.

`drwave246-tuning-table-overrides.bin` retains 512 × 240-byte indexed activation
records. The obsolete coefficient field at offset 164 is `0xdeadbeef` and is
not written to the generated table. `drwave246-tuning-table-output.bin` retains
the indexed-activation layout: 512-byte note table, 163,840-byte base banks,
512 × 9,272-byte results, generated 9,600-byte tuning table, unchanged base banks.
Coefficients are generated; register-to-render-buffer copying remains explicit
held-pitch harness code. [Contract and evidence](../../docs/drwave246-tuning-table.md).

## VoiceCtl tuning-index setter and Dr. Wave consumption

`voice246-tuning-index-input.bin` has 70,369 little-endian 32-byte records:
uint32 reload, signed32 coarse/fine, a 16-byte index/neighbor image, and an
unused uint32 word. `voice246-tuning-index-output.bin` has the resulting
16-byte image per record. Its index is at image offset 8 (`0x018c2184`).
There are 65,536 independent signed32 stress cases, 4,577 independent control
pairs, then 256 continuous calls with neighbors reloaded only at stream start.

`drwave246-tuning-index-overrides.bin` has 512 records of 240 bytes: the prior
232 activation override bytes followed by signed32 coarse/fine. The original
setter runs before activation. The harness stores explicit tuning at the
computed table index; the 2400-word test table starts at zero and persists.

`drwave246-tuning-index-output.bin` has the 512-byte copied note table and
163,840-byte base banks, then 512 records of 9,272 bytes, the final 9,600-byte
test coefficient table, and the unchanged base banks. Each record starts with
the 9,244-byte activation result, then the 16-byte index/neighbor image and
three words (coefficient before/at/after the selected index). Index neighbors
are explicit sentinels `0x11112222`, `0x33334444`, index, `0x77778888`.
This recovers index arithmetic/consumption, not original coefficient values.
[Contract, evidence and reproduction](../../docs/drwave246-tuning-index.md).

## VoiceCtl pitch bend input

`voice246-pitch-bend-input.bin` contains 65,792 little-endian 32-byte records:
uint32 reload action; four bytes (status, data1, data2, trailing guard); a
16-byte control/neighbor image for `0xff801048`; uint32 guard/register seed;
and uint32 opaque vector field. The first 65,536 records exhaust every unsigned
data-byte pair independently. The last 256 form one continuous control/register
stream, reloaded only at its start. Source/message guards refresh each call.

`voice246-pitch-bend-output.bin` contains 72 bytes per record: the full 16-byte
control image, a 32-byte source/vector image, then six preserved pitch words.
The source image has the seed at offsets 0/24/28, four message bytes at 4,
opaque field at 8, and begin/end/capacity at 12/16/20. Those pointers are
`0x02f02004`, `0x02f02007`, `0x02f02008`. Register words initialize as seed+i.
These are control observations, not rendered audio or a recovered buffer writer.
[Contract, evidence and reproduction](../../docs/drwave246-pitch-bend-input.md).

## Dr. Wave VoiceCtl pitch update delta

`drwave246-pitch-update-overrides.bin` contains 512 little-endian 2,824-byte
records. The first 232 bytes reuse the activation overrides. The appended
2,592 bytes contain uint32 query identity, signed32 explicit tuning, a guarded
signed32 mutable pitch (three words), and a 643-word list image: guard, 128
20-byte entries, uint32 count, guard. Entries hold the 16-byte note argument and
a signed32 voice: -2 broadcasts, -1 ignores, 0–5 writes on matching identity.

`drwave246-pitch-update-output.bin` retains the activation layout's 512-byte
note table and 163,840-byte base banks before 512 records of 11,828 bytes,
then the same banks. Each record starts with the activation result's 9,244
bytes and appends the 12-byte guarded scalar and 2,572-byte full list image.
All lengths 0–128 occur. There are 256 independent cases and four 64-call
streams; each call receives an explicit new list and tuning word. The original
routine runs after activation and before the harness holds the resulting
register pitch for Dr. Wave. This does not reconstruct the render-buffer writer.
[Contract, evidence and reproduction](../../docs/drwave246-pitch-update.md).

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

## Dr. Wave base-bank initialization delta

`drwave246-wavebank-input.bin` contains 18 records of 16 bytes: uint32 scratch
slot, gain-override flag, signed32 reference gain (raw bits), and uint32 preload
pattern. Every case calls the original shared initializer before the complete
bank builder, so the scratch wave starts clean. The first six cases preserve
the captured default gain and exercise all six scratch slots; twelve cases
inject explicit gains, including signed32 extremes.

`drwave246-wavebank-output.bin` contains 18 records of 172,128 bytes: 16 bytes of
settings immediately after shared initialization; 172,108 bytes from original
runtime range `[0x01cc1864,0x01ceb8b0)` after building the banks; and the uint32
scratch pointer. The range includes six contiguous banks of 320 × 256 signed16
table samples, six runtime waves, five shared increments, settings, caches and
surrounding memory. All values are little endian. The sine input is the previously
verified source supplied by the harness, not an execution of its full initializer.

[Contract, reproduction and limitations](../../docs/drwave246-wavebank.md).

## Dr. Wave three-transform delta

`drwave246-transform-input.bin` starts with a 6,144-byte synthetic six-wave pool,
then 512 records of 3,188 bytes. A record contains four uint32 words (action,
mode, slot, explicit Q16 word); 256 wave words; 2,080 bytes of guarded caches;
the uint32 reciprocal-init flag; 32 configuration bytes; and 32 pool-guard bytes.
Action bits reload the pool/caches, replace the current wave, and clear the
three lazy-init flags, respectively. Modes 0/1/2 call the original scatter/hold,
clamped and repeated read transforms. The first 256 calls are independent;
four streams of 64 calls then preserve shared state between calls.

`drwave246-transform-output.bin` has 512 records of 8,300 bytes: a guarded
six-wave pool (6,176 bytes), guarded caches (2,080), reciprocal-init flag (4),
configuration (32), and the two global pointers (8). All values are little
endian. The selected configuration word uses offset 12 for mode 0 and offset
20 for modes 1/2. Inputs are explicit in [0,65536], not normalized knob values.

The model includes the documented D-register deferred-writeback patch.
[Scope and reproduction](../../docs/drwave246-wave-transform.md).

## Dr. Wave spectral callbacks and complete-render delta

`drwave246-spectral-input.bin` has a synthetic 6,144-byte wave pool followed by
768 records of 1,072 bytes: argument, callback index, slot and action (four
uint32); 16 shared-setting bytes; 256 wave words; and a 16-byte trailing guard.
Actions reload all waves or replace the current wave. The first 384 records are
independent, followed by six streams of 64. Callback indices 0–5 select the six
functions listed in the [kernel contract](../../docs/drwave246-kernel.md).

`drwave246-spectral-output.bin` starts/ends with the 163,840 generated base-bank
bytes. Between them are 768 records of 6,196 bytes: the last 16 bank bytes as a
readonly leading guard, six waves, trailing guard, shared settings and wave
pointer. The callbacks must preserve the entire bank and all other voices.

`drwave246-kernel-input.bin` has 512 records of 9,316 bytes: four uint32 words
(action, slot, external counter image, enabled byte image); four signed16 controls;
128 pitch words; a 216-byte six-object pool; six wave buffers (6,144 bytes);
2,088 shared settings/cache bytes; five increments (20 bytes); 32 configuration
bytes; wave pointer and reciprocal-init flag (four bytes each); and a guarded
272-byte output image. Action bits reload state, construct, reset and release
the current slot. The enabled byte and counter are explicitly set every call.
There are 256 independent records and four streams of 64.

`drwave246-kernel-output.bin` starts/ends with the generated base-bank bytes.
Each of the 512 intervening records has 8,784 bytes: guarded output (272), pool
(216), waves (6,144), shared settings/caches (2,088), increments (20), configuration
(32), wave pointer (4), reciprocal flag (4) and counter with adjacent bytes (4).
All fields are little endian. Of these calls, 240 update a wave, 241 read without
updating, and 31 return inactive. There are 61,568 active audio samples; inactive
output is preserved, not counted as audio. The external counter/schedule and
pitch are explicit; original allocator, pitch calibration, ADSR and output remain
separate dependencies. The oracles use the documented patched GNU BF524 model.

## Dr. Wave counter-consumer delta

`drwave246-counter-output.bin`: 131,072 little-endian records of 8 bytes,
each containing the original dispatch remainder (uint32) and preserved source
DWORD. Records 0–65,535 cover every raw counter word with upper16 zero; the
second pass repeats with upper16 `0xffff`. The fragment is exactly
`[0x0197c89a,0x0197c8ca)` from the private image, with a caller/return supplied
by the harness. This output is not audio and does not establish clock cadence.

`drwave246-counter-overrides.bin`: 512 × 12 bytes of uint32 action, slot and
counter DWORD. Apply each triple at offsets 0–11 of the corresponding existing
`drwave246-kernel-input.bin` record. All remaining source bytes are reused.
The first 256 records are independent; two 128-call streams explicitly share
each counter across six calls, starting at 32758 and 65526. Their action reload
bit is set only at the start; constructor/reset/release events are retained.
This is a test clock.

`drwave246-counter-render-output.bin`: same format as the previous complete-render
output, from unmodified original render calls. 79 updates, 402 read-only calls,
31 inactive returns: 61,568 additional samples with complete state/cache/guard
comparison. All six slots, six spectral variants and three transforms are used.
[Contract, reproduction and blocked L1 provider](../../docs/drwave246-counter.md).

## Dr. Wave Core event routing delta

`drwave246-routing-overrides.bin`: 512 little-endian records of 148 bytes:
uint32 action, voice, counter DWORD, loaded sound, selected sound, then eight
sounds × eight int16 controls. Use the first three DWORDs to override offsets
0–11 of the corresponding `drwave246-kernel-input.bin` record, replace each
synthetic pool tag at offset `536 + voice*36` with `0x0190fb2c`, then append loaded
sound, selected sound and 128 control bytes. Expanded inputs are private and
9,452 bytes per record. Action bits reload state, call the six-voice Core factory,
call Core note-on and call Core release. Render reads controls from the loaded
sound row; the old four control words at input offset 16 are unused in this oracle.

`drwave246-routing-output.bin`: generated banks (163,840 bytes), 512 records of
9,164 bytes, then the unchanged banks again. Each record's first 8,784 bytes
follow the complete-render output format. Additional offsets: 8,784 note-on result
(uint32, `0xffffffff` when not requested), 8,788 six original producer pointers,
8,812 loaded sound, 8,816 selected sound, 8,820 controls (128 bytes), 8,948 pool
after Core events and before render (216 bytes).

256 independent cases and four 64-call streams; 51 in-record factories, 194
accepted and 86 rejected note-ons, 122 releases including 39 mismatched sound IDs.
79 wave updates, 402 read-only calls and 31 inactive returns give 61,568 additional
audio samples. Event order, counter and pitch remain explicit test inputs.
[Scope, provenance and reproduction](../../docs/drwave246-core-routing.md).

## Dr. Wave VoiceCtl activation delta

`drwave246-activation-overrides.bin`: 512 little-endian records of 232 bytes.
Offsets 0–147 follow the Core routing override format (action, voice, counter,
loaded/selected sounds and 128 control bytes). Only action bits 0 load and 1
Core factory are used; every record calls VoiceCtl exactly once. Offsets 148–163
contain the four source note words, 164 signed32 explicit tuning, 168–231 the
two 32-byte register images for `0xff80105c` and `0xff8012d4`.

Expand each record from the existing kernel input as in the Core routing oracle,
then append the note/tuning/register bytes. Expanded inputs are private, 9,536
bytes each. Initial pool tags are explicitly made valid Dr. Wave tags. The old
source pitch array is unused: the harness fills the render buffer with the
selected voice's registered pitch after activation, even following rejection.

`drwave246-activation-output.bin`: 512 bytes from the original bounded 128-note
copy, then generated banks (163,840 bytes), 512 records of 9,244 bytes, and the
unchanged banks. Each record's first 9,164 bytes follow Core routing output;
offset 8,784 is a reserved `0xffffffff`, not a VoiceCtl return value. Offset
9,164 contains the two register images (64 bytes), 9,228 the unchanged source
note (16 bytes). The pool at 8,948 is after activation and before render.

256 independent cases and four 64-call streams: 427 accepted, 85 gated, 214
accepted explicit pitches (27 ignore an out-of-range note index), all 128 lookup
notes accepted, and 61,568 additional active audio samples. Tuning and held pitch
are explicit test inputs; original calibration, pitch provider and ADSR remain
pending. [Contract and reproduction](../../docs/drwave246-note-activation.md).

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

## Entrada de cuatro canales ENV rev246

`control-envelope246-input.bin` y `control-envelope246-output.bin` contienen
1.024 entradas/salidas numéricas: callback ENV 0–3 seguido del render ADSR,
131.072 muestras, estados completos, RNG/dither, imágenes y guardas. Son 512
estados independientes y ocho secuencias de 64 llamadas. Fuentes, parámetros
iniciales y orden explícitos; no resuelven el relevo por voz ni pitch L1.
[Contrato y reproducción](../../docs/drwave246-envelope-control-input.md).

## Selector auxiliar ENV en contexto Dr. Wave

`control-envelope246-mode-input.bin` / `control-envelope246-mode-output.bin`: 1.024 llamadas, cinco colores ENV, seis estados ADSR, 131.072 muestras y
131.072 palabras de scratch dither compartido; inicializador de rangos original
capturado. Tipo interno 2 y pool inicializado explícitos. No completa el relevo
por voz ni el pitch L1. [Contrato](../../docs/drwave246-envelope-mode-control.md).

## Modificador de volumen/pitch en serie

`control-volume246-overrides.bin` y `control-volume246-output.bin`: 512 llamadas,
81 combinaciones, 65.536 muestras signed16 de volumen, 65.536 palabras de pitch
y 61.568 muestras Dr. Wave posteriores. Suavizados compartidos continuos;
buffers iniciales explícitos. Requiere ambos parches GNU, con probe previo
obligatorio. [Contrato](../../docs/drwave246-volume-buffer-modifier.md).

## Entrada de control VoiceCtl rev246

`voice246-control-input.bin` / `voice246-control-output.bin`: 32.768 parejas
independientes + 256 llamadas continuas, tabla completa de 128 palabras, guardas
y objeto/mensaje. Las imágenes iniciales se generan explícitamente desde las
semillas de entrada; el fixture de salida contiene todas las capturas numéricas.

`voice246-control-chain-overrides.bin` / `voice246-control-chain-output.bin`:
2.048 decodificaciones originales alimentan el modificador volumen/pitch y
el render Dr. Wave: 65.536 muestras de volumen y 61.568 de motor. Buffers
entrantes, selección de mensajes y orden explícitos.
[Contrato y reproducción](../../docs/drwave246-control-source-input.md).

## Destino instrumento Dr. Wave

`drwave246-control-instrument-overrides.bin` /
`drwave246-control-instrument-output.bin`: rangos originales del tipo 2,
2.048 decodificaciones y 512 callbacks instrumento, ocho selectores y
61.568 muestras posteriores. Imagen completa de parámetros, rangos y
contexto/estado/guardas; pitch/volumen/suavizado preservados.
[Contrato](../../docs/drwave246-instrument-control-routing.md).

## Destinos intercalados Dr. Wave

`drwave246-control-mixed-overrides.bin` / `drwave246-control-mixed-output.bin`:
512 llamadas, las 256 combinaciones de desactivado/instrumento/volumen/pitch
probadas con kind 0, 2.048 decodificaciones y 61.568 muestras Dr. Wave.
Parámetros, ambos buffers, suavizado, estado compartido, rangos y guardas
contrastados. Destino ENV excluido; no completa el instrumento.
[Contrato](../../docs/drwave246-mixed-control-routing.md).
