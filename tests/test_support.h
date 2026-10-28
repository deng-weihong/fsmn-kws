// SPDX-License-Identifier: Apache-2.0
#ifndef FSMN_KWS_TEST_SUPPORT_H
#define FSMN_KWS_TEST_SUPPORT_H
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <unistd.h>

inline void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename F> void require_error(F action, const char *message) {
    bool rejected = false;
    try { action(); } catch (const std::exception &) { rejected = true; }
    require(rejected, message);
}

class TempFile {
public:
    std::string path;
    TempFile() {
        char pattern[] = "/tmp/fsmn-kws-test-XXXXXX";
        int descriptor = mkstemp(pattern);
        if (descriptor < 0) throw std::runtime_error("cannot create test fixture");
        close(descriptor);
        path = pattern;
    }
    ~TempFile() { std::remove(path.c_str()); }
    TempFile(const TempFile &) = delete;
    TempFile &operator=(const TempFile &) = delete;
};

inline void write_model(const std::string &path) {
    std::ofstream file(path);
    const auto word = [&](float value) {
        int32_t bits;
        std::memcpy(&bits, &value, 4);
        file << bits << '\n';
    };
    const int header[][10] = {
        {0,1,120,5,3,0,0,0,0,0}, {1,0,120,144,1,1,0,0,0,0},
        {5,0,144,68,16,1,5,4,0,0}, {1,1,144,5,1,4,0,0,0,0}};
    for (const auto &row : header) for (int value : row) word(static_cast<float>(value));
    const auto matrix = [&](int rows, int cols, float value) {
        for (int row = 0; row < rows; ++row)
            for (int col = 0; col < (cols + 3) / 4 * 4; ++col)
                word(col < cols ? value : 0);
    };
    matrix(144, 120, 0);
    matrix(1, 144, .1f);
    for (int layer = 0; layer < 5; ++layer) {
        matrix(68, 144, .001f);
        matrix(16, 68, .01f);
        matrix(1, 68, .02f);
        matrix(144, 68, .001f);
        matrix(1, 144, .1f);
    }
    matrix(5, 144, .001f);
    // Class zero gets a known logit offset; padding is not a sixth class.
    for (int i = 0; i < 8; ++i) word(i == 0 ? 1.0f : 0.0f);
    require(static_cast<bool>(file), "cannot write synthetic model");
}
#endif
