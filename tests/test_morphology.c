/* SPDX-License-Identifier: MIT */
#include "nanotts/nanotts.h"
#include <assert.h>
#include <stddef.h>

static size_t count_phone(const nanotts_t *tts, nanotts_phone_t phone)
{
    const nanotts_event_t *e = nanotts_events(tts);
    size_t i, n = 0u;
    for (i = 0u; i < nanotts_event_count(tts); ++i)
        if ((nanotts_phone_t)e[i].phone == phone) ++n;
    return n;
}

static int has_morph_flag(const nanotts_t *tts, uint16_t flag)
{
    const nanotts_morpheme_t *m = nanotts_morphemes(tts);
    size_t i;
    for (i = 0u; i < nanotts_morpheme_count(tts); ++i)
        if ((m[i].flags & flag) != 0u) return 1;
    return 0;
}

int main(void)
{
#if !NANOTTS_ENABLE_MORPHOLOGY
    return 0;
#else
    nanotts_t tts;
    nanotts_text_options_t options;
    nanotts_parse_info_t info;

    assert(nanotts_init(&tts, 16000u) == NANOTTS_OK);
    nanotts_default_text_options(&options);
    options.style = NANOTTS_KO_STYLE_MODERN_SEOUL;

    /* Object-particle context selects verbal 신-고 and Rule-25-like
     * morphology-conditioned tensification. */
    assert(nanotts_parse_text_ex(&tts, "신을 신고", NANOTTS_NPOS,
                                &options, &info) == NANOTTS_OK);
    assert(count_phone(&tts, NANOTTS_PH_K_TENSE) == 1u);
    assert(count_phone(&tts, NANOTTS_PH_K_LAX) == 0u);
    assert(has_morph_flag(&tts, NANOTTS_MORPH_OBJECT_PARTICLE));
    assert(info.morpheme_count >= 4u);

    /* The same spelling in a nominal context remains lax. */
    assert(nanotts_parse_text_ex(&tts, "혼인 신고", NANOTTS_NPOS,
                                &options, &info) == NANOTTS_OK);
    assert(count_phone(&tts, NANOTTS_PH_K_LAX) == 1u);
    assert(count_phone(&tts, NANOTTS_PH_K_TENSE) == 0u);

    /* Homographic -하고 is an ending after a known verb stem, but a particle
     * after an ordinary noun.  Ending analysis must run first without stealing
     * the noun case. */
    assert(nanotts_parse_text_ex(&tts, "신고하고", NANOTTS_NPOS,
                                &options, &info) == NANOTTS_OK);
    assert(nanotts_morpheme_count(&tts) == 2u);
    assert(nanotts_morphemes(&tts)[0].pos == NANOTTS_KO_POS_VERB_STEM);
    assert(nanotts_morphemes(&tts)[1].pos == NANOTTS_KO_POS_ENDING);

    assert(nanotts_parse_text_ex(&tts, "친구하고", NANOTTS_NPOS,
                                &options, &info) == NANOTTS_OK);
    assert(nanotts_morpheme_count(&tts) == 2u);
    assert(nanotts_morphemes(&tts)[0].pos == NANOTTS_KO_POS_NOUN);
    assert(nanotts_morphemes(&tts)[1].pos == NANOTTS_KO_POS_PARTICLE);

    /* Passive/causative -기 suppresses the tensing that nominalizer -기 takes. */
    assert(nanotts_parse_text_ex(&tts, "안기다", NANOTTS_NPOS,
                                &options, &info) == NANOTTS_OK);
    assert(count_phone(&tts, NANOTTS_PH_K_LAX) == 1u);
    assert(count_phone(&tts, NANOTTS_PH_K_TENSE) == 0u);
    assert(has_morph_flag(&tts, NANOTTS_MORPH_PASSIVE_CAUSATIVE));

    assert(nanotts_parse_text_ex(&tts, "안기", NANOTTS_NPOS,
                                &options, &info) == NANOTTS_OK);
    assert(count_phone(&tts, NANOTTS_PH_K_TENSE) == 1u);
    assert(has_morph_flag(&tts, NANOTTS_MORPH_NOMINALIZER_GI));

    /* Attributive -(으)ㄹ conditions a tense following onset. */
    assert(nanotts_parse_text_ex(&tts, "할 것", NANOTTS_NPOS,
                                &options, &info) == NANOTTS_OK);
    assert(count_phone(&tts, NANOTTS_PH_K_TENSE) == 1u);
    assert(has_morph_flag(&tts, NANOTTS_MORPH_ATTRIBUTIVE_L));

    return 0;
#endif
}
