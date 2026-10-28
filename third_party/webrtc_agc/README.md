# WebRTC fixed-point digital AGC subset

Source: **webrtc-audio-processing 0.3**. The source URL, archive checksum and
original per-file hashes are in `PROVENANCE.json`. `COPYING`, `AUTHORS` and
`webrtc/PATENTS` are retained. This subset builds offline; neither the full
upstream archive nor the official SoundConnect binary is needed at runtime.

Only `webrtc/modules/audio_processing/agc/legacy/digital_agc.c` is adapted:

- Initialize the adaptive slow capacitor to zero.
- For negative VAD log ratio, use decay `-10`.
- In the `0..1024` log-ratio interval, use factor `100` and clamp decay to at
  most `-10`; retain the upstream `-65` above that interval.
- Ignore the VirtualMic low-level flag in digital decay processing.

The usual nine forward gain-smoothing comparisons are retained. Numerical
probes confirm that omitting these comparisons produces incorrect output.

`../../src/frontend/agc.c` initializes only the verified reference default:
16 kHz, 160-sample frames, adaptive digital mode, power 8, level 4, limiter
enabled, analog target 24. No upstream analog AGC, virtual microphone gain,
far-end input or optional noise suppression is part of this implementation.
The native virtual-microphone path does not change its input PCM in this
configuration; the optional audit compares against the full `AGC_Process`
path to verify the digital-only replacement.

Build with `-fwrapv` and no fast-math; fixed-point integer wrapping and signed
shifts matter. The project links this subset into its static library through
private `fsmn_kws_agc_*` wrappers. Numerical parity has been verified on Linux
x86_64; target-device validation is still required.
