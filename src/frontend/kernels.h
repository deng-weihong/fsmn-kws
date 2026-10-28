#ifndef FSMN_KWS_KERNELS_H
#define FSMN_KWS_KERNELS_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void fsmn_kws_hpf(float *, size_t, const float *, const float *, const float *, float *, size_t);
void fsmn_kws_ppf_synthesis(float *, float *, size_t, size_t, size_t, const float *, float, const float *, float *);
void fsmn_kws_auxiva_init(float *, size_t, float);
void fsmn_kws_auxiva_step(float *, size_t, float, const float *, float *);
size_t fsmn_kws_draec_state_size(void);
void fsmn_kws_draec_init(float *, uint32_t *);
void fsmn_kws_draec_step(float *, uint32_t *, const float *, const float *, float *, float *);
void *fsmn_kws_fft_create(int);
void fsmn_kws_fft_free(void *);
void fsmn_kws_fft_forward(void *, const float *, float *, size_t);
void fsmn_kws_fft_inverse(void *, const float *, float *, size_t, int);
size_t fsmn_kws_agc_state_size(void);
int fsmn_kws_agc_init(void *);
int fsmn_kws_agc_process(void *, const int16_t *, int16_t *, size_t);
#ifdef __cplusplus
}
#endif
#endif
