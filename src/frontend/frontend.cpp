// SPDX-License-Identifier: Apache-2.0
#include "fsmn_kws/frontend.h"
#include "kernels.h"
#include "coefficients.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace fsmn_kws {
namespace {
constexpr int H = 320, B = 40;

class InputEqualizer {
    std::vector<float> gains_;
public:
    explicit InputEqualizer(const std::string &path) {
        if (path.empty()) return;
        std::ifstream file(path);
        std::string magic;
        int rate = 0, channels = 0, bins = 0;
        if (!(file >> magic >> rate >> channels >> bins) ||
                (magic != "FSMN_KWS_INPUT_EQ_V1" && magic != "MIYA_INPUT_EQ_V1") || rate != 16000 || channels != 2 || bins != H + 1)
            throw std::runtime_error("invalid Miya input equalizer header: " + path);
        gains_.resize(2 * (H + 1));
        for (auto &gain : gains_)
            if (!(file >> gain) || !std::isfinite(gain) || gain < .01f || gain > 32.0f)
                throw std::runtime_error("invalid Miya input equalizer gain: " + path);
        file >> std::ws;
        if (!file.eof()) throw std::runtime_error("trailing Miya input equalizer data: " + path);
    }
    void apply(float *spectrum) const {
        if (gains_.empty()) return;
        for (int ch = 0; ch < 2; ++ch) {
            const float *gain = gains_.data() + ch * (H + 1);
            float *values = spectrum + ch * H * 2;
            // Packed KissFFT: Re(DC) is DC, Im(DC) is the real Nyquist bin.
            values[0] *= gain[0];
            values[1] *= gain[H];
            for (int bin = 1; bin < H; ++bin) {
                values[bin * 2] *= gain[bin];
                values[bin * 2 + 1] *= gain[bin];
            }
        }
    }
};

int16_t pcm16(float value) {
    const float scaled = value * 32768.0f + .5f;
    if (!std::isfinite(scaled)) throw std::runtime_error("non-finite frontend PCM");
    if (scaled >= 32767.0f) return 32767;
    if (scaled <= -32768.0f) return -32768;
    return static_cast<int16_t>(std::floor(scaled));
}

} // namespace

