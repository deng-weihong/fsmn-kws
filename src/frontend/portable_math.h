// SPDX-License-Identifier: Apache-2.0
#ifndef FSMN_KWS_PORTABLE_MATH_H
#define FSMN_KWS_PORTABLE_MATH_H
#include "libm.h"
// Keep adaptive powers consistent across host and embedded C libraries.
float fsmn_kws_powf(float value, float exponent);
#define powf fsmn_kws_powf
#endif
