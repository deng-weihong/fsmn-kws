# Cross-Compiling for Embedded Linux

[简体中文](cross-compiling.md) | **English**

The library has no sound-card, Python or service dependencies. Supply a Linux
cross toolchain and disable host tests. For an ARMv7 hard-float GCC toolchain:

```sh
cmake -S . -B build-arm \
  -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=arm \
  -DCMAKE_C_COMPILER=/path/to/toolchain/bin/arm-linux-gnueabihf-gcc \
  -DCMAKE_CXX_COMPILER=/path/to/toolchain/bin/arm-linux-gnueabihf-g++ \
  -DCMAKE_BUILD_TYPE=Release -DFSMN_KWS_BUILD_TESTS=OFF
cmake --build build-arm -j4
```

Use the compiler names and target sysroot provided by the actual SDK. Pass
`CMAKE_SYSROOT` when the toolchain requires an external sysroot. RV1106 uses
the Rockchip ARMv7-A/Cortex-A7 toolchain; a typical SDK needs
`-march=armv7-a -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard`.
These flags belong in the toolchain file or `CMAKE_C_FLAGS`/`CMAKE_CXX_FLAGS`.

`FSMN_KWS_PORTABLE_MATH` defaults to `ON` to stabilize adaptive FP32 powers
across C libraries. Preserve `-ffp-contract=off`, disabled fast-math and the
fixed-point wrapping flags. A compiler change still requires reference checks.

The generated coefficient header uses exact hexadecimal FP32 literals, so
the library is compiled as C++17. Its installed public API requires C++11.

The examples use POSIX clocks and resource accounting and run on Linux.
Applications can link only the library with `FSMN_KWS_BUILD_EXAMPLES=OFF` and
provide their own capture/PCM buffering. Processing remains fixed at 16 kHz
and exactly 320 samples per channel per call.

Cross-compilation alone is not a device test. Run the same reference recordings
on the target and compare enhanced PCM, features, model probabilities and
event blocks before connecting a live microphone stream.
