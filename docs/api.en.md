# Runtime Contract

[简体中文](api.md) | **English**

## Input and Clocks

Both APIs accept exactly 320 samples per channel at 16 kHz, corresponding to
20 ms. Inputs are raw signed PCM16 microphone 0 and microphone 1. An optional
third pointer supplies a playback reference already synchronized to capture.
Null reference means zeros. Microphone pointers must be non-null and input
buffers are not modified.

Do not feed already enhanced or software-amplified audio to this frontend
without validating the resulting processing chain. Playback synchronization
and sound-card capture are the responsibility of the application.

`FrontendOutput` contains:

| Field | Layout | Frequency |
| --- | --- | --- |
| `block` | Zero-based input block number | Every 20 ms |
| `enhanced` | Interleaved `int16_t[320 * 2]` | Every 20 ms |
| `features` | Channel-major `float[2 * 40]` | Every 20 ms |
| `gains` | `float[2]`, measured AGC L1 gain | Every 20 ms |

`Engine::step()` returns `Output`, whose `frontend` field contains the same
acoustic output. The model is evaluated on input blocks 0, 2, 4, and so on.
Its fields are valid only when `evaluated` is true; otherwise they are zero.

| Model Field | Meaning |
| --- | --- |
| `context` | Channel-major `[2, 120]`, two older selected feature frames then current |
| `posterior` | Five class probabilities: filler, ni, hao, mi, ya |
| `channel_weights` | Per-channel positive-activation winner fractions |
| `detected` | A completed keyword event |
| `confidence` | Adaptive HMM keyword score; not a calibrated probability |
| `channel` | Selected microphone, zero or one |

The FSMN has five layers with one model frame of lookahead each, totaling
200 ms. Reported event time is the processing position of the input block,
not a recovered speech onset or measured UI latency.

## State and Lifetime

Use one instance per continuous stream and serialize calls to it. Constructor
allocations create FFT plans, DSP histories and model parameters. Normal
`step()` processing does not allocate memory dynamically.

Call `reset()` after a dropped block, reconnect or other stream discontinuity.
It clears adaptive filters, AGC and all histories; the complete engine also
clears model context and decoder state. Block numbering restarts at zero.
Keep state across successful detections. Arbitrary repeated resets can reduce
recognition quality because the adaptive frontend needs history.

Bad model/calibration files and invalid arguments throw `std::runtime_error`.
A processing failure should be followed by a reset or stream reconstruction
before consuming further audio.

## Decoder Level

The default main level is `0.81`, secondary level `0.01`, night level `0.96`,
and adaptive recovery timer 500 model frames (20 seconds). The public main
level accepts finite values in `[0, 1]`. Level zero selects the reference
zero-level detection policy; it does not disable detection.

Adaptive state can produce scores below the configured main level. An
additional fixed confidence filter changes reference behavior. Threshold
selection for a new device requires labeled positive and background audio.

## Optional Device Calibration

All examples and APIs accept an optional input EQ path. The file contains:

```text
FSMN_KWS_INPUT_EQ_V1 16000 2 321
<321 gains for microphone 0>
<321 gains for microphone 1>
```

The compatibility header `MIYA_INPUT_EQ_V1` is also accepted. Gains must be
finite, in `[0.01, 32]`, with exactly 642 values. They correspond to 0, 25,
50, ..., 8000 Hz and multiply the microphone spectra after analysis and
before AEC. The playback reference is unchanged. Packed DC and Nyquist bins
are handled separately. An empty path keeps the reconstructed default chain.

No measured device profile is shipped. Calibration may include the loudspeaker,
room and microphone together; it should not be presented as a universal
microphone correction.
