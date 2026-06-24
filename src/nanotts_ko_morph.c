/* SPDX-License-Identifier: MIT */
#include "nanotts_internal.h"

#if NANOTTS_ENABLE_MORPHOLOGY
typedef enum ko_analysis_result {
    KO_ANALYSIS_NO_MATCH = 0,
    KO_ANALYSIS_MATCH,
    KO_ANALYSIS_ERROR
} ko_analysis_result_t;

typedef struct ko_suffix {
    const char *text;
    uint8_t pos;
    uint16_t flags;
    uint8_t strong_verbal;
} ko_suffix_t;

static const ko_suffix_t PARTICLES[] = {
    {"으로",NANOTTS_KO_POS_PARTICLE,0,0},
    {"에서",NANOTTS_KO_POS_PARTICLE,0,0},
    {"에게",NANOTTS_KO_POS_PARTICLE,0,0},
    {"한테",NANOTTS_KO_POS_PARTICLE,0,0},
    {"부터",NANOTTS_KO_POS_PARTICLE,0,0},
    {"까지",NANOTTS_KO_POS_PARTICLE,0,0},
    {"처럼",NANOTTS_KO_POS_PARTICLE,0,0},
    {"보다",NANOTTS_KO_POS_PARTICLE,0,0},
    {"밖에",NANOTTS_KO_POS_PARTICLE,0,0},
    {"만큼",NANOTTS_KO_POS_PARTICLE,0,0},
    {"마다",NANOTTS_KO_POS_PARTICLE,0,0},
    {"조차",NANOTTS_KO_POS_PARTICLE,0,0},
    {"마저",NANOTTS_KO_POS_PARTICLE,0,0},
    {"라도",NANOTTS_KO_POS_PARTICLE,0,0},
    {"이나",NANOTTS_KO_POS_PARTICLE,0,0},
    {"거나",NANOTTS_KO_POS_PARTICLE,0,0},
    {"하고",NANOTTS_KO_POS_PARTICLE,0,0},
    {"의",NANOTTS_KO_POS_PARTICLE,NANOTTS_MORPH_UI_PARTICLE,0},
    {"은",NANOTTS_KO_POS_PARTICLE,NANOTTS_MORPH_TOPIC_PARTICLE,0},
    {"는",NANOTTS_KO_POS_PARTICLE,NANOTTS_MORPH_TOPIC_PARTICLE,0},
    {"을",NANOTTS_KO_POS_PARTICLE,NANOTTS_MORPH_OBJECT_PARTICLE,0},
    {"를",NANOTTS_KO_POS_PARTICLE,NANOTTS_MORPH_OBJECT_PARTICLE,0},
    {"이",NANOTTS_KO_POS_PARTICLE,0,0},{"가",NANOTTS_KO_POS_PARTICLE,0,0},
    {"에",NANOTTS_KO_POS_PARTICLE,0,0},{"와",NANOTTS_KO_POS_PARTICLE,0,0},
    {"과",NANOTTS_KO_POS_PARTICLE,0,0},{"도",NANOTTS_KO_POS_PARTICLE,0,0},
    {"만",NANOTTS_KO_POS_PARTICLE,0,0},{"로",NANOTTS_KO_POS_PARTICLE,0,0}
};

