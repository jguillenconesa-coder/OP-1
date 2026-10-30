# Instrument release dispatcher: numeric fixture layout

Little-endian own inputs and numeric observations. No firmware, original
instructions, disassembly or ELF. [Contract and limits](../../docs/voice246-release-dispatch.md).

`voice246-release-dispatch-input.bin`: **1,680 records of 4,720 bytes**.

| Offset | Bytes | Input |
| --- | --- | --- |
| 0 | 4 | Reload flag, 0 or 1. |
| 4 | 4 | Unused caller word. |
| 8 | 32 | Source area: eight guard bytes, 16-byte argument, eight guard bytes. |
| 40 | 16 | Explicit preload of the dispatcher's local argument. |
| 56 | 8 | Unused caller words. |
| 64 | 4,656 | Shared regions below. |

Argument note starts at input offset 16; metadata byte is at offset 24. Its
three source padding bytes differ from the local preload padding by construction.

`voice246-release-dispatch-output.bin`: four bytes of **own LINK-frame probe**
address, 512-byte original note table, **1,680 records of 4,704 bytes**, then the
same 512-byte note table. No dispatcher semantic return or audio is recorded.

| Result offset | Bytes | Observation |
| --- | --- | --- |
| 0 | 4,656 | Shared image after complete original dispatcher. |
| 4656 | 32 | Complete source area/guards. |
| 4688 | 16 | Local argument after return; unchanged preload for filtered notes. |

| Shared offset | Address | Bytes | Retained in streams |
| --- | --- | --- | --- |
| 0 | `0x01dfe72c` | 2,576 | Yes. |
| 2576 | `0xff8000d0` | 664 | Yes. |
| 3240 | `0xffb00000` | 224 | Yes. |
| 3464 | `0xff801034` | 776 | No. |
| 4240 | `0x01b60ef0` | 128 | No. |
| 4368 | `0xff802bc8` | 8 | Yes. |
| 4376 | `0x01b60534` | 8 | Yes. |
| 4384 | `0x01b6053c` | 8 | No. |
| 4392 | `0xff8016d4` | 8 | No. |
| 4400 | `0x01aaac0c` | 8 | No. |
| 4408 | `0x01aaad14` | 32 | No. |
| 4440 | `0x027fd9dc` | 12 | No. |
| 4452 | `0x018c2180` | 12 | No. |
| 4464 | `0x01de4c3c` | 12 | No; discriminator word at 4468 is zero. |
| 4476 | `0x01aafd5c` | 136 | Yes; zero map at 4480–4607, with four-byte guards. |
| 4612 | `0x01aadb1c` | 40 | Yes; transport monitor. |
| 4652 | `0x018b600c` | 4 | Yes; transport flag monitor. |

Records 0–1279 are independent valid-note cases (256 per selector); 1280–1359
are independent filtered-note cases (16 per selector). Records 1360–1679 are
five streams of 64, reloading retained regions only at each stream start. Source
argument and local preload refresh every call. Map bytes stay zero and are not
recleared during a stream. Pools are explicitly initialized as described in the
contract; no renderer, pending transport or reserved full-voice caller is run.
