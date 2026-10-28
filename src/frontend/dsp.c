#include <stddef.h>
#include <math.h>
#include <string.h>

/* Independent third-order direct-form high-pass reconstruction. Compile with
 * -ffp-contract=off and without fast-math: float32 addition order is stateful.
 * history[channel][0..5] = x[-1..-3], y[-1..-3]. Coefficients are supplied by
 * the audit, not linked to or called from the proprietary binary.
 */
void fsmn_kws_hpf(float *history, size_t channels, const float *b,
                   const float *a, const float *input, float *output,
                   size_t samples)
{
    for (size_t channel = 0; channel < channels; ++channel) {
        float *h = history + channel * 6;
        for (size_t t = 0; t < samples; ++t) {
            float x = input[t * channels + channel];
            float feedforward = (((b[0] * x + 0.0f) + b[1] * h[0]) +
                                 b[2] * h[1]) + b[3] * h[2];
            float feedback = ((a[0] * h[3] + 0.0f) + a[1] * h[4]) + a[2] * h[5];
            float y = feedforward - feedback;
            h[2] = h[1]; h[1] = h[0]; h[0] = x;
            h[5] = h[4]; h[4] = h[3]; h[3] = y;
            output[t * channels + channel] = y;
        }
    }
}

/* One inverse-transform frame -> polyphase overlap/add -> de-emphasis.
 * Inputs are channel-major normalized inverse transforms of length 2 * hop;
 * history is phase/channel/sample, coefficients are phase/sample, and the
 * previous value is the final de-emphasized sample per channel.
 */
void fsmn_kws_ppf_synthesis(float *history, float *previous,
                             size_t channels, size_t phases, size_t hop,
                             const float *coefficients, float deemphasis,
                             const float *input, float *output)
{
    size_t block = channels * hop;
    for (size_t phase = 0; phase < phases; ++phase)
        for (size_t channel = 0; channel < channels; ++channel)
            for (size_t sample = 0; sample < hop; ++sample) {
                size_t index = phase * block + channel * hop + sample;
                float value = input[channel * 2 * hop + (phase % 2) * hop + sample];
                history[index] += value * coefficients[phase * hop + sample];
            }
    for (size_t channel = 0; channel < channels; ++channel) {
        float last = previous[channel];
        for (size_t sample = 0; sample < hop; ++sample) {
            last = last * deemphasis + history[channel * hop + sample];
            output[channel * hop + sample] = last;
        }
        previous[channel] = last;
    }
    memmove(history, history + block, (phases - 1) * block * sizeof(float));
    memset(history + (phases - 1) * block, 0, block * sizeof(float));
}

typedef struct { float re, im; } complex32;

static complex32 dot_two(const complex32 *matrix, const complex32 *vector,
                         size_t row, size_t bin, size_t bins)
{
    complex32 result = {0, 0};
    for (size_t column = 0; column < 2; ++column) {
        complex32 a = matrix[(row * 2 + column) * bins + bin];
        complex32 b = vector[column * bins + bin];
        result.re += a.re * b.re - a.im * b.im;
        result.im += b.im * a.re + a.im * b.re;
    }
    return result;
}

/* Independent two-microphone AuxIVA reconstruction for the observed default
 * path (no external pilot). State consists of per-source inverse correlations,
 * demixing, mixing, and scaled demixing matrices, all complex float32.
 */
void fsmn_kws_auxiva_init(float *storage, size_t bins, float diagload)
{
    complex32 *state = (complex32 *)storage;
    memset(storage, 0, 40 * bins * sizeof(float));
    for (size_t bin = 0; bin < bins; ++bin) {
        for (size_t source = 0; source < 2; ++source) {
            for (size_t row = 0; row < 2; ++row)
                state[(source * 4 + row * 3) * bins + bin].re = 1.0f / diagload;
            for (size_t matrix = 0; matrix < 3; ++matrix)
                state[(8 + matrix * 4 + source * 3) * bins + bin].re = 1.0f;
        }
    }
}

