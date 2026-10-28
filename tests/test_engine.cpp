// SPDX-License-Identifier: Apache-2.0
#include "fsmn_kws/engine.h"
#include "decoder/decoder.h"
#include "test_support.h"

#include <cmath>
#include <iostream>
#include <limits>

int main() {
    try {
        TempFile model;
        write_model(model.path);
        fsmn_kws::Engine engine(model.path);
        fsmn_kws::Frontend frontend;
        int16_t silence[320] = {};
        std::vector<fsmn_kws::Output> outputs;
        for (int block = 0; block < 40; ++block) {
            const auto out = engine.step(silence, silence);
            const auto acoustic = frontend.step(silence, silence);
            require(!std::memcmp(out.frontend.features, acoustic.features, sizeof(acoustic.features)),
                    "engine frontend differs from standalone frontend");
            require(out.evaluated == (block % 2 == 0), "wrong model clock");
            require(!out.detected, "synthetic filler model triggered");
            if (out.evaluated) {
                float sum = 0;
                for (float p : out.posterior) {
                    require(std::isfinite(p) && p >= 0 && p <= 1, "invalid posterior");
                    sum += p;
                }
                require(std::abs(sum - 1) < 1e-6f, "posterior not normalized");
                const float expected = std::exp(1.0f) / (std::exp(1.0f) + 4);
                require(std::abs(out.posterior[0] - expected) < 1e-6f, "model layout changed");
                require(out.channel_weights[0] == 1 && out.channel_weights[1] == 0,
                        "channel tie selection changed");
            } else {
                for (float p : out.posterior) require(p == 0, "stale model output on skipped block");
            }
            outputs.push_back(out);
        }
        engine.reset();
        for (const auto &expected : outputs) {
            const auto out = engine.step(silence, silence);
            require(out.frontend.block == expected.frontend.block, "reset lost clock");
            require(!std::memcmp(out.posterior, expected.posterior, sizeof(out.posterior)),
                    "reset lost model state");
            require(!std::memcmp(out.context, expected.context, sizeof(out.context)),
                    "reset lost feature context");
        }
        require_error([&] { fsmn_kws::Engine invalid(model.path, -1); }, "negative level accepted");
        require_error([&] { fsmn_kws::Engine invalid(model.path, std::numeric_limits<float>::quiet_NaN()); },
                      "NaN level accepted");
        { std::ofstream trailing(model.path, std::ios::app); trailing << "0\n"; }
        require_error([&] { fsmn_kws::Engine invalid(model.path); }, "trailing model accepted");
        { std::ofstream short_model(model.path); short_model << "0\n"; }
        require_error([&] { fsmn_kws::Engine invalid(model.path); }, "truncated model accepted");

        fsmn_kws::detail::Decoder decoder(.81f);
        const float weights[] = {.1f, .9f};
        int events = 0, channel = -1;
        float confidence = 0;
        // A complete four-state phrase must fire once and select microphone 1.
        for (int state : {0, 1, 2, 3, 4, 0}) {
            float probabilities[5] = {.00001f, .00001f, .00001f, .00001f, .00001f};
            probabilities[state] = .99996f;
            for (int frame = 0; frame < 8; ++frame)
                events += decoder.step(probabilities, weights, confidence, channel);
        }
        require(events == 1 && channel == 1, "decoder phrase/channel behavior changed");
        require(confidence > .81f, "decoder confidence changed");
        std::cout << "model, clock, reset, validation and decoder contracts passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
