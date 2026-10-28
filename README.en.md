# FSMN KWS

[简体中文](README.md) | **English**

A C/C++ acoustic frontend and offline keyword spotting runtime for embedded
Linux. The current implementation reconstructs the fixed-default Miya pipeline
and runs the original ModelScope **Nihaomiya** model on CPU in FP32.

The frontend can be used without a model. Model weights are downloaded
separately from their upstream source. The runtime builds offline with the
vendored DSP dependencies and does not load the official SoundConnect library.
Implementation origins and license review status are in
[docs/provenance.en.md](docs/provenance.en.md).

## Build and Test

Requirements: Linux, GCC or Clang, CMake >= 3.16, and a C++17-capable compiler.
The public headers can be consumed by C++11 applications.
Python >= 3.8 is optional for asset downloads and CLI integration tests.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
ctest --test-dir build --output-on-failure
python3 tools/check_sources.py
```

The tests use generated signals and synthetic weights. They require no model
download, Python packages, microphone, network, or neighboring source tree.
Python CLI checks are skipped when Python is unavailable; the C++ tests still run.

## Detect a Keyword

Download the pinned upstream model and optional reference recordings:

```sh
python3 tools/fetch_model.py --examples
build/kws_wav --model models/model.txt --wav models/3ch_nihaomiya.wav
```

The two reference WAVs contain five and ten reference detections, respectively.
The CLI emits JSON lines with block number, input time, HMM score and selected
microphone, followed by timing and memory statistics. See
[docs/validation.en.md](docs/validation.en.md) for the tested scope.

Live input from an ALSA capture device:

```sh
set -o pipefail
arecord -q -D default -t raw -f S16_LE -r 16000 -c 2 \
  | build/kws_stream --model models/model.txt --channels 2
```

Use `--channels 3` for two microphones plus a synchronized playback reference.
`--channels 1` duplicates a single microphone; that is a compatibility mode,
not a validated substitute for a physical two-microphone array.

## Use the Frontend Alone

Enhance a two- or three-channel recording without downloading any model:

```sh
build/frontend_wav --wav raw_input.wav --output enhanced.wav
```

The output is a stereo 16 kHz PCM16 WAV. Existing output files are rejected.
All WAV tools expect 16 kHz PCM16 input ordered as microphone 0, microphone 1,
and optional playback reference. They skip and report an incomplete final block.

## C++ API

```cpp
#include <fsmn_kws/frontend.h>
#include <fsmn_kws/engine.h>

fsmn_kws::Frontend frontend;
fsmn_kws::Engine wake("models/model.txt", 0.81f);

// Each input points to exactly 320 consecutive PCM16 samples.
auto audio = frontend.step(mic0, mic1, playback_reference);
auto result = wake.step(mic0, mic1, playback_reference);
if (result.detected) {
    on_keyword(result.confidence, result.channel);
}
```

Choose `Frontend` for enhancement/features, or `Engine` for the complete
pipeline. `Engine` already owns a frontend. Each instance belongs to one stream
and must be called by one thread at a time. See [docs/api.en.md](docs/api.en.md) for
layout, clocks, discontinuities and optional input calibration.

Install the library for use through CMake:

```sh
cmake --install build --prefix "$PWD/build/install"
```

```cmake
find_package(fsmn_kws 0.1 CONFIG REQUIRED)
target_link_libraries(your_application PRIVATE fsmn_kws::fsmn_kws)
```

Set `CMAKE_PREFIX_PATH` to the install prefix. The installed package includes
the public headers, static library, license notices and dependency provenance.
Embedded Linux SDK builds are described in
[docs/cross-compiling.en.md](docs/cross-compiling.en.md).

## Layout and Scope

| Directory | Contents |
| --- | --- |
| `include/fsmn_kws` | Standalone frontend and complete wake-engine API |
| `src/frontend` | High-pass, PPFbank, DRAEC, AuxIVA, AGC, Fbank and defaults |
| `src/model`, `src/decoder` | Fixed-topology FSMN and adaptive HMM |
| `third_party` | Pinned KissFFT, WebRTC digital AGC and musl math sources |
| `examples` | WAV detection, PCM stream detection and WAV enhancement |
| `tests` | Offline API, state, model-format and CLI contracts |
| `tools`, `models`, `docs` | Source verification, asset manifest and documentation |

The first release supports the original five-class Nihaomiya topology and fixed
frontend defaults. Custom keywords, arbitrary FSMN architectures, training,
non-default algorithms, and device-service deployment are outside this release.
Reference-recording parity is separate from real microphone recall and false
wakes per hour. Device EQ must be measured for the actual acoustic path.

## License and Attribution

Project contributions use [Apache License 2.0](LICENSE). Third-party code keeps
its own licenses, notices and patent grant; see
[THIRD_PARTY_NOTICES.en.md](THIRD_PARTY_NOTICES.en.md). Model weights are an upstream
training result, not trained by this project. Their source and declared license
are documented in [models/README.en.md](models/README.en.md).
