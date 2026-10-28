# Validation

[简体中文](validation.md) | **English**

## Offline Checks

`ctest` exercises the independent frontend, missing-reference behavior, stream
reset, input immutability, calibration validation, the model clock, synthetic
model parsing, decoder phrase/channel behavior, and CLI output protection.
The C++ tests run without Python or downloaded weights. Optional Python CLI
tests use only the standard library. Generated signals and synthetic model
weights are software fixtures, not speech recognition benchmarks.

`python3 tools/check_sources.py` verifies the vendored upstream file hashes.

## Extraction Checks on 2026-10-28

- Linux x86_64, GCC 11.4: Release build and all three CTest groups passed.
- Exported source-only tree: offline configure, build, all tests and dependency
  verification passed without downloaded model assets or the original project.
- Installed static library: a separate C++11 consumer configured, linked and ran.
- Rockchip GCC 8.3: ARMv7-A hard-float library and all three Linux examples
  cross-compiled successfully. The extracted binaries were not run on a device.
- Two pinned upstream recordings: all six dump types were byte-identical to
  the original independent implementation; all 15 wake events matched.
- PCM stream CLI: both recordings reproduced the WAV CLI's event blocks,
  scores and selected microphones exactly.

The configured GCC/Clang CI matrix is provided for future repository runs;
Clang was not available for the local checks above.

## Reference Regression

Fetch the pinned upstream model and two recordings using
`python3 tools/fetch_model.py --examples`. With the default level `0.81` and
no device EQ, the expected event block numbers are:

| Recording | Events | Input Block Numbers |
| --- | --- | --- |
| `3ch_nihaomiya.wav` | 5 | 370, 594, 868, 1158, 1398 |
| `3ch_nihaomiya10.wav` | 10 | 312, 470, 610, 744, 892, 1032, 1184, 1310, 1442, 1554 |

Input times are block numbers multiplied by 20 ms. An incomplete final block
is skipped, and reported separately. Frontend and decoder state start empty.

`tools/check_regression.py` compares this extraction with an independently
supplied compatible reference CLI. It runs both binaries with identical model,
WAV and optional EQ paths in an isolated temporary directory. It compares
enhanced PCM, Fbank, AGC gains, contexts, posteriors, channel weights and events.
The extraction acceptance criterion is byte equality on the same host/toolchain.
The report contains asset/binary hashes and results, not local input paths.
The checked-in [extraction report](extraction-regression.json) records both
recordings: all six dump types match byte-for-byte and all 15 events agree.

```sh
python3 tools/check_regression.py \
  --reference /path/to/reference_cli --candidate build/kws_wav \
  --model models/model.txt \
  --wav models/3ch_nihaomiya.wav models/3ch_nihaomiya10.wav \
  --report reports/extraction.json
```

## Platform and Quality Boundaries

This extraction targets Linux x86_64 and cross-compilation for Linux ARM
using GCC or Clang. Same-host numerical regression does not certify another
toolchain, library, CPU, device capture path or microphone array.
The original implementation had RV1106 measurements; the extracted library
requires its own target verification before deployment.

The two recordings establish fixed-default implementation regression only.
Far-field recall, quiet first-pass speech, noise robustness and false wakes
per hour require separate labeled recordings and long-duration background tests.
Measured device EQ is optional and is not part of the reference baseline.