/* Longest and most diagnostic endings first. */
static const ko_suffix_t ENDINGS[] = {
    {"겠습니까",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,1},
    {"십시오",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,1},
    {"습니다",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,1},
    {"습니까",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,1},
    {"었어요",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,1},
    {"았어요",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,1},
    {"였어요",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,1},
    {"겠어요",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,1},
    {"는데요",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,1},
    {"으세요",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,1},
    {"세요",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,1},
    {"입니다",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,1},
    {"합니다",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,1},
    {"됩니다",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,1},
    {"이었다",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,1},
    {"하였다",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,1},
    {"겠다고",NANOTTS_KO_POS_ENDING,0,1},
    {"으려고",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_KEEP_WITH_NEXT,1},
    {"려고",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_KEEP_WITH_NEXT,1},
    {"으면서",NANOTTS_KO_POS_ENDING,0,1},{"면서",NANOTTS_KO_POS_ENDING,0,1},
    {"으니까",NANOTTS_KO_POS_ENDING,0,1},{"니까",NANOTTS_KO_POS_ENDING,0,1},
    {"으므로",NANOTTS_KO_POS_ENDING,0,1},{"므로",NANOTTS_KO_POS_ENDING,0,1},
    {"더라도",NANOTTS_KO_POS_ENDING,0,1},{"도록",NANOTTS_KO_POS_ENDING,0,1},
    {"는데",NANOTTS_KO_POS_ENDING,0,1},{"지만",NANOTTS_KO_POS_ENDING,0,1},
    {"으면",NANOTTS_KO_POS_ENDING,0,1},{"면",NANOTTS_KO_POS_ENDING,0,0},
    {"어서",NANOTTS_KO_POS_ENDING,0,1},{"아서",NANOTTS_KO_POS_ENDING,0,1},
    {"여서",NANOTTS_KO_POS_ENDING,0,1},{"고서",NANOTTS_KO_POS_ENDING,0,1},
    {"겠지",NANOTTS_KO_POS_ENDING,0,1},{"겠네",NANOTTS_KO_POS_ENDING,0,1},
    {"나요",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,1},
    {"네요",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,1},
    {"군요",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,1},
    {"어요",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,1},
    {"아요",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,1},
    {"여요",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,1},
    {"었다",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,1},
    {"았다",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,1},
    {"였다",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,1},
    {"겠다",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,1},
    {"는다",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,1},
    {"니다",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,1},
    {"는다",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,1},
    {"자",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,0},
    {"라",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,0},
    {"다",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,0},
    {"고",NANOTTS_KO_POS_ENDING,0,0},{"게",NANOTTS_KO_POS_ENDING,0,0},
    {"지",NANOTTS_KO_POS_ENDING,0,0},{"니",NANOTTS_KO_POS_ENDING,0,0},
    {"까",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,0},
    {"요",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,0},
    {"네",NANOTTS_KO_POS_ENDING,NANOTTS_MORPH_STRONG_BREAK,0}
};
#endif /* NANOTTS_ENABLE_MORPHOLOGY */

static bool add_morpheme(nanotts_impl_t *impl, size_t token_index,
                         size_t syllable_start, size_t syllable_count,
                         nanotts_ko_pos_t pos, uint16_t flags)
{
    nanotts_morpheme_t *m;
    nanotts_ko_token_t *token;
    if (!syllable_count) return true;
    if (impl->morpheme_count >= NANOTTS_MAX_MORPHEMES ||
        syllable_start > UINT16_MAX || syllable_count > UINT16_MAX)
        return false;
    m = &impl->morphemes[impl->morpheme_count];
    m->syllable_start = (uint16_t)syllable_start;
    m->syllable_count = (uint16_t)syllable_count;
    m->flags = flags;
    m->token_index = (uint8_t)token_index;
    m->pos = (uint8_t)pos;
    token = &impl->tokens[token_index];
    if (!token->morpheme_count) token->morpheme_start = (uint16_t)impl->morpheme_count;
    ++token->morpheme_count;
    ++impl->morpheme_count;
    return true;
}

#if NANOTTS_ENABLE_MORPHOLOGY
static bool previous_has_object_particle(const nanotts_impl_t *impl, size_t token_index)
{
    return token_index > 0u &&
        (impl->tokens[token_index - 1u].flags & KO_TOKEN_OBJECT_CONTEXT) != 0u;
}

static bool exact_any(const char *word, size_t length,
                      const char *const *items, size_t count)
{
    size_t i;
    for (i = 0u; i < count; ++i)
        if (ko_word_equal(word, length, items[i])) return true;
    return false;
}

