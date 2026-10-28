// SPDX-License-Identifier: Apache-2.0
#ifndef FSMN_KWS_ENGINE_H
#define FSMN_KWS_ENGINE_H

#include "fsmn_kws/frontend.h"

namespace fsmn_kws {

struct Output {
    FrontendOutput frontend;
    float context[240];              // Channel-major [2, 120], every 40 ms.
    float posterior[5];              // filler, ni, hao, mi, ya.
    float channel_weights[2];
    bool evaluated;
    bool detected;
    float confidence;                // Adaptive HMM score, not a probability.
    int channel;
};

// Fixed-topology CPU FP32 runtime for the original Nihaomiya text model.
class Engine {
public:
    explicit Engine(const std::string &model_path, float level = .81f,
                    const std::string &input_equalizer_path = std::string());
    ~Engine();
    Engine(const Engine &) = delete;
    Engine &operator=(const Engine &) = delete;

    void reset();
    // Same input contract as Frontend::step(). One instance per stream/thread.
    // Model fields are zero on blocks where evaluated is false.
    Output step(const int16_t *mic0, const int16_t *mic1,
                const int16_t *reference = nullptr);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fsmn_kws
#endif
