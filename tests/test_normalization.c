/* SPDX-License-Identifier: MIT */
#include "nanotts/nanotts.h"
#include <assert.h>
#include <stddef.h>

static nanotts_phone_t first_spoken_phone(const nanotts_t *tts)
{
    const nanotts_event_t *e = nanotts_events(tts);
    size_t i;
    for (i = 0u; i < nanotts_event_count(tts); ++i) {
        nanotts_phone_t p = (nanotts_phone_t)e[i].phone;
        if (p != NANOTTS_PH_SILENCE && p != NANOTTS_PH_SHORT_PAUSE &&
            p != NANOTTS_PH_AP_PAUSE && p != NANOTTS_PH_PHRASE_PAUSE)
            return p;
    }
    return NANOTTS_PH_SILENCE;
}

static void parses(nanotts_t *tts, const char *text,
                   const nanotts_text_options_t *options)
{
    nanotts_parse_info_t info;
    assert(nanotts_parse_text_ex(tts, text, NANOTTS_NPOS,
                                options, &info) == NANOTTS_OK);
    assert(info.normalization_count > 0u);
    assert(info.event_count > 0u);
}

int main(void)
{
#if !NANOTTS_ENABLE_NORMALIZATION
    return 0;
#else
    nanotts_t tts;
    nanotts_text_options_t options;
    nanotts_parse_info_t info;
    const nanotts_event_t *e;
    size_t i, question_count;

    assert(nanotts_init(&tts, 16000u) == NANOTTS_OK);
    nanotts_default_text_options(&options);

    /* Counter-sensitive readings: 열두 시 versus 십이 분. */
    parses(&tts, "12시", &options);
    assert(first_spoken_phone(&tts) == NANOTTS_PH_Y);
    parses(&tts, "12분", &options);
    assert(first_spoken_phone(&tts) == NANOTTS_PH_S_LAX_PAL);

    parses(&tts, "2026-06-24", &options);
    parses(&tts, "07:30", &options);
    parses(&tts, "3.14", &options);
    parses(&tts, "1/2", &options);
    parses(&tts, "85%", &options);
    parses(&tts, "-12.5", &options);
    parses(&tts, "010-1234-5678", &options);
    parses(&tts, "1.2.3", &options);
    parses(&tts, "192.168.1.10", &options);
    parses(&tts, "24 V", &options);
    parses(&tts, "2.4 GHz", &options);

    /* Grouping commas are numeric, but the final period remains a phrase
     * boundary rather than becoming part of the number token. */
    assert(nanotts_parse_text_ex(&tts, "1,000.", NANOTTS_NPOS,
                                &options, &info) == NANOTTS_OK);
    assert(info.token_count == 1u);
    assert(first_spoken_phone(&tts) == NANOTTS_PH_C_ASP); /* 천 */
    e = nanotts_events(&tts);
    assert(e[nanotts_event_count(&tts) - 1u].phone == NANOTTS_PH_PHRASE_PAUSE);
    assert((e[nanotts_event_count(&tts) - 1u].flags & NANOTTS_EVENT_QUESTION) == 0u);

    /* A question followed by a statement does not leak question modality into
     * the final statement. */
    assert(nanotts_parse_text_ex(&tts, "12시입니까? 12분입니다.", NANOTTS_NPOS,
                                &options, &info) == NANOTTS_OK);
    e = nanotts_events(&tts);
    question_count = 0u;
    for (i = 0u; i < nanotts_event_count(&tts); ++i)
        if ((e[i].flags & NANOTTS_EVENT_QUESTION) != 0u) ++question_count;
    assert(question_count == 1u);

    /* Explicitly disabling semantic normalization spells the digits. */
    options.flags |= NANOTTS_OPT_NO_NORMALIZATION;
    assert(nanotts_parse_text_ex(&tts, "12시", NANOTTS_NPOS,
                                &options, &info) == NANOTTS_OK);
    assert(first_spoken_phone(&tts) == NANOTTS_PH_I); /* 일 */

    return 0;
#endif
}
