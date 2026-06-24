/* SPDX-License-Identifier: MIT */
#include "nanotts/nanotts.h"
#include <assert.h>
#include <stddef.h>

static int contains_phone(const nanotts_t *tts, nanotts_phone_t phone)
{
    const nanotts_event_t *e = nanotts_events(tts);
    size_t i;
    for (i = 0u; i < nanotts_event_count(tts); ++i)
        if ((nanotts_phone_t)e[i].phone == phone) return 1;
    return 0;
}

static unsigned total_duration(const nanotts_t *tts)
{
    const nanotts_event_t *e = nanotts_events(tts);
    size_t i;
    unsigned sum = 0u;
    for (i = 0u; i < nanotts_event_count(tts); ++i)
        sum += e[i].duration_percent;
    return sum;
}

int main(void)
{
    nanotts_t tts;
    nanotts_text_options_t options;
    nanotts_parse_info_t info;
    unsigned clear_duration, conversational_duration;

    assert(nanotts_init(&tts, 16000u) == NANOTTS_OK);
    nanotts_default_text_options(&options);

    options.style = NANOTTS_KO_STYLE_PRESCRIPTIVE;
    assert(nanotts_parse_text_ex(&tts, "맛있다 나의 책", NANOTTS_NPOS,
                                &options, &info) == NANOTTS_OK);
#if NANOTTS_ENABLE_BUILTIN_LEXICON
    assert(contains_phone(&tts, NANOTTS_PH_T_LAX)); /* 마딛따 */
#else
    assert(contains_phone(&tts, NANOTTS_PH_S_LAX_PAL)); /* spelling fallback */
#endif
#if NANOTTS_ENABLE_MORPHOLOGY
    assert(contains_phone(&tts, NANOTTS_PH_UI));    /* 조사 의 */
#endif

    options.style = NANOTTS_KO_STYLE_MODERN_SEOUL;
    assert(nanotts_parse_text_ex(&tts, "맛있다 나의 책", NANOTTS_NPOS,
                                &options, &info) == NANOTTS_OK);
    assert(contains_phone(&tts, NANOTTS_PH_S_LAX_PAL)); /* 마싣따 */
    assert(!contains_phone(&tts, NANOTTS_PH_UI));
#if NANOTTS_ENABLE_MORPHOLOGY
    assert(contains_phone(&tts, NANOTTS_PH_E));         /* 나에 */
#endif
    assert(info.variant_count > 0u); /* noninitial ㅢ and/or lexical variant */

    options.style = NANOTTS_KO_STYLE_CLEAR_DEVICE;
    assert(nanotts_parse_text_ex(&tts, "배터리가 부족합니다", NANOTTS_NPOS,
                                &options, NULL) == NANOTTS_OK);
    clear_duration = total_duration(&tts);

    options.style = NANOTTS_KO_STYLE_CONVERSATIONAL;
    assert(nanotts_parse_text_ex(&tts, "배터리가 부족합니다", NANOTTS_NPOS,
                                &options, NULL) == NANOTTS_OK);
    conversational_duration = total_duration(&tts);
    assert(clear_duration > conversational_duration);

    return 0;
}
