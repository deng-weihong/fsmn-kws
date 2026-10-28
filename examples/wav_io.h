// SPDX-License-Identifier: Apache-2.0
#ifndef FSMN_KWS_EXAMPLE_WAV_IO_H
#define FSMN_KWS_EXAMPLE_WAV_IO_H
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace fsmn_kws_example {
using File = std::unique_ptr<FILE, int (*)(FILE *)>;
inline File open(const std::string &path, const char *mode) {
    FILE *file = std::fopen(path.c_str(), mode);
    if (!file) throw std::runtime_error("cannot open " + path + ": " + std::strerror(errno));
    return File(file, std::fclose);
}
inline uint16_t u16(const unsigned char *p) { return p[0] | (static_cast<uint16_t>(p[1]) << 8); }
inline uint32_t u32(const unsigned char *p) { return u16(p) | (static_cast<uint32_t>(u16(p + 2)) << 16); }
inline void read(FILE *file, void *data, size_t bytes) {
    if (std::fread(data, 1, bytes, file) != bytes) throw std::runtime_error("truncated WAV");
}
struct Wav {
    File file;
    unsigned channels = 0;
    uint32_t bytes = 0;
    long data_offset = 0;
    explicit Wav(const std::string &path) : file(open(path, "rb")) {
        unsigned char header[12]; read(file.get(), header, 12);
        if (std::memcmp(header, "RIFF", 4) || std::memcmp(header + 8, "WAVE", 4))
            throw std::runtime_error("expected RIFF/WAVE");
        bool format = false;
        for (;;) {
            unsigned char chunk[8]; read(file.get(), chunk, 8);
            const uint32_t size = u32(chunk + 4);
            if (!std::memcmp(chunk, "fmt ", 4)) {
                if (size < 16 || size > 4096) throw std::runtime_error("invalid WAV fmt");
                std::vector<unsigned char> value(size); read(file.get(), value.data(), size);
                channels = u16(value.data() + 2);
                if (u16(value.data()) != 1 || u32(value.data() + 4) != 16000 ||
                    u16(value.data() + 14) != 16 || (channels != 2 && channels != 3) ||
                    u16(value.data() + 12) != channels * 2)
                    throw std::runtime_error("expected 16 kHz PCM16, two mics plus optional reference");
                format = true;
            } else if (!std::memcmp(chunk, "data", 4)) {
                if (!format || size % (channels * 2)) throw std::runtime_error("invalid WAV data");
                bytes = size; data_offset = std::ftell(file.get()); return;
            } else if (std::fseek(file.get(), size, SEEK_CUR)) {
                throw std::runtime_error("cannot skip WAV chunk");
            }
            if ((size & 1) && std::fseek(file.get(), 1, SEEK_CUR)) throw std::runtime_error("WAV alignment");
        }
    }
};

} // namespace fsmn_kws_example
#endif
