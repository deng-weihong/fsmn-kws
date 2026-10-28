// SPDX-License-Identifier: Apache-2.0
#include <fsmn_kws/frontend.h>
#include <fsmn_kws/engine.h>
#include <cmath>

int main() {
    fsmn_kws::Frontend frontend;
    int16_t zero[fsmn_kws::kBlockSamples] = {};
    const auto result = frontend.step(zero, zero);
    return result.block == 0 && std::isfinite(result.features[0]) ? 0 : 1;
}
