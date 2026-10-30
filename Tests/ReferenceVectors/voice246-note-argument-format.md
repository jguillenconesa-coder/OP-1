# VoiceCtl rev246 note-argument numeric fixtures

Own numeric test inputs and observed outputs, little-endian. No original
instructions, firmware image, disassembly or ELF. See
[contract, attribution and limits](../../docs/voice246-note-argument.md).

`voice246-note-argument-input.bin`: 1,280 records of 3,672 bytes.

| Offset | Bytes | Input |
| --- | --- | --- |
| 0 | 4 | Reload action, 0 or 1. |
| 4 | 4 | Raw note in R1. |
| 8 | 4 | Raw velocity in R2. |
| 12 | 4 | Destination shift: 0, 4, 8 or 12. |
| 16 | 64 | Initial destination area; argument starts at 16 + shift within it. |
| 80 | 3,592 | Shared regions concatenated in the order below. |

`voice246-note-argument-output.bin`: 1,280 records of 3,656 bytes: complete
64-byte destination area followed by the same 3,592-byte shared image.
There is no semantic return, audio buffer or renderer in this oracle.

| Image offset | Address | Bytes | Retained in streams |
| --- | --- | --- | --- |
| 0 | `0x01de4c3c` | 12 | No; discriminator at image offset 4. |
| 12 | `0xff8016d0` | 12 | No; selected slot at image offset 16. |
| 24 | `0x01dfe4d8` | 48 | No; eight table words start at image offset 32. |
| 72 | `0xff801034` | 776 | Yes. |
| 848 | `0xff8000d0` | 160 | Yes. |
| 1008 | `0x01dfe72c` | 2,576 | Yes. |
| 3584 | `0xff802bc8` | 8 | Yes. |

There are 1,024 independent reloads followed by four streams of 64 calls;
each stream reloads at its start. The destination area is retained with the
same action as the retained shared regions. A changing destination shift may
make a previous written byte become part of the next argument's padding.
All shared images are arbitrary preserved stress patterns, not assertions
about initialized voice state or a complete instrument graph.
