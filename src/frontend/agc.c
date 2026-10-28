/* Independent default AGC wrapper; no official library is loaded or linked.
 * The adapted fixed-point digital kernel is BSD-licensed WebRTC code in
 * third_party/webrtc_agc; see its COPYING and PROVENANCE.json.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "webrtc/modules/audio_processing/agc/legacy/digital_agc.h"

#define EXPORT __attribute__((visibility("default")))

/* These assertions also document the state layout used by optional audits. */
_Static_assert(sizeof(DigitalAgc) == 256, "unexpected digital AGC layout");
_Static_assert(offsetof(DigitalAgc, vadNearend) == 144, "unexpected VAD offset");
_Static_assert(offsetof(DigitalAgc, vadFarend) == 200, "unexpected far VAD offset");

EXPORT size_t fsmn_kws_agc_state_size(void) { return sizeof(DigitalAgc); }

EXPORT int fsmn_kws_agc_init(void *state) {
    DigitalAgc *agc = state;
    memset(agc, 0, sizeof(*agc));
    WebRtcAgc_InitDigital(agc, 2); /* adaptive digital, initially unity gain */
    /* Native AGC_Power=8, AGC_Level=4, limiter=1, analogTarget=24. */
    return WebRtcAgc_CalculateGainTable(agc->gainTable, 8, 4, 1, 24);
}

EXPORT int fsmn_kws_agc_process(void *state, const int16_t *input,
                               int16_t *output, size_t frames) {
    for (size_t frame = 0; frame < frames; ++frame) {
        const int16_t *in[1] = {input + frame * 160};
        int16_t *out[1] = {output + frame * 160};
        int result = WebRtcAgc_ProcessDigital(state, in, 1, out, 16000, 0);
        if (result) return result;
    }
    return 0;
}
