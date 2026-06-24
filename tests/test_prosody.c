/* SPDX-License-Identifier: MIT */
#include "nanotts/nanotts.h"
#include <assert.h>
#include <stddef.h>

static const nanotts_event_t *last_vowel(const nanotts_t *tts)
{
    const nanotts_event_t *e = nanotts_events(tts);
    size_t i = nanotts_event_count(tts);
    while (i > 0u) {
        nanotts_phone_t p;
        --i;
        p = (nanotts_phone_t)e[i].phone;
        if (p == NANOTTS_PH_A || p == NANOTTS_PH_EO || p == NANOTTS_PH_O ||
            p == NANOTTS_PH_U || p == NANOTTS_PH_EU || p == NANOTTS_PH_I ||
            p == NANOTTS_PH_AE || p == NANOTTS_PH_E || p == NANOTTS_PH_UI)
            return &e[i];
    }
    return NULL;
}

int main(void)
{
    nanotts_t tts;
    nanotts_text_options_t options;
    nanotts_parse_info_t info;
    const nanotts_event_t *e, *last;
    size_t i, q_count;
    int8_t yes_no_end, wh_end, statement_end;
    int has_continuous = 0, has_wh = 0;

    assert(nanotts_init(&tts, 16000u) == NANOTTS_OK);
    nanotts_default_text_options(&options);

    assert(nanotts_parse_text_ex(&tts, "가요?", NANOTTS_NPOS,
                                &options, &info) == NANOTTS_OK);
    last = last_vowel(&tts); assert(last != NULL);
    yes_no_end = last->pitch_end_semitones_q4;

    assert(nanotts_parse_text_ex(&tts, "어디 가요?", NANOTTS_NPOS,
                                &options, &info) == NANOTTS_OK);
    last = last_vowel(&tts); assert(last != NULL);
    wh_end = last->pitch_end_semitones_q4;
    e = nanotts_events(&tts);
    for (i = 0u; i < nanotts_event_count(&tts); ++i) {
        if (e[i].pitch_semitones_q4 != e[i].pitch_end_semitones_q4)
            has_continuous = 1;
        if ((e[i].acoustic_flags & NANOTTS_ACOUSTIC_WH_QUESTION) != 0u)
            has_wh = 1;
    }
    assert(has_continuous && has_wh);

    assert(nanotts_parse_text_ex(&tts, "가요.", NANOTTS_NPOS,
                                &options, &info) == NANOTTS_OK);
    last = last_vowel(&tts); assert(last != NULL);
    statement_end = last->pitch_end_semitones_q4;
    assert(yes_no_end > statement_end);
    assert(wh_end < yes_no_end);

    assert(nanotts_parse_text_ex(&tts, "괜찮아요? 네, 좋아요.", NANOTTS_NPOS,
                                &options, &info) == NANOTTS_OK);
    e = nanotts_events(&tts); q_count = 0u;
    for (i = 0u; i < nanotts_event_count(&tts); ++i)
        if ((e[i].flags & NANOTTS_EVENT_QUESTION) != 0u) ++q_count;
    assert(q_count == 1u);

    return 0;
}