static ko_analysis_result_t analyze_special(nanotts_impl_t *impl, const char *word, size_t length,
                            size_t token_index)
{
    nanotts_ko_token_t *t = &impl->tokens[token_index];
    size_t start = t->syllable_start, count = t->syllable_count;
    static const char *const passive[] =
        {"안기다","감기다","굶기다","옮기다","남기다"};
    static const char *const nominalized[] =
        {"안기","감기","굶기","옮기","남기"};

    if (ko_word_equal(word, length, "신고")) {
        if (previous_has_object_particle(impl, token_index) && count >= 2u) {
            if (!add_morpheme(impl, token_index, start, count - 1u,
                              NANOTTS_KO_POS_VERB_STEM, 0u) ||
                !add_morpheme(impl, token_index, start + count - 1u, 1u,
                              NANOTTS_KO_POS_ENDING, NANOTTS_MORPH_STRONG_BREAK))
                return KO_ANALYSIS_ERROR;
            t->flags |= KO_TOKEN_VERBAL | KO_TOKEN_STRONG_BREAK;
        } else {
            if (!add_morpheme(impl, token_index, start, count,
                              NANOTTS_KO_POS_NOUN, 0u)) return KO_ANALYSIS_ERROR;
            t->flags |= KO_TOKEN_NOUN;
        }
        return KO_ANALYSIS_MATCH;
    }

    if (exact_any(word, length, passive, sizeof(passive) / sizeof(passive[0])) &&
        count >= 3u) {
        if (!add_morpheme(impl, token_index, start, count - 2u,
                          NANOTTS_KO_POS_VERB_STEM, 0u) ||
            !add_morpheme(impl, token_index, start + count - 2u, 1u,
                          NANOTTS_KO_POS_SUFFIX, NANOTTS_MORPH_PASSIVE_CAUSATIVE) ||
            !add_morpheme(impl, token_index, start + count - 1u, 1u,
                          NANOTTS_KO_POS_ENDING, NANOTTS_MORPH_STRONG_BREAK))
            return KO_ANALYSIS_ERROR;
        t->flags |= KO_TOKEN_VERBAL | KO_TOKEN_STRONG_BREAK;
        return KO_ANALYSIS_MATCH;
    }

    if (exact_any(word, length, nominalized,
                  sizeof(nominalized) / sizeof(nominalized[0])) && count >= 2u) {
        if (!add_morpheme(impl, token_index, start, count - 1u,
                          NANOTTS_KO_POS_VERB_STEM, 0u) ||
            !add_morpheme(impl, token_index, start + count - 1u, 1u,
                          NANOTTS_KO_POS_SUFFIX, NANOTTS_MORPH_NOMINALIZER_GI))
            return KO_ANALYSIS_ERROR;
        t->flags |= KO_TOKEN_NOUN;
        return KO_ANALYSIS_MATCH;
    }
    return KO_ANALYSIS_NO_MATCH;
}


static bool likely_attributive_head(const nanotts_impl_t *impl,
                                     const char *text, size_t token_index)
{
    static const char *const heads[] = {
        "것","수","때","곳","사람","방법","이유","계획","예정",
        "필요","가능","문제","작업","기능","제품","기회","시간",
        "준비","내용","대상","경우","바","적","도리","법","듯",
        "리","말","일","점","부분","자료","버튼","장치","명령"
    };
    const nanotts_ko_token_t *next;
    const char *word;
    size_t length, i;
    if (token_index + 1u >= impl->token_count) return false;
    next = &impl->tokens[token_index + 1u];
    if (next->type != KO_TOKEN_HANGUL) return false;
    word = text + next->byte_start;
    length = next->byte_length;
    for (i = 0u; i < sizeof(heads) / sizeof(heads[0]); ++i) {
        size_t n = strlen(heads[i]);
        if (n <= length && memcmp(word, heads[i], n) == 0) return true;
    }
    return false;
}

