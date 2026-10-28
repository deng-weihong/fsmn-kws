# Implementation Origins

[简体中文](provenance.md) | **English**

The runtime was extracted from a local experimental implementation named
`fsmn_lab`. `SOURCE_PROVENANCE.json` records the original source hashes and
the changes made during extraction. Numerical behavior is preserved by
regression testing, not by changing the processing algorithms.

## Compatibility and Parameters

The runtime implements the fixed-default Miya pipeline. Numerical behavior is
verified by comparing per-stage outputs and internal state. The reference
runtime is `py_sound_connect 0.2.1` Linux x86_64, pinned to SHA256
`ea564494b80c02f32197539f25acd71bba3d07ccb5725d12958b9debf9ee2dd1`.

No official shared library is linked or loaded at runtime, and no official
binary or private recording is included in this source tree.

The checked-in `src/frontend/defaults.json` retains the exact default high-pass
and six-phase PPFbank coefficients. `coefficients.h` freezes their binary FP32
values for Python-free builds.
Model parameters remain separate upstream training artifacts.

KissFFT is unmodified public source. WebRTC's public digital AGC is adapted
to the observed default state behavior. Selected musl/Arm math files preserve
FP32 power rounding across host and target libraries. Individual dependency
provenance and adaptation records are retained under `third_party`.

## License Evidence and Release Status

The upstream model card at the pinned revision declares Apache License 2.0.
The inspected reference wheel metadata contains `License: UNKNOWN` and an
MIT license classifier; no corresponding license text was found in that wheel.
The model card does not resolve the official runtime's terms or redistribution
rights for the compatible implementation and default coefficients.

Redistribution of the compatible implementation and default coefficients
requires separate review under the applicable upstream terms.
The project's Apache license applies to contributions
its authors are entitled to license; it does not grant rights to upstream
runtimes, models or third-party works beyond their own licenses.

## Attribution

Describe this project as a fixed-default, Miya-compatible C/C++ frontend and
FSMN runtime. Credit the ModelScope upstream model authors and retained DSP
dependencies. Reference matching is evidence for the tested files and defaults,
not a claim of endorsement or universal microphone performance.
