# Blackfin isolated probes

These independently written harness templates and linker scripts are AGPL-3.0-only.
They contain no original firmware instructions. `scripts/verify-reference-dsp.py`
extracts the inventoried original snippets into a new operator-selected directory
outside the repository and assembles them with these harnesses. It uses GDB 13.1
`testutils.inc` from the caller-supplied GNU source checkout; that external GPL
helper is not copied into this project or linked into Pocket One.

The simulator runs bounded isolated programs with synthetic inputs and explicit
RAM mappings, not OP-1 boot code, OS, drivers or a complete firmware image.
[Formats, scope and limitations](../../Tests/ReferenceVectors/README.md).
