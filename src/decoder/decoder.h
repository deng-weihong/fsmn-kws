// SPDX-License-Identifier: Apache-2.0
#ifndef FSMN_KWS_DECODER_INTERNAL_H
#define FSMN_KWS_DECODER_INTERNAL_H
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace fsmn_kws { namespace detail {
constexpr int N = 60;

inline void normalize(float *values, int count) {
    float total = 0;
    for (int i = 0; i < count; ++i) total += values[i];
    if (total < 1e-6f) std::fill_n(values, count, 1.0f / count);
    else for (int i = 0; i < count; ++i) values[i] /= total;
}

class Decoder {
    float transitions_[5][5], prior_[5], posterior_[5], joint_[5][5], viterbi_[5];
    int history_[5][N], path_[N];
    float prob_history_[5][N], weights_[2][N];
    int cursor_, opt_, state_, ignore_start_, ignore_length_, timer_, channel_;
    float level_, day_level_, confidence_;
    static constexpr float secondary_ = .01f, night_ = .96f;
    void normalize_transitions() {
        for (auto &row : transitions_) {
            float total = 0;
            for (float value : row) total += .001f + value;
            const float inverse = 1.0f / total;
            for (auto &value : row) value = (value + .001f) * inverse;
        }
    }
    void advance(const float *probabilities) {
        float total = 0;
        std::fill_n(posterior_, 5, 0.0f);
        for (int source = 0; source < 5; ++source) {
            for (int dest = 0; dest < 5; ++dest) {
                float value = (prior_[source] * transitions_[source][dest]) * probabilities[dest];
                joint_[source][dest] = value;
                total = value + total;
                posterior_[dest] += value;
            }
        }
        const float scale = total < 1e-6f ? 1e6f : 1.0f / total;
        for (auto &row : joint_) for (auto &value : row) value *= scale;
        normalize(posterior_, 5);
        for (int source = 0; source < 5; ++source) {
            float adaptive = .9f + (1.0f - .9f) * (1.0f - posterior_[source]);
            float divisor = std::max(1e-6f, posterior_[source]);
            for (int dest = 0; dest < 5; ++dest)
                transitions_[source][dest] = transitions_[source][dest] * adaptive
                    + ((1.0f - adaptive) * joint_[source][dest]) / divisor;
        }
        normalize_transitions();
        std::copy_n(posterior_, 5, prior_);
        float next[5], maximum = -std::numeric_limits<float>::max();
        for (int dest = 0; dest < 5; ++dest) {
            float best = 0;
            int predecessor = 0;
            for (int source = 0; source < 5; ++source) {
                float candidate = transitions_[source][dest] * viterbi_[source];
                if (candidate > best) { best = candidate; predecessor = source; }
            }
            next[dest] = best * probabilities[dest];
            history_[dest][cursor_] = predecessor;
            if (next[dest] > maximum) { maximum = next[dest]; opt_ = dest; }
        }
        normalize(next, 5);
        std::copy_n(next, 5, viterbi_);
        cursor_ = (cursor_ + 1) % N;
    }
    void boundary(int ignored, int &start, int &length) {
        bool active = false;
        int end = 0, zeros = 0;
        const int count = N - ignored;
        for (int index = count - 1; index >= 0; --index) {
            const int state = path_[ignored + index];
            if (state) {
                if (!active) { end = index; active = true; }
                else if (zeros)
                    std::fill_n(path_ + ignored + index + 1, zeros, state);
                zeros = 0;
            } else if (active) {
                if (++zeros > 2) {
                    start = index + zeros + ignored;
                    length = end - (index + zeros) + 1;
                    return;
                }
            }
        }
        start = zeros + ignored;
        length = end - zeros + 1;
    }
public:
    explicit Decoder(float level) : day_level_(level) {
        if (!std::isfinite(level) || level < 0 || level > 1)
            throw std::runtime_error("Miya HMM level must be in [0,1]");
        reset();
    }
    void reset() {
        for (int source = 0; source < 5; ++source) {
            std::fill_n(transitions_[source], 5, 1.0f);
            transitions_[source][source] += 5;
            transitions_[source][source == 4 ? 0 : source + 1] += 2.5f;
        }
        normalize_transitions();
        prior_[0] = .6f;
        std::fill_n(prior_ + 1, 4, .1f);
        std::copy_n(prior_, 5, viterbi_);
        std::memset(history_, 0, sizeof(history_));
        std::memset(path_, 0, sizeof(path_));
        std::memset(prob_history_, 0, sizeof(prob_history_));
        std::memset(weights_, 0, sizeof(weights_));
        cursor_ = opt_ = state_ = ignore_start_ = ignore_length_ = timer_ = channel_ = 0;
        confidence_ = 0;
        level_ = day_level_;
    }
    bool step(const float *probabilities, const float *weights, float &confidence, int &channel) {
        advance(probabilities);
        for (int state = 0; state < 5; ++state) {
            std::memmove(prob_history_[state], prob_history_[state] + 1, (N - 1) * sizeof(float));
            prob_history_[state][N - 1] = posterior_[state];
        }
        const int previous_opt = path_[N - 1];
        path_[N - 1] = opt_;
        for (int index = N - 2; index >= 0; --index) {
            int history_index = (index - N + 1 + cursor_ + N) % N;
            path_[index] = history_[path_[index + 1]][history_index];
        }
        if (ignore_start_ > 0) --ignore_start_;
        else if (ignore_length_ > 0) --ignore_length_;
        if (timer_ > 0 && --timer_ == 0 && level_ < night_) level_ = day_level_;
        int start, length;
        boundary(ignore_start_ + ignore_length_, start, length);
        int policy = 0;
        if (length >= 2) {
            float maxima[4] = {};
            int counts[4] = {};
            for (int index = start; index < start + length; ++index) {
                const int state = path_[index];
                if (state >= 1 && state <= 4) {
                    ++counts[state - 1];
                    maxima[state - 1] = std::max(maxima[state - 1], prob_history_[state][index]);
                }
            }
            const int weakest = static_cast<int>(std::min_element(maxima, maxima + 4) - maxima);
            float score = 1;
            int present = 0;
            for (int i = 0; i < 4; ++i) {
                if (i != weakest) score *= maxima[i];
                present += counts[i] > 0;
            }
            if (level_ == 0) policy = path_[N - 1] == 0 && present >= 3 ? 3 : 0;
            else if (score >= level_) policy = path_[N - 1] == 0 ? 2 : 1;
            if (secondary_ > 0 && level_ < night_ && previous_opt != 0 &&
                    path_[N - 1] == 0 && score >= secondary_) {
                level_ = secondary_; timer_ = 500;
                ignore_start_ = start; ignore_length_ = length;
            }
            if (policy) confidence_ = score;
        }
        int event = 0;
        if ((state_ == 0 || state_ == 2 || state_ == 3) && (policy == 1 || policy == 3))
            event = policy;
        else if (state_ == 0 && policy == 2) event = 3;
        else if (state_ == 1 && policy != 1) event = 2;
        state_ = policy;
        if (event == 2 || event == 3) { ignore_start_ = start; ignore_length_ = length; }
        for (int ch = 0; ch < 2; ++ch) {
            std::memmove(weights_[ch], weights_[ch] + 1, (N - 1) * sizeof(float));
            weights_[ch][N - 1] = weights[ch];
        }
        if (event == 1 || event == 3) {
            float best = 0;
            for (int ch = 0; ch < 2; ++ch) {
                float total = 0;
                for (int index = std::max(0, start); index < std::min(start + length, N); ++index)
                    total += weights_[ch][index];
                if (total > best) { best = total; channel_ = ch; }
            }
        }
        confidence = confidence_;
        channel = channel_;
        return event == 2 || event == 3;
    }
};

} } // namespace fsmn_kws::detail
#endif