class Frontend::Impl {
    InputEqualizer input_equalizer_;
    std::unique_ptr<void, decltype(&fsmn_kws_fft_free)> fft_;
    float hpf_[18] = {}, analysis_[6][3][H] = {}, previous_[3] = {};
    float synthesis_[6][2][H] = {}, deemphasis_[2] = {}, auxiva_[40 * H];
    std::vector<float> draec_;
    uint32_t control_[2];
    // uint64_t storage ensures alignment for the fixed-point AGC structure.
    std::array<std::vector<uint64_t>, 2> agc_;
    uint64_t block_ = 0;
public:
    explicit Impl(const std::string &input_equalizer)
        : input_equalizer_(input_equalizer), fft_(fsmn_kws_fft_create(640), &fsmn_kws_fft_free),
          draec_(fsmn_kws_draec_state_size()) {
        if (!fft_) throw std::runtime_error("cannot allocate Miya FFT plans");
        for (auto &state : agc_) state.resize((fsmn_kws_agc_state_size() + 7) / 8);
        reset();
    }
    void reset() {
        std::memset(hpf_, 0, sizeof(hpf_));
        std::memset(analysis_, 0, sizeof(analysis_));
        std::memset(previous_, 0, sizeof(previous_));
        std::memset(synthesis_, 0, sizeof(synthesis_));
        std::memset(deemphasis_, 0, sizeof(deemphasis_));
        fsmn_kws_auxiva_init(auxiva_, H, 1e-5f);
        fsmn_kws_draec_init(draec_.data(), control_);
        for (auto &state : agc_)
            if (fsmn_kws_agc_init(state.data())) throw std::runtime_error("Miya AGC init failed");
        block_ = 0;
    }
    FrontendOutput step(const int16_t *mic0, const int16_t *mic1, const int16_t *reference) {
        if (!mic0 || !mic1) throw std::runtime_error("Miya needs two microphone channels");
        FrontendOutput out = {};
        out.block = block_++;
        float audio[H * 3], filtered[H * 3], time[3 * H * 2], analyzed[3 * H * 2];
        for (int i = 0; i < H; ++i) {
            audio[i * 3] = mic0[i] / 32768.0f;
            audio[i * 3 + 1] = mic1[i] / 32768.0f;
            audio[i * 3 + 2] = reference ? reference[i] / 32768.0f : 0;
        }
        fsmn_kws_hpf(hpf_, 3, kHpfNumerator, kHpfFeedback, audio, filtered, H);
        for (int ch = 0; ch < 3; ++ch) {
            float last = previous_[ch];
            for (int i = 0; i < H; ++i) {
                float value = filtered[i * 3 + ch];
                analysis_[5][ch][i] = value - .9f * last;
                last = value;
            }
            previous_[ch] = last;
            for (int i = 0; i < H; ++i) {
                float first = 0, second = 0;
                for (int phase = 0; phase < 6; phase += 2) {
                    first += analysis_[phase][ch][i] * kAnalysisCoefficients[phase * H + i];
                    second += analysis_[phase + 1][ch][i] * kAnalysisCoefficients[(phase + 1) * H + i];
                }
                time[ch * H * 2 + i] = first;
                time[ch * H * 2 + H + i] = second;
            }
        }
        fsmn_kws_fft_forward(fft_.get(), time, analyzed, 3);
        input_equalizer_.apply(analyzed);
        float residual[2 * H * 2], echo[2 * H * 2], separated[2 * H * 2];
        fsmn_kws_draec_step(draec_.data(), control_, analyzed, analyzed + 2 * H * 2, residual, echo);
        fsmn_kws_auxiva_step(auxiva_, H, .999f, residual, separated);
        float inverse[2 * H * 2], before[2 * H];
        fsmn_kws_fft_inverse(fft_.get(), separated, inverse, 2, 1);
        fsmn_kws_ppf_synthesis(&synthesis_[0][0][0], deemphasis_, 2, 6, H,
            kSynthesisCoefficients, .9f, inverse, before);
        for (int ch = 0; ch < 2; ++ch) {
            int16_t pcm[H], enhanced[H];
            for (int i = 0; i < H; ++i) pcm[i] = pcm16(before[ch * H + i]);
            if (fsmn_kws_agc_process(agc_[ch].data(), pcm, enhanced, 2))
                throw std::runtime_error("Miya AGC process failed");
            float input_l1 = 0, output_l1 = 0;
            for (int i = 0; i < H; ++i) {
                input_l1 += std::abs(before[ch * H + i]);
                output_l1 += std::abs(enhanced[i] / 32768.0f);
                out.enhanced[i * 2 + ch] = enhanced[i];
            }
            float gain = input_l1 >= 1e-6f ? output_l1 / input_l1 : 1;
            out.gains[ch] = gain;
            float power[H];
            for (int i = 0; i < H; ++i) {
                float real = separated[(ch * H + i) * 2] * gain;
                float imag = separated[(ch * H + i) * 2 + 1] * gain;
                power[i] = real * real + imag * imag;
            }
            for (int bin = 0; bin < B; ++bin) {
                const int left = kMelBounds[bin], center = kMelBounds[bin + 1], right = kMelBounds[bin + 2];
                float energy = 0;
                for (int i = left; i < center; ++i)
                    energy += static_cast<float>(i - left) * power[i] / static_cast<float>(center - left);
                for (int i = center; i < right; ++i)
                    energy += static_cast<float>(right - i) * power[i] / static_cast<float>(right - center);
                out.features[ch * B + bin] = std::max(10.0f * std::log10(energy), -75.0f);
            }
        }
        // Match np.roll(history, -1): old phase zero is retained at the tail,
        // then overwritten by the next block.
        float oldest[3 * H];
        std::memcpy(oldest, analysis_[0], sizeof(oldest));
        std::memmove(analysis_[0], analysis_[1], 5 * sizeof(analysis_[0]));
        std::memcpy(analysis_[5], oldest, sizeof(oldest));
        return out;
    }
};

Frontend::Frontend(const std::string &equalizer) : impl_(new Impl(equalizer)) {}
Frontend::~Frontend() = default;
void Frontend::reset() { impl_->reset(); }
FrontendOutput Frontend::step(const int16_t *mic0, const int16_t *mic1, const int16_t *reference) {
    return impl_->step(mic0, mic1, reference);
}
} // namespace fsmn_kws