static ko_analysis_result_t analyze_attributive_l(nanotts_impl_t *impl,
                                                   const char *text,
                                                   size_t token_index)
{
    nanotts_ko_token_t *t = &impl->tokens[token_index];
    const nanotts_ko_syllable_t *seq;
    const nanotts_ko_syllable_t *last;
    size_t start = t->syllable_start, count = t->syllable_count;
    bool matched = false;
    bool separate_eul = false;
    if (!count || !likely_attributive_head(impl, text, token_index))
        return KO_ANALYSIS_NO_MATCH;
    seq = &impl->syllables[start];
    last = &seq[count - 1u];

    /* -을: the ending occupies its own syllable. */
    if (count > 1u && last->lexical_onset == 11u && last->vowel == 18u &&
        last->lexical_coda == 8u &&
        ko_is_known_verb_syllables(seq, count - 1u, false)) {
        matched = true;
        separate_eul = true;
    /* -ㄹ: the ending is the final coda of the stem-final syllable.  Also
     * accept ㄹ-final stems in an attributive context; morphology and the
     * following lexical token disambiguate them from most noun uses. */
    } else if (last->lexical_coda == 8u &&
               (ko_is_known_verb_syllables(seq, count, true) ||
                ko_is_known_verb_syllables(seq, count, false))) {
        matched = true;
    }
    if (!matched) return KO_ANALYSIS_NO_MATCH;

    if (separate_eul) {
        if (!add_morpheme(impl, token_index, start, count - 1u,
                          NANOTTS_KO_POS_VERB_STEM, 0u) ||
            !add_morpheme(impl, token_index, start + count - 1u, 1u,
                          NANOTTS_KO_POS_ENDING,
                          NANOTTS_MORPH_ATTRIBUTIVE_L |
                          NANOTTS_MORPH_KEEP_WITH_NEXT))
            return KO_ANALYSIS_ERROR;
    } else {
        if (!add_morpheme(impl, token_index, start, count,
                          NANOTTS_KO_POS_VERB_STEM,
                          NANOTTS_MORPH_ATTRIBUTIVE_L |
                          NANOTTS_MORPH_KEEP_WITH_NEXT))
            return KO_ANALYSIS_ERROR;
    }
    t->flags |= KO_TOKEN_VERBAL | KO_TOKEN_KEEP_WITH_NEXT;
    return KO_ANALYSIS_MATCH;
}

static ko_analysis_result_t analyze_particle(nanotts_impl_t *impl, const char *word, size_t length,
                             size_t token_index)
{
    size_t i;
    nanotts_ko_token_t *t = &impl->tokens[token_index];
    for (i = 0u; i < sizeof(PARTICLES) / sizeof(PARTICLES[0]); ++i) {
        size_t prefix_bytes, suffix_syllables, prefix_syllables;
        if (!ko_word_ends_with(word, length, PARTICLES[i].text, &prefix_bytes) ||
            !prefix_bytes) continue;
        suffix_syllables = ko_hangul_syllable_count(PARTICLES[i].text,
                                                    strlen(PARTICLES[i].text));
        if (!suffix_syllables || t->syllable_count <= suffix_syllables) continue;
        prefix_syllables = t->syllable_count - suffix_syllables;
        if (!add_morpheme(impl, token_index, t->syllable_start, prefix_syllables,
                          NANOTTS_KO_POS_NOUN, 0u) ||
            !add_morpheme(impl, token_index,
                          t->syllable_start + prefix_syllables, suffix_syllables,
                          NANOTTS_KO_POS_PARTICLE, PARTICLES[i].flags))
            return KO_ANALYSIS_ERROR;
        t->flags |= KO_TOKEN_NOUN;
        if (ko_is_question_word(word, prefix_bytes))
            t->flags |= KO_TOKEN_QUESTION_WORD;
        if (PARTICLES[i].flags & NANOTTS_MORPH_OBJECT_PARTICLE)
            t->flags |= KO_TOKEN_OBJECT_CONTEXT;
        return KO_ANALYSIS_MATCH;
    }
    return KO_ANALYSIS_NO_MATCH;
}