void fsmn_kws_auxiva_step(float *storage, size_t bins, float forget,
                           const float *input, float *output)
{
    complex32 *inverse = (complex32 *)storage;
    complex32 *demix = inverse + 8 * bins;
    complex32 *mix = demix + 4 * bins;
    complex32 *scaled = mix + 4 * bins;
    const complex32 *x = (const complex32 *)input;
    complex32 *y = (complex32 *)output;
    float energy[2] = {0, 0};
    for (size_t source = 0; source < 2; ++source) {
        for (size_t bin = 0; bin < bins; ++bin) {
            complex32 raw = dot_two(demix, x, source, bin, bins);
            energy[source] += raw.re * raw.re + raw.im * raw.im;
            y[source * bins + bin] = dot_two(scaled, x, source, bin, bins);
        }
    }
    for (size_t source = 0; source < 2; ++source) {
        float beta = (1.0f - forget) * powf(fmaxf(energy[source], 0.01f), -0.9f);
        complex32 *p = inverse + source * 4 * bins;
        for (size_t bin = 0; bin < bins; ++bin) {
            complex32 v[2];
            float quadratic = 0;
            for (size_t row = 0; row < 2; ++row) {
                /* Preserve native float32 product-sum grouping. */
                v[row] = (complex32){0, 0};
                for (size_t column = 0; column < 2; ++column) {
                    complex32 a = p[(row * 2 + column) * bins + bin];
                    complex32 b = x[column * bins + bin];
                    v[row].re += a.re * b.re - a.im * b.im;
                    v[row].im += b.im * a.re + b.re * a.im;
                }
                complex32 value = x[row * bins + bin];
                quadratic += v[row].re * value.re + v[row].im * value.im;
            }
            float factor = beta / (quadratic * beta + forget);
            float reciprocal = 1.0f / forget;
            for (size_t row = 0; row < 2; ++row) {
                complex32 *diagonal = p + row * 3 * bins + bin;
                diagonal->re = (diagonal->re - (v[row].re * v[row].re + v[row].im * v[row].im) * factor) * reciprocal;
                diagonal->im = 0;
            }
            complex32 *upper = p + bins + bin;
            upper->re = (upper->re - (v[0].re * v[1].re + v[0].im * v[1].im) * factor) * reciprocal;
            upper->im = (upper->im - (v[0].im * v[1].re - v[1].im * v[0].re) * factor) * reciprocal;
            p[2 * bins + bin] = (complex32){upper->re, -upper->im};
            if (p[bin].re + p[3 * bins + bin].re > 1000000.0f) {
                for (size_t cell = 0; cell < 4; ++cell) {
                    p[cell * bins + bin].re *= 0.01f;
                    p[cell * bins + bin].im *= 0.01f;
                }
            }
            complex32 proposal[2] = {{0, 0}, {0, 0}};
            float norm = 0;
            for (size_t row = 0; row < 2; ++row) {
                for (size_t column = 0; column < 2; ++column) {
                    complex32 a = p[(row * 2 + column) * bins + bin];
                    complex32 b = mix[(column * 2 + source) * bins + bin];
                    proposal[row].re += a.re * b.re - a.im * b.im;
                    proposal[row].im += b.im * a.re + a.im * b.re;
                }
                norm += proposal[row].re * proposal[row].re + proposal[row].im * proposal[row].im;
            }
            norm = sqrtf(norm);
            complex32 delta[2];
            for (size_t column = 0; column < 2; ++column) {
                complex32 next = {proposal[column].re / norm, -(proposal[column].im / norm)};
                complex32 old = demix[(source * 2 + column) * bins + bin];
                delta[column] = (complex32){next.re - old.re, -(next.im - old.im)};
                demix[(source * 2 + column) * bins + bin] = next;
            }
            complex32 u[2] = {{0, 0}, {0, 0}};
            for (size_t column = 0; column < 2; ++column)
                for (size_t row = 0; row < 2; ++row) {
                    complex32 a = mix[(row * 2 + column) * bins + bin];
                    u[column].re += delta[row].re * a.re + delta[row].im * a.im;
                    u[column].im += a.im * delta[row].re - delta[row].im * a.re;
                }
            float real = 1.0f + u[source].re;
            float denominator = real * real + u[source].im * u[source].im;
            complex32 reciprocal_update = {real / denominator, u[source].im / -denominator};
            for (size_t row = 0; row < 2; ++row) {
                complex32 a = mix[(row * 2 + source) * bins + bin];
                complex32 update = {reciprocal_update.re * a.re - reciprocal_update.im * a.im,
                                    a.im * reciprocal_update.re + a.re * reciprocal_update.im};
                for (size_t column = 0; column < 2; ++column) {
                    complex32 *cell = mix + (row * 2 + column) * bins + bin;
                    cell->im -= update.re * u[column].im + u[column].re * update.im;
                    cell->re -= update.re * u[column].re - update.im * u[column].im;
                }
            }
        }
    }
    for (size_t source = 0; source < 2; ++source)
        for (size_t bin = 0; bin < bins; ++bin) {
            complex32 gain = mix[source * bins + bin];
            complex32 other = mix[(2 + source) * bins + bin];
            if (gain.re * gain.re + gain.im * gain.im < other.re * other.re + other.im * other.im)
                gain = other;
            for (size_t channel = 0; channel < 2; ++channel) {
                complex32 weight = demix[(source * 2 + channel) * bins + bin];
                scaled[(source * 2 + channel) * bins + bin] = (complex32){
                    gain.re * weight.re - gain.im * weight.im,
                    gain.re * weight.im + gain.im * weight.re};
            }
        }
}
