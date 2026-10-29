// SPDX-License-Identifier: AGPL-3.0-only
// Own disjoint-half packet probes; inputs and expected values are independent.
.include "testutils.inc"
start
P0.H = results;
P0.L = results;
P1.H = word_source;
P1.L = word_source;
I0 = P0;
imm32 R1, 0x12345678
R0 = 2;
R1.H = R0.L << 8 (S) || R1.L = W[P1] || W[I0++] = R1.H;
[P0+4] = R1;
P0 += 8;
I0 = P0;
imm32 R1, 0x56781234
R0 = 3;
R1.L = R0.L << 8 (S) || R1.H = W[P1] || W[I0++] = R1.L;
[P0+4] = R1;
P0 += 8;
I1.H = other_source;
I1.L = other_source;
imm32 R1, 0xdeadbeef
MNOP || R1.L = W[P1] || R1.H = W[I1];
[P0] = R1;
P0.H = args;
P0.L = args;
R0 = P0;
P0 = 5;
EXCPT 0;
exit 0
.data
.align 4
args: .long 1,results,20
word_source: .short 0xabcd,0
other_source: .short 0x2468,0
results: .space 20