static ko_analysis_result_t analyze_ending(nanotts_impl_t *impl, const char *word, size_t length,
                           size_t token_index)
{
    size_t i;
    nanotts_ko_token_t *t = &impl->tokens[token_index];
    for (i = 0u; i < sizeof(ENDINGS) / sizeof(ENDINGS[0]); ++i) {
        size_t prefix_bytes, suffix_syllables, prefix_syllables;
        bool known;
        if (!ko_word_ends_with(word, length, ENDINGS[i].text, &prefix_bytes) ||
            !prefix_bytes) continue;
        known = ENDINGS[i].strong_verbal != 0u ||
                ko_is_known_verb_stem(word, prefix_bytes);
        if (!known) continue;
        suffix_syllables = ko_hangul_syllable_count(ENDINGS[i].text,
                                                    strlen(ENDINGS[i].text));
        if (!suffix_syllables || t->syllable_count <= suffix_syllables) continue;
        prefix_syllables = t->syllable_count - suffix_syllables;
        if (!add_morpheme(impl, token_index, t->syllable_start, prefix_syllables,
                          NANOTTS_KO_POS_VERB_STEM, 0u) ||
            !add_morpheme(impl, token_index,
                          t->syllable_start + prefix_syllables, suffix_syllables,
                          NANOTTS_KO_POS_ENDING, ENDINGS[i].flags))
            return KO_ANALYSIS_ERROR;
        t->flags |= KO_TOKEN_VERBAL;
        if (ENDINGS[i].flags & NANOTTS_MORPH_STRONG_BREAK)
            t->flags |= KO_TOKEN_STRONG_BREAK;
        if (ENDINGS[i].flags & NANOTTS_MORPH_KEEP_WITH_NEXT)
            t->flags |= KO_TOKEN_KEEP_WITH_NEXT;
        return KO_ANALYSIS_MATCH;
    }
    return KO_ANALYSIS_NO_MATCH;
}
#endif /* NANOTTS_ENABLE_MORPHOLOGY */

static bool likely_attributive_l(const nanotts_impl_t *impl, size_t token_index)
{
    const nanotts_ko_token_t *t = &impl->tokens[token_index];
    const nanotts_ko_syllable_t *last;
    if (!t->syllable_count || token_index + 1u >= impl->token_count) return false;
    last = &impl->syllables[t->syllable_start + t->syllable_count - 1u];
    if (last->lexical_coda != 8u) return false;
    if ((t->flags & KO_TOKEN_VERBAL) != 0u) return true;
    /* A conservative list of frequent one-syllable attributive forms. */
    if (t->syllable_count == 1u) {
        uint8_t l = last->lexical_onset, v = last->vowel;
        return (l == 18u && v == 0u) || /* 할 */
               (l == 0u && v == 0u) ||  /* 갈 */
               (l == 11u && v == 8u) || /* 올 */
               (l == 7u && v == 8u) ||  /* 볼 */
               (l == 3u && v == 11u);   /* 될 */
    }
    return false;
}

static void annotate_morphemes(nanotts_impl_t *impl)
{
    size_t i, j;
    for (i = 0u; i < impl->morpheme_count; ++i) {
        const nanotts_morpheme_t *m = &impl->morphemes[i];
        size_t start = m->syllable_start;
        size_t end = start + m->syllable_count;
        for (j = start; j < end; ++j) {
            nanotts_ko_syllable_t *s = &impl->syllables[j];
            s->morpheme_index = (uint8_t)i;
            s->pos = m->pos;
            s->morph_flags |= m->flags;
            if (j == start) s->flags |= KO_SYLLABLE_MORPH_INITIAL;
            if (j + 1u == end) s->flags |= KO_SYLLABLE_MORPH_FINAL;
        }
    }
}

static bool ending_is_question(const char *word, size_t length)
{
    static const char *const q[] = {"습니까","나요","니","까","나요","죠"};
    size_t i, ignored;
    for (i = 0u; i < sizeof(q) / sizeof(q[0]); ++i)
        if (ko_word_ends_with(word, length, q[i], &ignored)) return true;
    return false;
}

