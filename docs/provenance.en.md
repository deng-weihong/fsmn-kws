# Implementation Origins

[简体中文](provenance.md) | **English**

The runtime was extracted from a local experimental implementation named
`fsmn_lab`. `SOURCE_PROVENANCE.json` records the original source hashes and
the changes made during extraction. Numerical behavior is preserved by
regression testing, not by changing the processing algorithms.

## Reconstruction Method

The fixed-default pipeline was reconstructed using a checksum-pinned
`py_sound_connect 0.2.1` Linux x86_64 binary, its disassembly, Ghidra pseudocode,
extracted defaults, and native stage/state comparisons. Its SHA256 is
`ea564494b80c02f32197539f25acd71bba3d07ccb5725d12958b9debf9ee2dd1`.

This is an independent compatible implementation, not a clean-room design.
The proprietary original source was not recovered. No official shared library
is linked or loaded at runtime, and no official binary, trace, disassembly,
decompiler output, or private recording is included in this source tree.

The high-pass and six-phase PPFbank coefficients were recovered from native
traces. The checked-in `src/frontend/defaults.json` retains the exact defaults;
`coefficients.h` freezes their binary FP32 values for Python-free builds.
Model parameters remain separate upstream training artifacts.

KissFFT is unmodified public source. WebRTC's public digital AGC is adapted
to the observed default state behavior. Selected musl/Arm math files preserve
FP32 power rounding across host and target libraries. Individual dependency
provenance and adaptation records are retained under `third_party`.

## License Evidence and Release Status

The upstream model card at the pinned revision declares Apache License 2.0.
The inspected reference wheel metadata contains `License: UNKNOWN` and an
MIT license classifier; no corresponding license text was found in that wheel.
The model card does not resolve the binary's terms or the publication rights
for reconstructed implementation details and recovered coefficients.

The local extraction is prepared for review. Verify the applicable upstream
terms and the redistribution boundary for the reconstruction and constants
before publishing it. The project's Apache license applies to contributions
its authors are entitled to license; it does not grant rights to upstream
binaries, models or third-party works beyond their own licenses.

## Attribution

Describe this project as a fixed-default, Miya-compatible C/C++ frontend and
FSMN runtime. Credit the ModelScope upstream model authors and retained DSP
dependencies. Reference matching is evidence for the tested files and defaults,
not a claim of endorsement or universal microphone performance.
