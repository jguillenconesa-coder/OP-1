# Isolated original-instruction reference vectors

Generated with GNU Binutils 2.44 (bfin-elf) and GNU GDB 13.1 Blackfin simulator,
BF524 model in user mode with explicitly mapped RAM. The matching original LDR
SHA-256 is recorded in `evidence.json`; original code snippets are not included.
Inputs are synthetic. Numeric outputs were independently compared to
`Source/ReferenceDsp.h`. This experimental header is not used by the plugin.

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
simulator initializer output. Do not silently substitute either into production.

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