static bool ending_is_command(const char *word, size_t length)
{
    static const char *const c[] = {"십시오","세요","해라","하라","라","자"};
    size_t i, ignored;
    for (i = 0u; i < sizeof(c) / sizeof(c[0]); ++i)
        if (ko_word_ends_with(word, length, c[i], &ignored)) return true;
    return false;
}

nanotts_result_t ko_analyze_morphology(nanotts_impl_t *impl, const char *text,
    size_t length, const nanotts_text_options_t *options,
    nanotts_parse_info_t *info)
{
    size_t i;
    (void)length;
    impl->morpheme_count = 0u;
    for (i = 0u; i < impl->token_count; ++i) {
        nanotts_ko_token_t *t = &impl->tokens[i];
#if NANOTTS_ENABLE_MORPHOLOGY
        const char *word = text + t->byte_start;
        size_t word_length = t->byte_length;
#else
        (void)text;
#endif
        t->morpheme_count = 0u;

        if (t->type == KO_TOKEN_NUMBER) {
            if (!add_morpheme(impl, i, t->syllable_start, t->syllable_count,
                              NANOTTS_KO_POS_NUMBER, 0u))
                return NANOTTS_ERR_TOO_MANY_MORPHEMES;
            continue;
        }
        if (t->type == KO_TOKEN_LATIN) {
            if (!add_morpheme(impl, i, t->syllable_start, t->syllable_count,
                              NANOTTS_KO_POS_FOREIGN, 0u))
                return NANOTTS_ERR_TOO_MANY_MORPHEMES;
            continue;
        }
        if (t->type == KO_TOKEN_SYMBOL) {
            if (!add_morpheme(impl, i, t->syllable_start, t->syllable_count,
                              NANOTTS_KO_POS_UNKNOWN, 0u))
                return NANOTTS_ERR_TOO_MANY_MORPHEMES;
            continue;
        }
        if (t->flags & KO_TOKEN_COUNTER) {
            if (!add_morpheme(impl, i, t->syllable_start, t->syllable_count,
                              NANOTTS_KO_POS_COUNTER,
                              NANOTTS_MORPH_COUNTER_NATIVE))
                return NANOTTS_ERR_TOO_MANY_MORPHEMES;
            continue;
        }

#if NANOTTS_ENABLE_MORPHOLOGY
        if (!(options->flags & NANOTTS_OPT_NO_MORPHOLOGY)) {
            ko_analysis_result_t a = analyze_special(impl, word, word_length, i);
            if (a == KO_ANALYSIS_ERROR) return NANOTTS_ERR_TOO_MANY_MORPHEMES;
            if (a == KO_ANALYSIS_MATCH) continue;
            a = analyze_attributive_l(impl, text, i);
            if (a == KO_ANALYSIS_ERROR) return NANOTTS_ERR_TOO_MANY_MORPHEMES;
            if (a == KO_ANALYSIS_MATCH) continue;
            /* Verbal endings are attempted before homographic particles such
             * as -하고.  The ending analyzer requires a known verbal stem, so
             * noun phrases such as 친구하고 still fall through to particles. */
            a = analyze_ending(impl, word, word_length, i);
            if (a == KO_ANALYSIS_ERROR) return NANOTTS_ERR_TOO_MANY_MORPHEMES;
            if (a == KO_ANALYSIS_MATCH) continue;
            a = analyze_particle(impl, word, word_length, i);
            if (a == KO_ANALYSIS_ERROR) return NANOTTS_ERR_TOO_MANY_MORPHEMES;
            if (a == KO_ANALYSIS_MATCH) continue;
        }
#else
        (void)options;
#endif
        if (!add_morpheme(impl, i, t->syllable_start, t->syllable_count,
                          NANOTTS_KO_POS_NOUN, 0u))
            return NANOTTS_ERR_TOO_MANY_MORPHEMES;
        t->flags |= KO_TOKEN_NOUN;
    }

    /* Add morphology-conditioned prosodic and phonological flags.  These
     * heuristics are bypassed completely when the application asks for the
     * spelling-only frontend. */
    for (i = 0u; i < impl->token_count; ++i) {
        nanotts_ko_token_t *t = &impl->tokens[i];
        bool morphology_enabled =
#if NANOTTS_ENABLE_MORPHOLOGY
            (options->flags & NANOTTS_OPT_NO_MORPHOLOGY) == 0u;
#else
            false;
#endif
        if (morphology_enabled && likely_attributive_l(impl, i) &&
            t->morpheme_count) {
            nanotts_morpheme_t *m = &impl->morphemes[
                t->morpheme_start + t->morpheme_count - 1u];
            m->flags |= NANOTTS_MORPH_ATTRIBUTIVE_L |
                        NANOTTS_MORPH_KEEP_WITH_NEXT;
            t->flags |= KO_TOKEN_KEEP_WITH_NEXT | KO_TOKEN_VERBAL;
        }
        if (morphology_enabled && t->type == KO_TOKEN_HANGUL &&
            (t->flags & KO_TOKEN_VERBAL) != 0u) {
            const char *word = text + t->byte_start;
            size_t n = t->byte_length;
            if (ending_is_question(word, n)) {
                impl->utterance_flags |= KO_UTTERANCE_QUESTION;
                t->flags |= KO_TOKEN_QUESTION_END;
            }
            if (ending_is_command(word, n)) {
                impl->utterance_flags |= KO_UTTERANCE_COMMAND;
                t->flags |= KO_TOKEN_COMMAND_END;
            }
        }
    }

    annotate_morphemes(impl);
    for (i = 0u; i < impl->token_count; ++i) {
        if ((impl->tokens[i].flags & KO_TOKEN_KEEP_WITH_NEXT) &&
            impl->tokens[i].syllable_count) {
            size_t last = impl->tokens[i].syllable_start +
                          impl->tokens[i].syllable_count - 1u;
            impl->syllables[last].flags |= KO_SYLLABLE_KEEP_NEXT;
        }
        if ((impl->tokens[i].flags & KO_TOKEN_STRONG_BREAK) &&
            impl->tokens[i].syllable_count) {
            size_t last = impl->tokens[i].syllable_start +
                          impl->tokens[i].syllable_count - 1u;
            impl->syllables[last].flags |= KO_SYLLABLE_AP_BREAK;
        }
        if (impl->tokens[i].flags & KO_TOKEN_QUESTION_WORD)
            impl->utterance_flags |= KO_UTTERANCE_WH_QUESTION;
    }
    if (info) info->morpheme_count = impl->morpheme_count;
    return NANOTTS_OK;
}

