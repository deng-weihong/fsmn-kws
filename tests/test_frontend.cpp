// SPDX-License-Identifier: Apache-2.0
#include "fsmn_kws/frontend.h"
#include "test_support.h"

#include <cmath>
#include <iostream>

namespace {
void equal(const fsmn_kws::FrontendOutput &a, const fsmn_kws::FrontendOutput &b) {
    require(a.block == b.block, "block clock changed");
    require(!std::memcmp(a.enhanced, b.enhanced, sizeof(a.enhanced)), "PCM changed");
    require(!std::memcmp(a.features, b.features, sizeof(a.features)), "features changed");
    require(!std::memcmp(a.gains, b.gains, sizeof(a.gains)), "AGC gains changed");
}
void write_eq(const std::string &path, const std::string &magic, int count, float gain) {
    std::ofstream out(path);
    out << magic << " 16000 2 321\n";
    for (int i = 0; i < count; ++i) out << gain << '\n';
}
}

int main() {
    try {
        int16_t left[320], right[320], zero[320] = {};
        for (int i = 0; i < 320; ++i) {
            left[i] = static_cast<int16_t>(8000 * std::sin(i * .13));
            right[i] = static_cast<int16_t>(6000 * std::cos(i * .17));
        }
        int16_t original[320]; std::memcpy(original, left, sizeof(left));
        fsmn_kws::Frontend a, b;
        std::vector<fsmn_kws::FrontendOutput> history;
        for (int block = 0; block < 80; ++block) {
            auto out = a.step(left, right);
            equal(out, b.step(left, right, zero));
            require(out.block == static_cast<uint64_t>(block), "wrong block number");
            for (float value : out.features) require(std::isfinite(value), "non-finite feature");
            history.push_back(out);
        }
        require(!std::memcmp(original, left, sizeof(left)), "input was modified");
        a.reset();
        for (const auto &expected : history) equal(expected, a.step(left, right));
        require_error([&] { a.step(nullptr, right); }, "null microphone accepted");

        TempFile eq;
        write_eq(eq.path, "FSMN_KWS_INPUT_EQ_V1", 642, 1);
        fsmn_kws::Frontend calibrated(eq.path);
        b.reset();
        for (int block = 0; block < 20; ++block)
            equal(calibrated.step(left, right), b.step(left, right));
        write_eq(eq.path, "MIYA_INPUT_EQ_V1", 642, 1);
        fsmn_kws::Frontend compatible(eq.path);
        b.reset(); equal(compatible.step(left, right), b.step(left, right));
        write_eq(eq.path, "FSMN_KWS_INPUT_EQ_V1", 641, 1);
        require_error([&] { fsmn_kws::Frontend invalid(eq.path); }, "short EQ accepted");
        write_eq(eq.path, "FSMN_KWS_INPUT_EQ_V1", 643, 1);
        require_error([&] { fsmn_kws::Frontend invalid(eq.path); }, "trailing EQ accepted");
        write_eq(eq.path, "FSMN_KWS_INPUT_EQ_V1", 642, 0);
        require_error([&] { fsmn_kws::Frontend invalid(eq.path); }, "zero EQ accepted");
        std::cout << "frontend state, reset, input and calibration contracts passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
