// SPDX-License-Identifier: Apache-2.0
#ifndef FSMN_KWS_MODEL_INTERNAL_H
#define FSMN_KWS_MODEL_INTERNAL_H
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fsmn_kws { namespace detail {
constexpr int D = 144, P = 68, L = 16;

class Reader {
    std::vector<float> data_;
    size_t position_ = 0;
public:
    explicit Reader(const std::string &path) {
        std::ifstream file(path);
        if (!file) throw std::runtime_error("cannot read Miya model: " + path);
        int64_t word;
        while (file >> word) {
            if (word < INT32_MIN || word > INT32_MAX)
                throw std::runtime_error("invalid model integer");
            int32_t bits = static_cast<int32_t>(word);
            float value;
            std::memcpy(&value, &bits, 4);
            if (!std::isfinite(value)) throw std::runtime_error("non-finite model weight");
            data_.push_back(value);
        }
        if (!file.eof()) throw std::runtime_error("invalid model text");
    }
    std::vector<float> take(size_t count) {
        if (count > data_.size() - position_) throw std::runtime_error("truncated model");
        std::vector<float> out(data_.begin() + position_, data_.begin() + position_ + count);
        position_ += count;
        return out;
    }
    std::vector<float> matrix(int rows, int columns) {
        const int padded = (columns + 3) / 4 * 4;
        auto raw = take(rows * padded);
        std::vector<float> out(rows * columns);
        for (int row = 0; row < rows; ++row)
            std::copy_n(raw.data() + row * padded, columns, out.data() + row * columns);
        return out;
    }
    std::vector<float> bias(int count) {
        auto out = take((count + 3) / 4 * 4);
        out.resize(count);
        return out;
    }
    bool finished() const { return position_ == data_.size(); }
};

inline void dense(const float *input, const std::vector<float> &weight,
           const std::vector<float> &bias, int rows, int columns,
           float *output, bool relu = true) {
    for (int row = 0; row < rows; ++row) {
        float sum = 0;
        const float *w = weight.data() + row * columns;
        for (int col = 0; col < columns; ++col) sum += input[col] * w[col];
        sum += bias[row];
        output[row] = relu ? std::max(sum, 0.0f) : sum;
    }
}

struct Layer {
    std::vector<float> shrink, left, right, expand, bias;
    // Includes the current projection at slot L. Earlier L slots are history.
    float history[(L + 1) * 2 * P] = {};
    void reset() { std::memset(history, 0, sizeof(history)); }
    void step(const float *input, int channels, float *output) {
        const int width = channels * P;
        float *projected = history + L * width;
        for (int ch = 0; ch < channels; ++ch) {
            for (int row = 0; row < P; ++row) {
                float sum = 0;
                for (int col = 0; col < D; ++col)
                    sum += input[ch * D + col] * shrink[row * D + col];
                projected[ch * P + row] = sum;
            }
            float memory[P];
            std::copy_n(history + (L - 1) * width + ch * P, P, memory);
            for (int row = 0; row < L; ++row)
                for (int col = 0; col < P; ++col)
                    memory[col] += history[row * width + ch * P + col] * left[row * P + col];
            for (int col = 0; col < P; ++col)
                memory[col] += projected[ch * P + col] * right[col];
            dense(memory, expand, bias, D, P, output + ch * D);
        }
        std::memmove(history, history + width, L * width * sizeof(float));
    }
};

class Model {
    std::vector<float> feature_w_, feature_b_, decision_w_, decision_b_;
    std::array<Layer, 5> layers_;
public:
    explicit Model(const std::string &path) {
        Reader reader(path);
        const auto header = reader.take(40);
        const int expected[][10] = {
            {0,1,120,5,3,0,0,0,0,0},
            {1,0,120,144,1,1,0,0,0,0},
            {5,0,144,68,16,1,5,4,0,0},
            {1,1,144,5,1,4,0,0,0,0}};
        // Check defined fields; reserved header words need not be zero.
        const int sizes[] = {5,6,8,6};
        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < sizes[row]; ++col)
                if (header[row * 10 + col] != expected[row][col])
                    throw std::runtime_error("unsupported Miya model topology");
        feature_w_ = reader.matrix(D, 120);
        feature_b_ = reader.bias(D);
        for (auto &layer : layers_) {
            layer.shrink = reader.matrix(P, D);
            layer.left = reader.matrix(L, P);
            layer.right = reader.matrix(1, P);
            layer.expand = reader.matrix(D, P);
            layer.bias = reader.bias(D);
        }
        decision_w_ = reader.matrix(5, D);
        decision_b_ = reader.bias(5);
        if (!reader.finished()) throw std::runtime_error("trailing model data");
    }
    void reset() { for (auto &layer : layers_) layer.reset(); }
    void step(const float *context, float *posterior, float *channel_weights) {
        float value[2 * D], next[2 * D];
        for (int ch = 0; ch < 2; ++ch)
            dense(context + ch * 120, feature_w_, feature_b_, D, 120, value + ch * D);
        // All five layers advance on frame zero, including their bias histories.
        for (auto &layer : layers_) {
            layer.step(value, 2, next);
            std::copy_n(next, 2 * D, value);
        }
        float selected[D];
        int counts[2] = {}, positives = 0;
        for (int i = 0; i < D; ++i) {
            const int winner = value[D + i] > value[i] ? 1 : 0;
            selected[i] = value[winner * D + i];
            if (selected[i] > 0) { ++positives; ++counts[winner]; }
        }
        const float inverse = positives ? 1.0f / positives : 0;
        for (int ch = 0; ch < 2; ++ch) channel_weights[ch] = counts[ch] * inverse;
        dense(selected, decision_w_, decision_b_, 5, D, posterior, false);
        float maximum = *std::max_element(posterior, posterior + 5), total = 0;
        for (int i = 0; i < 5; ++i) {
            posterior[i] = std::exp(posterior[i] - maximum);
            total += posterior[i];
        }
        for (int i = 0; i < 5; ++i) posterior[i] /= total;
    }
};

} } // namespace fsmn_kws::detail
#endif