void ko_apply_style_variants(nanotts_impl_t *impl, nanotts_ko_style_t style,
                             nanotts_parse_info_t *info)
{
    size_t i;
    for (i = 0u; i < impl->syllable_count; ++i) {
        nanotts_ko_syllable_t *s = &impl->syllables[i];
        uint8_t before = s->vowel;
        bool modern = style == NANOTTS_KO_STYLE_MODERN_SEOUL ||
                      style == NANOTTS_KO_STYLE_CONVERSATIONAL;
        if ((s->morph_flags & NANOTTS_MORPH_UI_PARTICLE) && modern) {
            s->vowel = 5u; /* 조사 의 -> 에 */
        } else if (s->vowel == 19u &&
                   !(s->flags & KO_SYLLABLE_WORD_INITIAL) && modern) {
            s->vowel = 20u; /* noninitial 의 -> 이 */
        } else if (s->vowel == 3u && modern) {
            s->vowel = 7u;  /* ㅒ -> ㅖ */
        } else if (s->vowel == 7u && modern &&
                   s->lexical_onset != 11u && s->lexical_onset != 5u) {
            s->vowel = 5u;  /* 계/혜... common 게/헤-like realization */
        } else if (s->vowel == 1u &&
                   style == NANOTTS_KO_STYLE_CONVERSATIONAL) {
            s->vowel = 5u;  /* optional ㅐ/ㅔ merger */
        }
        if (s->vowel != before && info) ++info->variant_count;
    }
}
