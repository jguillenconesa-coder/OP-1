# Last-valid-voice query numeric fixtures

Little-endian own test inputs and numeric observations; no firmware, original
instructions, disassembly or ELF. [Contract and limits](../../docs/voice246-last-voice-query.md).

`voice246-last-voice-query-input.bin`: **1,024 records of 3,756 bytes**.

| Offset | Bytes | Input |
| --- | --- | --- |
| 0 | 4 | Reload action, 0 or 1. |
| 4 | 4 | Incoming R0 stress word, not a semantic argument. |
| 8 | 4 | Incoming R1 stress word. |
| 12 | 4 | Incoming R2 stress word. |
| 16 | 3,740 | Shared regions below. |

`voice246-last-voice-query-output.bin`: **1,024 records of 3,744 bytes**:
return R0 (4 bytes) followed by the complete 3,740-byte shared image.

| Shared offset | Address | Bytes | Retained in streams |
| --- | --- | --- | --- |
| 0 | `0x01dfe4d0` | 64 | Yes; persistent word at image offset 4. |
| 64 | `0x01dfe72c` | 2,576 | No; entries begin at image offset 72; count at 2632. |
| 2640 | `0xff801034` | 776 | Yes. |
| 3416 | `0xff8000d0` | 160 | Yes. |
| 3576 | `0xff802bc8` | 8 | Yes. |
| 3584 | `0x01b60ef0` | 128 | No. |
| 3712 | `0xff8016d0` | 16 | No. |
| 3728 | `0x01de4c3c` | 12 | No. |

The first 512 calls reload all regions independently; the last 512 are eight
streams of 64 calls, reloading retained regions at each stream start only.
Lists/counts and non-retained globals are explicit each call, including unused
entries after the count. Incoming persistent bytes in a non-reload record are
ignored. The only original write allowed within these images is offset 4.

Records 0–63 are 32 reversed pairs, lengths 2/6/17/128: each odd record reverses
the preceding record's active 20-byte entries and retains the unused tail.
Signed32 voice words occur at entry offset 16. Other entry fields and shared
memory are raw stress data, not initialized voice state. All numeric cases are
screened before execution to exclude the original error-helper condition.
