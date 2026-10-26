// SPDX-License-Identifier: AGPL-3.0-only
// Independently written packet-semantics probes; no firmware instructions.
.include "testutils.inc"
start
imm32 SP, 0x03000000
P0.H = results;
P0.L = results;
imm32 R0, 0x11112222
R1 = 3;
R2 = 4;
R0 = R1 + R2 (NS) || [P0] = R0 || NOP;
[P0+4] = R0;
P0 += 8;
imm32 R0, 0x33334444
R0 = MIN(R1,R2) || [P0] = R0 || NOP;
[P0+4] = R0;
P0 += 8;
I0 = P0;
P1.H = source;
P1.L = source;
imm32 R0, 0x55556666
MNOP || R0 = [P1++] || [I0++] = R0;
[P0+4] = R0;
P0 += 8;
I0 = P0;
imm32 R0, 0x12345678
imm32 R1, 0x43214321
[SP+12] = R1;
MNOP || R0 = [SP+12] || W[I0] = R0.H;
[P0+4] = R0;
P0 += 8;
imm32 R0, 0x13572468
R0 = R0 - R0 (NS) || [P0] = R0 || NOP;
[P0+4] = R0;
P0.H = io_args;
P0.L = io_args;
R0 = P0;
P0 = 5;
EXCPT 0;
exit 0
.data
.align 4
io_args: .long 1,results,40
source: .long 0x77778888
results: .space 40
