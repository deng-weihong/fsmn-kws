/* Minimal standalone glue for the unmodified MIT-licensed musl powf sources.
 * Only positive finite powers are used by the reconstructed DSP. Symbol
 * prefixes prevent collisions with either the host or target C library.
 */
#ifndef FSMN_KWS_MUSL_LIBM_GLUE_H
#define FSMN_KWS_MUSL_LIBM_GLUE_H
#include <math.h>
#include <stdint.h>
#include <string.h>
#define hidden __attribute__((visibility("hidden")))
#define TOINT_INTRINSICS 0
#define __powf_log2_data fsmn_kws_powf_log2_data
#define __exp2f_data fsmn_kws_exp2f_data
#define predict_false(x) __builtin_expect(!!(x), 0)
static inline uint32_t asuint(float x) { uint32_t v; memcpy(&v, &x, 4); return v; }
static inline float asfloat(uint32_t x) { float v; memcpy(&v, &x, 4); return v; }
static inline uint64_t asuint64(double x) { uint64_t v; memcpy(&v, &x, 8); return v; }
static inline double asdouble(uint64_t x) { double v; memcpy(&v, &x, 8); return v; }
static inline double eval_as_double(double x) { return x; }
static inline float eval_as_float(double x) { return (float)x; }
static inline float fp_barrierf(float x) { volatile float y = x; return y; }
static inline int issignalingf_inline(float x) {
    uint32_t v = asuint(x);
    return (v & 0x7fc00000) == 0x7f800000 && (v & 0x003fffff) != 0;
}
static inline float __math_invalidf(float x) { return (x - x) / (x - x); }
static inline float __math_oflowf(uint32_t sign) { return sign ? -INFINITY : INFINITY; }
static inline float __math_uflowf(uint32_t sign) { return sign ? -0.0f : 0.0f; }
#endif
