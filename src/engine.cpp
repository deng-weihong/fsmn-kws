// SPDX-License-Identifier: Apache-2.0
#include "fsmn_kws/engine.h"
#include "model/model.h"
#include "decoder/decoder.h"

#include <algorithm>
#include <cstring>

namespace fsmn_kws {

class Engine::Impl {
    detail::Model model_;
    detail::Decoder decoder_;
    Frontend frontend_;
    float context_[2][120] = {};

public:
    Impl(const std::string &model, float level, const std::string &equalizer)
        : model_(model), decoder_(level), frontend_(equalizer) {}

    void reset() {
        frontend_.reset();
        std::memset(context_, 0, sizeof(context_));
        model_.reset();
        decoder_.reset();
    }

    Output step(const int16_t *mic0, const int16_t *mic1, const int16_t *reference) {
        Output out = {};
        out.frontend = frontend_.step(mic0, mic1, reference);
        if (out.frontend.block % 2 == 0) {
            for (int ch = 0; ch < 2; ++ch) {
                std::memmove(context_[ch], context_[ch] + 40, 80 * sizeof(float));
                std::copy_n(out.frontend.features + ch * 40, 40, context_[ch] + 80);
            }
            std::memcpy(out.context, context_, sizeof(context_));
            model_.step(out.context, out.posterior, out.channel_weights);
            out.evaluated = true;
            out.detected = decoder_.step(out.posterior, out.channel_weights,
                                         out.confidence, out.channel);
        }
        return out;
    }
};

Engine::Engine(const std::string &model, float level, const std::string &equalizer)
    : impl_(new Impl(model, level, equalizer)) {}
Engine::~Engine() = default;
void Engine::reset() { impl_->reset(); }
Output Engine::step(const int16_t *mic0, const int16_t *mic1, const int16_t *reference) {
    return impl_->step(mic0, mic1, reference);
}

} // namespace fsmn_kws
