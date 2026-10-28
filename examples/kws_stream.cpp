// SPDX-License-Identifier: Apache-2.0
#include "fsmn_kws/engine.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <time.h>

namespace {
double seconds(clockid_t clock) {
    timespec value;
    if (clock_gettime(clock, &value)) throw std::runtime_error("clock_gettime failed");
    return value.tv_sec + value.tv_nsec * 1e-9;
}
const char *usage = "usage: kws_stream --model model.txt [--channels 2] [--level .81] [--input-eq device.txt]\n"
                    "stdin: consecutive 16 kHz PCM16_LE frames, 1/2 mics or 2 mics + reference\n";
}

int main(int argc, char **argv) {
    try {
        std::string model, equalizer;
        unsigned channels = 2;
        float level = .81f;
        for (int i = 1; i < argc; ++i) {
            const std::string key = argv[i];
            if (key == "--help") { std::fputs(usage, stdout); return 0; }
            if (i + 1 >= argc) throw std::runtime_error("option needs a value");
            const std::string value = argv[++i];
            if (key == "--model") model = value;
            else if (key == "--input-eq") equalizer = value;
            else if (key == "--channels" && (value == "1" || value == "2" || value == "3"))
                channels = static_cast<unsigned>(value[0] - '0');
            else if (key == "--level") {
                size_t used = 0;
                level = std::stof(value, &used);
                if (used != value.size() || !std::isfinite(level) || level < 0 || level > 1)
                    throw std::runtime_error("level must be in [0, 1]");
            } else throw std::runtime_error("invalid option: " + key);
        }
        if (model.empty()) throw std::runtime_error(usage);
        setvbuf(stdout, nullptr, _IOLBF, 0);
        fsmn_kws::Engine engine(model, level, equalizer);
        std::printf("{\"type\":\"ready\",\"device\":\"cpu\",\"precision\":\"fp32\","
                    "\"sample_rate\":16000,\"channels\":%u,\"level\":%.3f,\"input_equalization\":%s}\n",
                    channels, level, equalizer.empty() ? "false" : "true");
        uint64_t blocks = 0, events = 0, clipped[2] = {};
        double sums[2] = {}, squares[2] = {};
        const double started = seconds(CLOCK_MONOTONIC);
        const double cpu_started = seconds(CLOCK_PROCESS_CPUTIME_ID);
        size_t trailing = 0;
        for (;;) {
            unsigned char bytes[1920];
            const size_t wanted = 320 * channels * 2;
            const size_t received = std::fread(bytes, 1, wanted, stdin);
            if (received != wanted) {
                if (std::ferror(stdin)) throw std::runtime_error(std::strerror(errno));
                trailing = received;
                break;
            }
            int16_t mic0[320], mic1[320], reference[320];
            const auto sample = [&](size_t at) {
                return static_cast<int16_t>(static_cast<unsigned>(bytes[at * 2]) |
                                           (static_cast<unsigned>(bytes[at * 2 + 1]) << 8));
            };
            for (size_t i = 0; i < 320; ++i) {
                mic0[i] = sample(i * channels);
                mic1[i] = sample(i * channels + (channels > 1 ? 1 : 0));
                reference[i] = channels == 3 ? sample(i * channels + 2) : 0;
                const int16_t values[2] = {mic0[i], mic1[i]};
                for (int ch = 0; ch < 2; ++ch) {
                    const double value = values[ch] / 32768.0;
                    sums[ch] += value;
                    squares[ch] += value * value;
                    clipped[ch] += values[ch] == -32768 || values[ch] == 32767;
                }
            }
            if (blocks == 0) std::puts("{\"type\":\"connected\",\"message\":\"audio connected\"}");
            const auto out = engine.step(mic0, mic1, reference);
            ++blocks;
            if (out.detected) {
                ++events;
                std::printf("{\"type\":\"wake\",\"message\":\"keyword detected\",\"count\":%llu,"
                            "\"block\":%llu,\"time_seconds\":%.3f,\"confidence\":%.9g,\"channel\":%d}\n",
                    static_cast<unsigned long long>(events), static_cast<unsigned long long>(out.frontend.block),
                    out.frontend.block * .02, out.confidence, out.channel);
            }
            if (blocks % 100 == 0) {
                double db[2];
                for (int ch = 0; ch < 2; ++ch) {
                    const double mean = sums[ch] / 32000;
                    db[ch] = 10 * std::log10(std::max(1e-12, squares[ch] / 32000 - mean * mean));
                }
                std::printf("{\"type\":\"listening\",\"time_seconds\":%.2f,\"rms_ac_dbfs\":[%.2f,%.2f],"
                            "\"clipped_samples\":[%llu,%llu],\"events\":%llu}\n", blocks * .02, db[0], db[1],
                    static_cast<unsigned long long>(clipped[0]), static_cast<unsigned long long>(clipped[1]),
                    static_cast<unsigned long long>(events));
                for (int ch = 0; ch < 2; ++ch) { sums[ch] = 0; squares[ch] = 0; clipped[ch] = 0; }
            }
        }
        const double wall = seconds(CLOCK_MONOTONIC) - started;
        const double cpu = seconds(CLOCK_PROCESS_CPUTIME_ID) - cpu_started;
        std::printf("{\"type\":\"summary\",\"blocks\":%llu,\"audio_seconds\":%.3f,\"events\":%llu,"
                    "\"trailing_bytes_skipped\":%zu,\"wall_seconds\":%.3f,\"cpu_seconds\":%.3f}\n",
            static_cast<unsigned long long>(blocks), blocks * .02, static_cast<unsigned long long>(events),
            trailing, wall, cpu);
        if (!blocks) throw std::runtime_error("no complete audio blocks received");
        if (trailing % (channels * 2)) throw std::runtime_error("truncated PCM sample frame");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "kws_stream: %s\n", error.what());
        return 1;
    }
}
