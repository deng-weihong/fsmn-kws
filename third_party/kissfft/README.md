# KissFFT scalar float32 subset

Unmodified sources from **KissFFT 131.1.0**. `PROVENANCE.json` records the
archive URL/checksum and every source checksum. The BSD-3-Clause license is
retained in `COPYING` and `LICENSES/BSD-3-Clause`.

The separate `../../fft_reconstruction.c` wrapper packs the Nyquist real value
into the imaginary DC slot, and normalizes inverse outputs by float32 `1/N`,
matching the pinned reference's `FFTWrapper`. No official code or binary is
linked into this backend. Compile without fast-math or fused multiply/add.

Both 640-point forward and inverse transforms are checked against the native
engine. Runtime exposes only `reference_fft_*` symbols. This backend removes
the small rounding differences of NumPy FFT at AGC's PCM16 decision boundaries;
it does not implement AEC or the keyword decoder.
