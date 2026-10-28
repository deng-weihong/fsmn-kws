// SPDX-License-Identifier: Apache-2.0
#ifndef FSMN_KWS_FRONTEND_H
#define FSMN_KWS_FRONTEND_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace fsmn_kws {

constexpr std::size_t kSampleRate = 16000;
constexpr std::size_t kBlockSamples = 320;
constexpr std::size_t kMicrophones = 2;
constexpr std::size_t kFeatureBins = 40;

struct FrontendOutput {
    uint64_t block;
    int16_t enhanced[kBlockSamples * kMicrophones];  // Interleaved [320, 2].
    float features[kMicrophones * kFeatureBins];    // Channel-major [2, 40].
    float gains[kMicrophones];
};

// Fixed-default Miya-compatible frontend. Each instance owns all stream state.
class Frontend {
public:
    // Optional device calibration, applied to microphone spectra before AEC.
    explicit Frontend(const std::string &input_equalizer_path = std::string());
    ~Frontend();
    Frontend(const Frontend &) = delete;
    Frontend &operator=(const Frontend &) = delete;

    // Reset on stream discontinuities, not after each detected keyword.
    void reset();
    // Exactly 320 consecutive PCM16 samples per channel. Null reference = zero.
    // Microphone pointers must be non-null. Input buffers are never modified.
    FrontendOutput step(const int16_t *mic0, const int16_t *mic1,
                        const int16_t *reference = nullptr);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fsmn_kws
#endif
