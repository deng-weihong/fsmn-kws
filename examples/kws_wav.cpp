// SPDX-License-Identifier: Apache-2.0
#include "fsmn_kws/engine.h"
#include "wav_io.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <sys/resource.h>
#include <time.h>

namespace {
const char *usage = "usage: kws_wav --model model.txt --wav input.wav [--input-eq device.txt] [--dump-prefix path] [--level .81] [--repeat 1]\n";
using fsmn_kws_example::File;
using fsmn_kws_example::open;
using fsmn_kws_example::read;
using fsmn_kws_example::Wav;
double seconds(clockid_t clock) {
    timespec value;
    if (clock_gettime(clock, &value)) throw std::runtime_error("clock_gettime failed");
    return value.tv_sec + value.tv_nsec * 1e-9;
}
void write(FILE *file, const void *data, size_t bytes) {
    if (file && std::fwrite(data, 1, bytes, file) != bytes) throw std::runtime_error("dump write failed");
}
}

int main(int argc, char **argv) {
    try {
        std::string model, wav_path, prefix, input_equalizer;
        float level = .81f;
        unsigned repeat = 1;
        for (int i = 1; i < argc; ++i) {
            if (std::string(argv[i]) == "--help") { std::fputs(usage, stdout); return 0; }
            if (i + 1 >= argc) throw std::runtime_error("option needs a value");
            const std::string key = argv[i], value = argv[++i];
            if (key == "--model") model = value;
            else if (key == "--wav") wav_path = value;
            else if (key == "--dump-prefix") prefix = value;
            else if (key == "--input-eq") input_equalizer = value;
            else if (key == "--level") {
                size_t consumed = 0; level = std::stof(value, &consumed);
                if (consumed != value.size()) throw std::runtime_error("invalid level");
            } else if (key == "--repeat") {
                char *end = nullptr;
                unsigned long count = std::strtoul(value.c_str(), &end, 10);
                if (!end || *end || count < 1 || count > 1000) throw std::runtime_error("invalid repeat");
                repeat = count;
            } else throw std::runtime_error("unknown option: " + key);
        }
        if (model.empty() || wav_path.empty())
            throw std::runtime_error(usage);
        Wav wav(wav_path);
        const double load_started = seconds(CLOCK_MONOTONIC);
        fsmn_kws::Engine engine(model, level, input_equalizer);
        const double load_seconds = seconds(CLOCK_MONOTONIC) - load_started;
        std::vector<File> files;
        const char *suffixes[] = {"enhanced.pcm", "features.f32", "gains.f32", "context.f32", "posterior.f32", "weights.f32"};
        for (auto suffix : suffixes)
            files.push_back(prefix.empty() ? File(nullptr, std::fclose) : open(prefix + "." + suffix, "wb"));
        const size_t samples = wav.bytes / (wav.channels * 2), blocks = samples / 320;
        std::vector<double> steps;
        steps.reserve(blocks * repeat);
        unsigned events = 0;
        const double started = seconds(CLOCK_MONOTONIC), cpu_started = seconds(CLOCK_PROCESS_CPUTIME_ID);
        for (unsigned run = 0; run < repeat; ++run) {
            engine.reset();
            if (std::fseek(wav.file.get(), wav.data_offset, SEEK_SET)) throw std::runtime_error("cannot rewind WAV");
            for (size_t block = 0; block < blocks; ++block) {
                int16_t pcm[960], mic0[320], mic1[320], reference[320];
                read(wav.file.get(), pcm, 320 * wav.channels * 2);
                for (size_t i = 0; i < 320; ++i) {
                    mic0[i] = pcm[i * wav.channels]; mic1[i] = pcm[i * wav.channels + 1];
                    reference[i] = wav.channels == 3 ? pcm[i * 3 + 2] : 0;
                }
                const double begin = seconds(CLOCK_MONOTONIC);
                const auto out = engine.step(mic0, mic1, reference);
                steps.push_back((seconds(CLOCK_MONOTONIC) - begin) * 1000);
                write(files[0].get(), out.frontend.enhanced, sizeof(out.frontend.enhanced));
                write(files[1].get(), out.frontend.features, sizeof(out.frontend.features));
                write(files[2].get(), out.frontend.gains, sizeof(out.frontend.gains));
                if (out.evaluated) {
                    write(files[3].get(), out.context, sizeof(out.context));
                    write(files[4].get(), out.posterior, sizeof(out.posterior));
                    write(files[5].get(), out.channel_weights, sizeof(out.channel_weights));
                }
                if (out.detected) {
                    ++events;
                    std::printf("{\"type\":\"wake\",\"run\":%u,\"block\":%llu,\"time_seconds\":%.3f,\"confidence\":%.9g,\"channel\":%d}\n",
                        run, static_cast<unsigned long long>(out.frontend.block), out.frontend.block * .02, out.confidence, out.channel);
                }
            }
        }
        const double wall = seconds(CLOCK_MONOTONIC) - started;
        const double cpu = seconds(CLOCK_PROCESS_CPUTIME_ID) - cpu_started;
        const double audio = blocks * .02 * repeat;
        std::sort(steps.begin(), steps.end());
        const auto quantile = [&](double q) { return steps.empty() ? 0.0 : steps[static_cast<size_t>((steps.size() - 1) * q)]; };
        rusage usage = {};
        getrusage(RUSAGE_SELF, &usage);
        for (auto &file : files)
            if (file && std::fflush(file.get())) throw std::runtime_error("dump flush failed");
        std::printf("{\"type\":\"summary\",\"backend\":\"reconstructed-default-021\",\"precision\":\"fp32\",\"device\":\"cpu\","
                    "\"input_equalization\":%s,\"runs\":%u,\"blocks\":%zu,\"audio_seconds\":%.4f,\"trailing_samples_skipped\":%zu,"
                    "\"events\":%u,\"load_seconds\":%.6f,\"wall_seconds\":%.6f,\"cpu_seconds\":%.6f,"
                    "\"wall_rtf\":%.6f,\"cpu_rtf\":%.6f,\"step_p50_ms\":%.6f,\"step_p95_ms\":%.6f,"
                    "\"step_max_ms\":%.6f,\"max_rss_kib\":%ld}\n",
                    input_equalizer.empty() ? "false" : "true", repeat, blocks * repeat, audio, samples % 320, events, load_seconds, wall, cpu,
                    audio ? wall / audio : 0, audio ? cpu / audio : 0,
                    quantile(.5), quantile(.95), quantile(1), usage.ru_maxrss);
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "kws_wav: %s\n", error.what());
        return 1;
    }
}
