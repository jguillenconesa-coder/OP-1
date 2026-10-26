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


`pd-stream.s.in` preserves original kernel state across blocks and supplies
per-sample increment trajectories; it calls original reset only at sequence
boundaries. `phase-controls.json` records raw knob words and provenance from the
operator-supplied firmware. The musical control path is not inferred from their
presence. The independently generated study bank is documented in
[phase-core-studies.md](../../docs/phase-core-studies.md).

`voice-contract.s.in` calls the original SDRAM control-range initializer and
sampler frequency setter. `scripts/verify-phase-contract.py` reproduces these
results and a separate 88.2 kHz Pd stream oracle; it keeps the full private SDRAM
image outside this repository. The optional 0.8.0 native integration is described
in [phase246-native.md](../../docs/phase246-native.md).


## ADSR probes and simulator model (0.9.0)

`parallel-dreg.s`, `adsr-random.s.in` and `adsr-stream.s.in` are our independently
written harnesses. `verify-envelope246.py` reproduces both the stock-model hazard
trace and the separately corrected-model oracles. `compare-dreg-model.py` checks
old privately generated probes, including the now-unshifted LUT copy.

**Exception to the harness license:** `gnu-13.1-dreg-writeback.patch` is a local
GPL-3.0-or-later modification of GNU GDB 13.1's Blackfin simulator (FSF / Analog
Devices source); it is not linked into the plugin. The GNU binary is not shipped.
Keep the stock simulator and a separate patched build. This fixes the documented
D-register writeback hazards only, not every ISA/timing behavior. See
[the full method and reproduction](../../docs/envelope246-research.md).
