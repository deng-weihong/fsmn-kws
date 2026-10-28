// SPDX-License-Identifier: Apache-2.0
#include "fsmn_kws/frontend.h"
#include "wav_io.h"

#include <limits>

namespace {
void put16(unsigned char *p, uint16_t value) {
    p[0] = static_cast<unsigned char>(value);
    p[1] = static_cast<unsigned char>(value >> 8);
}
void put32(unsigned char *p, uint32_t value) {
    put16(p, static_cast<uint16_t>(value));
    put16(p + 2, static_cast<uint16_t>(value >> 16));
}
void write(FILE *file, const void *data, size_t bytes) {
    if (std::fwrite(data, 1, bytes, file) != bytes)
        throw std::runtime_error("cannot write enhanced WAV");
}
const char *usage = "usage: frontend_wav --wav input.wav --output enhanced.wav [--input-eq device.txt]\n";
}

int main(int argc, char **argv) {
    try {
        std::string input, output, equalizer;
        for (int i = 1; i < argc; ++i) {
            const std::string key = argv[i];
            if (key == "--help") { std::fputs(usage, stdout); return 0; }
            if (i + 1 >= argc) throw std::runtime_error("option needs a value");
            const std::string value = argv[++i];
            if (key == "--wav") input = value;
            else if (key == "--output") output = value;
            else if (key == "--input-eq") equalizer = value;
            else throw std::runtime_error("unknown option: " + key);
        }
        if (input.empty() || output.empty()) throw std::runtime_error(usage);
        fsmn_kws_example::Wav wav(input);
        const size_t samples = wav.bytes / (wav.channels * 2), blocks = samples / 320;
        if (!blocks) throw std::runtime_error("no complete audio blocks");
        if (blocks > (std::numeric_limits<uint32_t>::max() - 36) / 1280)
            throw std::runtime_error("output exceeds RIFF/WAVE size limit");
        fsmn_kws::Frontend frontend(equalizer);
        // Exclusive creation also protects the input and existing recordings.
        auto destination = fsmn_kws_example::open(output, "wbx");
        const uint32_t bytes = static_cast<uint32_t>(blocks * 1280);
        unsigned char header[44] = {};
        std::memcpy(header, "RIFF", 4); put32(header + 4, 36 + bytes);
        std::memcpy(header + 8, "WAVEfmt ", 8); put32(header + 16, 16);
        put16(header + 20, 1); put16(header + 22, 2);
        put32(header + 24, 16000); put32(header + 28, 64000);
        put16(header + 32, 4); put16(header + 34, 16);
        std::memcpy(header + 36, "data", 4); put32(header + 40, bytes);
        write(destination.get(), header, sizeof(header));
        for (size_t block = 0; block < blocks; ++block) {
            unsigned char raw[1920];
            fsmn_kws_example::read(wav.file.get(), raw, 320 * wav.channels * 2);
            int16_t mic0[320], mic1[320], reference[320];
            for (size_t i = 0; i < 320; ++i) {
                mic0[i] = static_cast<int16_t>(fsmn_kws_example::u16(raw + i * wav.channels * 2));
                mic1[i] = static_cast<int16_t>(fsmn_kws_example::u16(raw + (i * wav.channels + 1) * 2));
                reference[i] = wav.channels == 3
                    ? static_cast<int16_t>(fsmn_kws_example::u16(raw + (i * 3 + 2) * 2)) : 0;
            }
            const auto out = frontend.step(mic0, mic1, reference);
            unsigned char pcm[1280];
            for (size_t i = 0; i < 640; ++i) put16(pcm + i * 2, static_cast<uint16_t>(out.enhanced[i]));
            write(destination.get(), pcm, sizeof(pcm));
        }
        if (std::fflush(destination.get())) throw std::runtime_error("cannot flush enhanced WAV");
        std::printf("{\"type\":\"summary\",\"blocks\":%zu,\"audio_seconds\":%.3f,"
                    "\"trailing_samples_skipped\":%zu,\"input_equalization\":%s}\n",
                    blocks, blocks * .02, samples % 320, equalizer.empty() ? "false" : "true");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "frontend_wav: %s\n", error.what());
        return 1;
    }
}
