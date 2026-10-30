# Adjustment setter / constructor numeric fixtures

Little-endian own inputs and observed numeric outputs; no original firmware,
instructions, disassembly or ELF. [Contract and limits](../../docs/voice246-note-adjustment.md).

`voice246-note-adjustment-input.bin`: **904 records of 3,720 bytes**.

| Offset | Bytes | Input |
| --- | --- | --- |
| 0 | 4 | Reload action, 0 or 1. |
| 4 | 4 | Setter index, signed32 0–7. |
| 8 | 4 | Setter value, signed32 -4–4. |
| 12 | 4 | Constructor raw note. |
| 16 | 4 | Constructor raw velocity. |
| 20 | 4 | Argument shift: 0, 4, 8 or 12. |
| 24 | 32 | Eight initial signed32 adjustments, consumed by setters only on chain reload. |
| 56 | 4 | Chain flag: 0 setter alone, 1 setter then constructor. |
| 60 | 4 | Unused caller word. |
| 64 | 64 | Destination area; argument starts at 16 + shift within this area. |
| 128 | 3,592 | Concatenated shared regions below. |

`voice246-note-adjustment-output.bin`: **904 records of 7,316 bytes**.

| Offset | Bytes | Observation |
| --- | --- | --- |
| 0 | 4 | Original R0 after the target setter, observed as 4; not a semantic bool. |
| 4 | 64 | Complete destination area after setter, before constructor. |
| 68 | 3,592 | Shared image after setter, before constructor. |
| 3660 | 64 | Complete destination area after constructor, or unchanged in standalone cases. |
| 3724 | 3,592 | Shared image after constructor, or unchanged in standalone cases. |

| Shared offset | Address | Bytes | Retained in streams |
| --- | --- | --- | --- |
| 0 | `0x01de4c3c` | 12 | No; discriminator at offset 4. |
| 12 | `0xff8016d0` | 12 | No; selected slot at offset 16. |
| 24 | `0x01dfe4d8` | 48 | Yes; table starts at offset 32. |
| 72 | `0xff801034` | 776 | Yes. |
| 848 | `0xff8000d0` | 160 | Yes. |
| 1008 | `0x01dfe72c` | 2,576 | Yes. |
| 3584 | `0xff802bc8` | 8 | Yes. |

Records 0–71 are independent setters with arbitrary poison in other table words.
Records 72–647 are independent chain cases. Records 648–903 are four streams
of 64 calls, reloading only at each stream start. Chain reloads overwrite all
eight poison table words by calling original setters with the header's eight
initial values; subsequent changes use the target setter. No table reload occurs
between calls in a stream or between setter and constructor. The argument area
and all retained regions follow the reload action. Shared data are raw stress
images, not initialized musical voices. No audio is produced by this oracle.
