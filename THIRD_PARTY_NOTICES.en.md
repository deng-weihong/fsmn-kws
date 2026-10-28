# Third-Party Notices

[简体中文](THIRD_PARTY_NOTICES.md) | **English**

The project's own contributions are offered under Apache License 2.0.
Third-party files retain their original notices and licenses.

| Component | License | Included Notices | Changes |
| --- | --- | --- | --- |
| KissFFT 131.1.0 | BSD-3-Clause | `third_party/kissfft/COPYING`, `LICENSES/BSD-3-Clause` | Upstream files unmodified; separate packing wrapper |
| WebRTC audio processing 0.3 digital AGC | BSD-3-Clause, accompanying patent grant | `third_party/webrtc_agc/COPYING`, `AUTHORS`, `webrtc/PATENTS` | Adapted digital AGC, documented in its README and provenance |
| musl / Arm FP32 math | MIT and retained per-file notices | `third_party/musl_math/COPYRIGHT` and source headers | Selected upstream files unmodified; local glue and symbol prefixes |

Each dependency includes a `PROVENANCE.json` with source URLs and checksums.
Run `python3 tools/check_sources.py` to verify the vendored upstream files.

The Nihaomiya model is not included in the source distribution. Its upstream
model card declares Apache License 2.0. See `models/README.en.md` for the pinned
revision and attribution. Model licensing is separate from this library.

No official SoundConnect shared library, wheel, disassembly, decompiled listing,
personal recording, or device-specific calibration is included. Reconstruction
methods and the remaining upstream license review are recorded in
`docs/provenance.en.md`; the model card is not evidence of a license for the
official binary or its implementation.
