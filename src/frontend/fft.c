/* Packed FFT wrapper; public BSD-licensed KissFFT is compiled independently.
 * No official library is loaded or linked. See third_party/kissfft/COPYING.
 */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "kiss_fftr.h"

#define EXPORT __attribute__((visibility("default")))

typedef struct {
    int size;
    kiss_fftr_cfg forward, inverse;
    kiss_fft_cpx *temporary;
} fsmn_kws_fft;

EXPORT void fsmn_kws_fft_free(fsmn_kws_fft *state) {
    if (!state) return;
    free(state->forward);
    free(state->inverse);
    free(state->temporary);
    free(state);
}

EXPORT fsmn_kws_fft *fsmn_kws_fft_create(int size) {
    if (size < 2 || size % 2) return NULL;
    fsmn_kws_fft *state = calloc(1, sizeof(*state));
    if (!state) return NULL;
    state->size = size;
    state->forward = kiss_fftr_alloc(size, 0, NULL, NULL);
    state->inverse = kiss_fftr_alloc(size, 1, NULL, NULL);
    state->temporary = malloc((size / 2 + 1) * sizeof(kiss_fft_cpx));
    if (!state->forward || !state->inverse || !state->temporary) {
        fsmn_kws_fft_free(state);
        return NULL;
    }
    return state;
}

EXPORT void fsmn_kws_fft_forward(fsmn_kws_fft *state, const float *input,
                                  float *packed, size_t rows) {
    for (size_t row = 0; row < rows; ++row) {
        kiss_fftr(state->forward, input, state->temporary);
        memcpy(packed, state->temporary, state->size * sizeof(float));
        packed[1] = state->temporary[state->size / 2].r;
        input += state->size;
        packed += state->size;
    }
}

EXPORT void fsmn_kws_fft_inverse(fsmn_kws_fft *state, const float *packed,
                                  float *output, size_t rows, int normalize) {
    float scale = 1.0f / (float)state->size;
    for (size_t row = 0; row < rows; ++row) {
        memcpy(state->temporary, packed, state->size * sizeof(float));
        state->temporary[state->size / 2].r = packed[1];
        state->temporary[state->size / 2].i = 0.0f;
        state->temporary[0].i = 0.0f;
        kiss_fftri(state->inverse, state->temporary, output);
        if (normalize)
            for (int sample = 0; sample < state->size; ++sample)
                output[sample] *= scale;
        packed += state->size;
        output += state->size;
    }
}
