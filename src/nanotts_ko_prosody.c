/* SPDX-License-Identifier: MIT */
#include "nanotts_internal.h"

/* Compact, style-selectable Korean prosody and allophone planner. */
typedef struct ko_style_profile {
    uint8_t vowel_open_q8;
    uint8_t vowel_closed_q8;
    uint8_t consonant_q8;
    uint8_t word_pause_q8;
    uint8_t comma_pause_q8;
    uint8_t phrase_pause_q8;
    uint8_t phrase_final_q8;
    uint8_t prominence_q8;
    int8_t pitch_range_q4;
    uint8_t max_ap_syllables;
    uint8_t max_ap_tokens;
} ko_style_profile_t;

static const ko_style_profile_t STYLE_PROFILES[NANOTTS_KO_STYLE_COUNT] = {
    /* open closed cons word comma phrase final prominence range syl tok */
    {100u, 92u,100u, 50u, 70u,100u,116u,132u,16,8u,3u}, /* prescriptive */
    { 98u, 91u, 98u, 44u, 66u, 96u,113u,130u,15,9u,4u}, /* modern Seoul */
    {116u,108u,108u, 68u, 82u,116u,124u,150u,19,7u,3u}, /* clear device */
    { 89u, 84u, 91u, 30u, 55u, 82u,108u,124u,13,10u,4u},/* conversational */
    {124u,114u,114u, 72u, 88u,122u,128u,158u,20,6u,3u} /* learner */
};

static const ko_style_profile_t *style_profile(const nanotts_impl_t *impl)
{
    unsigned style = impl->current_style;
    if (style >= (unsigned)NANOTTS_KO_STYLE_COUNT)
        style = (unsigned)NANOTTS_KO_STYLE_PRESCRIPTIVE;
    return &STYLE_PROFILES[style];
}

static nanotts_phone_t onset_phone(uint8_t onset, uint8_t vowel,
                                   uint8_t *acoustic_flags)
{
    bool palatal = vowel == 20u || vowel == 2u || vowel == 3u || vowel == 6u ||
                   vowel == 7u || vowel == 12u || vowel == 17u;
    switch (onset) {
    case 0: return NANOTTS_PH_K_LAX;
    case 1: return NANOTTS_PH_K_TENSE;
    case 2: return NANOTTS_PH_N;
    case 3: return NANOTTS_PH_T_LAX;
    case 4: return NANOTTS_PH_T_TENSE;
    case 5: return NANOTTS_PH_R;
    case 6: return NANOTTS_PH_M;
    case 7: return NANOTTS_PH_P_LAX;
    case 8: return NANOTTS_PH_P_TENSE;
    case 9:
        if (palatal) {
            if (acoustic_flags) *acoustic_flags |= NANOTTS_ACOUSTIC_PALATALIZED;
            return NANOTTS_PH_S_LAX_PAL;
        }
        return NANOTTS_PH_S_LAX;
    case 10:
        if (palatal) {
            if (acoustic_flags) *acoustic_flags |= NANOTTS_ACOUSTIC_PALATALIZED;
            return NANOTTS_PH_S_TENSE_PAL;
        }
        return NANOTTS_PH_S_TENSE;
    case 12: return NANOTTS_PH_C_LAX;
    case 13: return NANOTTS_PH_C_TENSE;
    case 14: return NANOTTS_PH_C_ASP;
    case 15: return NANOTTS_PH_K_ASP;
    case 16: return NANOTTS_PH_T_ASP;
    case 17: return NANOTTS_PH_P_ASP;
    case 18: return NANOTTS_PH_H;
    case KO_L_LATERAL: return NANOTTS_PH_L;
    default: return NANOTTS_PH_SILENCE;
    }
}

static nanotts_phone_t coda_phone(uint8_t coda)
{
    switch (coda) {
    case KO_CODA_K: return NANOTTS_PH_K_CODA;
    case KO_CODA_N: return NANOTTS_PH_N;
    case KO_CODA_T: return NANOTTS_PH_T_CODA;
    case KO_CODA_L: return NANOTTS_PH_L;
    case KO_CODA_M: return NANOTTS_PH_M;
    case KO_CODA_P: return NANOTTS_PH_P_CODA;
    case KO_CODA_NG: return NANOTTS_PH_NG;
    default: return NANOTTS_PH_SILENCE;
    }
}

static bool nasal_phone(nanotts_phone_t phone)
{
    return phone == NANOTTS_PH_M || phone == NANOTTS_PH_N ||
           phone == NANOTTS_PH_NG;
}

static bool token_is_functional(const nanotts_impl_t *impl, size_t token_index)
{
    const nanotts_ko_token_t *t;
    const nanotts_morpheme_t *m;
    if (token_index >= impl->token_count) return false;
    t = &impl->tokens[token_index];
    if (!t->morpheme_count) return false;
    m = &impl->morphemes[t->morpheme_start];
    return m->pos == NANOTTS_KO_POS_PARTICLE ||
           m->pos == NANOTTS_KO_POS_ENDING ||
           m->pos == NANOTTS_KO_POS_COUNTER;
}

static bool should_break_after_token(const nanotts_impl_t *impl,
                                     size_t token_index,
                                     size_t ap_start_syllable,
                                     size_t ap_token_count,
                                     const ko_style_profile_t *profile)
{
    const nanotts_ko_token_t *token = &impl->tokens[token_index];
    size_t end = token->syllable_start + token->syllable_count;
    const nanotts_ko_syllable_t *next;

    if (end >= impl->syllable_count) return true;
    next = &impl->syllables[end];
    if (ko_boundary_strength(next->boundary_before) >= NANOTTS_BOUNDARY_COMMA)
        return true;
    if ((token->flags & KO_TOKEN_KEEP_WITH_NEXT) != 0u) return false;
    if ((token->flags & KO_TOKEN_STRONG_BREAK) != 0u) return true;
    if (token_is_functional(impl, token_index + 1u)) return false;
    if (end - ap_start_syllable >= profile->max_ap_syllables) return true;
    if (ap_token_count >= profile->max_ap_tokens) return true;

    /* Topic and object particles are natural, but not mandatory, AP edges.  Use
     * them only once the phrase already contains enough material. */
    if (token->morpheme_count) {
        const nanotts_morpheme_t *last = &impl->morphemes[
            token->morpheme_start + token->morpheme_count - 1u];
        if ((last->flags & (NANOTTS_MORPH_TOPIC_PARTICLE |
                            NANOTTS_MORPH_OBJECT_PARTICLE)) != 0u &&
            end - ap_start_syllable >= 4u)
            return true;
    }
    return false;
}

static size_t choose_ap_end(const nanotts_impl_t *impl, size_t start)
{
    const ko_style_profile_t *profile = style_profile(impl);
    size_t token_index = impl->syllables[start].token_index;
    size_t token_count = 0u;
    size_t end = start;

    while (token_index < impl->token_count) {
        const nanotts_ko_token_t *token = &impl->tokens[token_index];
        size_t token_end = token->syllable_start + token->syllable_count;
        if (token_end <= start) {
            ++token_index;
            continue;
        }
        end = token_end;
        ++token_count;
        if (should_break_after_token(impl, token_index, start,
                                     token_count, profile))
            break;
        ++token_index;
    }
    if (end <= start) end = start + 1u;
    if (end > impl->syllable_count) end = impl->syllable_count;
    return end;
}

static bool high_initial_phone(nanotts_phone_t phone)
{
    return nanotts_phone_is_high_onset(phone) ||
           phone == NANOTTS_PH_S_LAX || phone == NANOTTS_PH_S_TENSE ||
           phone == NANOTTS_PH_S_LAX_PAL || phone == NANOTTS_PH_S_TENSE_PAL ||
           phone == NANOTTS_PH_H;
}

static int8_t clamp_pitch(int value)
{
    if (value < -64) return -64;
    if (value > 63) return 63;
    return (int8_t)value;
}

static int ap_anchor(size_t pos, size_t count, bool high_initial,
                     int range_q4)
{
    int initial = high_initial ? range_q4 : -range_q4;
    int high = range_q4;
    int low = -((range_q4 * 3) / 4);
    if (count <= 1u) return high_initial ? range_q4 / 2 : -range_q4 / 3;
    if (count == 2u) return pos ? high : initial;
    if (count == 3u) return pos == 0u ? initial : (pos == 1u ? low : high);
    if (pos == 0u) return initial;
    if (pos == 1u) return high;
    if (pos + 2u >= count) return pos + 1u == count ? high : low;
    return high - (int)(((long)(high - low) * (long)(pos - 1u)) /
                        (long)(count - 2u));
}

static bool token_range_has_question_word(const nanotts_impl_t *impl,
                                          size_t first_token,
                                          size_t last_token)
{
    size_t i;
    if (last_token >= impl->token_count) last_token = impl->token_count - 1u;
    for (i = first_token; i <= last_token; ++i)
        if ((impl->tokens[i].flags & KO_TOKEN_QUESTION_WORD) != 0u)
            return true;
    return false;
}

static void phrase_modality(const nanotts_impl_t *impl,
                            size_t ap_start, size_t ap_end,
                            bool *terminal, bool *question,
                            bool *wh_question, bool *command)
{
    size_t first_token = impl->syllables[ap_start].token_index;
    size_t last_token = impl->syllables[ap_end - 1u].token_index;
    uint8_t boundary = ap_end < impl->syllable_count
        ? impl->syllables[ap_end].boundary_before
        : impl->trailing_boundary;
    uint16_t flags = impl->tokens[last_token].flags;

    *terminal = ap_end == impl->syllable_count ||
        ko_boundary_strength(boundary) >= NANOTTS_BOUNDARY_PHRASE;
    *question = ((boundary & KO_BOUNDARY_QUESTION) != 0u) ||
        ((flags & KO_TOKEN_QUESTION_END) != 0u);
    *command = (flags & KO_TOKEN_COMMAND_END) != 0u;
    *wh_question = *question &&
        token_range_has_question_word(impl, first_token, last_token);
}

static void pitch_pair(const nanotts_impl_t *impl, size_t position,
                       size_t count, bool high_initial, bool phrase_terminal,
                       bool question, bool wh_question, bool command,
                       int8_t *start_q4, int8_t *end_q4)
{
    const ko_style_profile_t *profile = style_profile(impl);
    int a = ap_anchor(position, count, high_initial, profile->pitch_range_q4);
    int b = ap_anchor(position + 1u < count ? position + 1u : position,
                      count, high_initial, profile->pitch_range_q4);
    if (position + 1u == count && phrase_terminal) {
        if (question) {
            if (wh_question)
                b -= profile->pitch_range_q4 / 2;
            else
                b += profile->pitch_range_q4 + profile->pitch_range_q4 / 2;
        } else if (command) {
            b -= profile->pitch_range_q4;
        } else {
            b -= profile->pitch_range_q4 + profile->pitch_range_q4 / 2;
        }
    }
    *start_q4 = clamp_pitch(a);
    *end_q4 = clamp_pitch(b);
}

static uint8_t scaled_percent(uint8_t base, uint8_t scale)
{
    unsigned value = ((unsigned)base * (unsigned)scale + 50u) / 100u;
    if (value < 20u) value = 20u;
    if (value > 255u) value = 255u;
    return (uint8_t)value;
}

static uint8_t onset_duration(nanotts_phone_t phone, bool ap_initial,
                              const ko_style_profile_t *profile)
{
    nanotts_phone_kind_t kind = (nanotts_phone_kind_t)nanotts_phone_def(phone)->kind;
    uint8_t base;
    switch (kind) {
    case NANOTTS_KIND_STOP_LAX: base = ap_initial ? 100u : 84u; break;
    case NANOTTS_KIND_STOP_TENSE: base = ap_initial ? 100u : 91u; break;
    case NANOTTS_KIND_STOP_ASP: base = ap_initial ? 100u : 89u; break;
    case NANOTTS_KIND_AFFRICATE_LAX: base = ap_initial ? 100u : 88u; break;
    case NANOTTS_KIND_AFFRICATE_TENSE: base = ap_initial ? 100u : 92u; break;
    case NANOTTS_KIND_AFFRICATE_ASP: base = ap_initial ? 100u : 91u; break;
    case NANOTTS_KIND_FRICATIVE_LAX:
    case NANOTTS_KIND_FRICATIVE_TENSE: base = ap_initial ? 100u : 93u; break;
    case NANOTTS_KIND_FRICATIVE_H: base = ap_initial ? 96u : 79u; break;
    case NANOTTS_KIND_NASAL: base = ap_initial ? 96u : 90u; break;
    case NANOTTS_KIND_LATERAL: base = 92u; break;
    case NANOTTS_KIND_TAP: base = 84u; break;
    default: base = 100u; break;
    }
    return scaled_percent(base, profile->consonant_q8);
}

static bool push_vowels(nanotts_impl_t *impl,
                         const nanotts_ko_syllable_t *syllable,
                         uint8_t first_flags, uint8_t end_flags,
                         int8_t pitch_start, int8_t pitch_end,
                         uint8_t duration, uint8_t prominence,
                         uint8_t acoustic_flags)
{
    nanotts_phone_t sequence[3];
    size_t count = 0u, i;
    switch (syllable->vowel) {
    case 0: sequence[count++] = NANOTTS_PH_A; break;
    case 1: sequence[count++] = NANOTTS_PH_AE; break;
    case 2: sequence[count++] = NANOTTS_PH_Y; sequence[count++] = NANOTTS_PH_A; break;
    case 3: sequence[count++] = NANOTTS_PH_Y; sequence[count++] = NANOTTS_PH_AE; break;
    case 4: sequence[count++] = NANOTTS_PH_EO; break;
    case 5: sequence[count++] = NANOTTS_PH_E; break;
    case 6: sequence[count++] = NANOTTS_PH_Y; sequence[count++] = NANOTTS_PH_EO; break;
    case 7: sequence[count++] = NANOTTS_PH_Y; sequence[count++] = NANOTTS_PH_E; break;
    case 8: sequence[count++] = NANOTTS_PH_O; break;
    case 9: sequence[count++] = NANOTTS_PH_W; sequence[count++] = NANOTTS_PH_A; break;
    case 10:sequence[count++] = NANOTTS_PH_W; sequence[count++] = NANOTTS_PH_AE; break;
    case 11:sequence[count++] = NANOTTS_PH_W; sequence[count++] = NANOTTS_PH_E; break;
    case 12:sequence[count++] = NANOTTS_PH_Y; sequence[count++] = NANOTTS_PH_O; break;
    case 13:sequence[count++] = NANOTTS_PH_U; break;
    case 14:sequence[count++] = NANOTTS_PH_W; sequence[count++] = NANOTTS_PH_EO; break;
    case 15:sequence[count++] = NANOTTS_PH_W; sequence[count++] = NANOTTS_PH_E; break;
    case 16:sequence[count++] = NANOTTS_PH_W; sequence[count++] = NANOTTS_PH_I; break;
    case 17:sequence[count++] = NANOTTS_PH_Y; sequence[count++] = NANOTTS_PH_U; break;
    case 18:sequence[count++] = NANOTTS_PH_EU; break;
    case 19:
        if (syllable->lexical_onset == 11u &&
            (impl->current_style == NANOTTS_KO_STYLE_PRESCRIPTIVE ||
             impl->current_style == NANOTTS_KO_STYLE_CLEAR_DEVICE ||
             impl->current_style == NANOTTS_KO_STYLE_LEARNER))
            sequence[count++] = NANOTTS_PH_UI;
        else
            sequence[count++] = NANOTTS_PH_I;
        break;
    case 20:sequence[count++] = NANOTTS_PH_I; break;
    default:return false;
    }

    for (i = 0u; i < count; ++i) {
        uint8_t flags = i ? 0u : first_flags;
        uint8_t d = nanotts_phone_is_vowel(sequence[i]) ? duration : 72u;
        int p0 = pitch_start;
        int p1 = pitch_end;
        if (count > 1u) {
            int span = (int)pitch_end - (int)pitch_start;
            p0 = (int)pitch_start + (span * (int)i) / (int)count;
            p1 = (int)pitch_start + (span * (int)(i + 1u)) / (int)count;
        }
        if (i + 1u == count) flags |= end_flags;
        if (!nanotts_push_event_ex(impl, sequence[i], flags, d,
                                  clamp_pitch(p0), clamp_pitch(p1),
                                  prominence, acoustic_flags))
            return false;
    }
    return true;
}

static bool push_pause(nanotts_impl_t *impl, uint8_t boundary,
                       bool question, const ko_style_profile_t *profile)
{
    uint8_t strength = ko_boundary_strength(boundary);
    uint8_t flags = 0u;
    uint8_t duration;
    nanotts_phone_t phone;
    if (strength == NANOTTS_BOUNDARY_NONE) return true;
    if (strength == NANOTTS_BOUNDARY_SPACE) {
        phone = NANOTTS_PH_AP_PAUSE;
        duration = profile->word_pause_q8;
    } else if (strength == NANOTTS_BOUNDARY_COMMA) {
        phone = NANOTTS_PH_PHRASE_PAUSE;
        duration = profile->comma_pause_q8;
        flags = NANOTTS_EVENT_PHRASE_END | NANOTTS_EVENT_CONTINUATION;
    } else {
        phone = NANOTTS_PH_PHRASE_PAUSE;
        duration = profile->phrase_pause_q8;
        flags = NANOTTS_EVENT_PHRASE_END;
        if ((boundary & KO_BOUNDARY_QUESTION) != 0u || question)
            flags |= NANOTTS_EVENT_QUESTION;
    }
    return nanotts_push_event_ex(impl, phone, flags, duration, 0, 0, 128u, 0u);
}

nanotts_result_t ko_emit_events(nanotts_impl_t *impl,
    const nanotts_text_options_t *options)
{
    const ko_style_profile_t *profile = style_profile(impl);
    size_t ap_start = 0u;
    bool no_pause = (options->flags & NANOTTS_OPT_NO_AUTOPAUSE) != 0u;
    impl->event_count = 0u;

    while (ap_start < impl->syllable_count) {
        size_t ap_end = choose_ap_end(impl, ap_start);
        size_t i;
        uint8_t dummy = 0u;
        nanotts_phone_t initial_phone = onset_phone(
            impl->syllables[ap_start].onset,
            impl->syllables[ap_start].vowel, &dummy);
        bool high_initial = initial_phone != NANOTTS_PH_SILENCE &&
                            high_initial_phone(initial_phone);
        bool phrase_terminal, phrase_question, phrase_wh, phrase_command;
        phrase_modality(impl, ap_start, ap_end, &phrase_terminal,
                        &phrase_question, &phrase_wh, &phrase_command);

        for (i = ap_start; i < ap_end; ++i) {
            nanotts_ko_syllable_t *s = &impl->syllables[i];
            bool ap_initial = i == ap_start;
            bool ap_final = i + 1u == ap_end;
            bool utterance_final = ap_final && ap_end == impl->syllable_count;
            bool word_end = (s->flags & KO_SYLLABLE_WORD_FINAL) != 0u;
            bool phrase_edge = ap_final && (utterance_final ||
                (ap_end < impl->syllable_count &&
                 ko_boundary_strength(impl->syllables[ap_end].boundary_before) >=
                    NANOTTS_BOUNDARY_COMMA));
            uint8_t first_flags = NANOTTS_EVENT_SYLLABLE_START;
            uint8_t end_flags = 0u;
            uint8_t acoustic = 0u;
            uint8_t onset_acoustic = 0u;
            uint8_t prominence = 128u;
            int8_t p0, p1;
            nanotts_phone_t onset, coda;
            uint8_t vowel_duration;
            bool closed;

            pitch_pair(impl, i - ap_start, ap_end - ap_start, high_initial,
                       phrase_terminal, phrase_question, phrase_wh,
                       phrase_command, &p0, &p1);
            onset = onset_phone(s->onset, s->vowel, &onset_acoustic);
            coda = coda_phone(s->coda);
            closed = coda != NANOTTS_PH_SILENCE;
            vowel_duration = closed ? profile->vowel_closed_q8 : profile->vowel_open_q8;

            if (ap_initial) {
                first_flags |= NANOTTS_EVENT_AP_START;
                acoustic |= NANOTTS_ACOUSTIC_AP_INITIAL;
            }
            if (ap_final) {
                end_flags |= NANOTTS_EVENT_AP_END;
                acoustic |= NANOTTS_ACOUSTIC_AP_FINAL;
                vowel_duration = scaled_percent(vowel_duration,
                    phrase_edge ? profile->phrase_final_q8 : 108u);
            }
            if (word_end) {
                end_flags |= NANOTTS_EVENT_WORD_END;
                vowel_duration = scaled_percent(vowel_duration, 103u);
            }
            if ((s->flags & KO_SYLLABLE_LONG) != 0u) {
                end_flags |= NANOTTS_EVENT_LONG;
                vowel_duration = scaled_percent(vowel_duration, 135u);
            }
            if ((s->flags & KO_SYLLABLE_FOCUS) != 0u ||
                (impl->tokens[s->token_index].flags & KO_TOKEN_QUESTION_WORD) != 0u) {
                acoustic |= NANOTTS_ACOUSTIC_FOCUSED;
                prominence = profile->prominence_q8;
            } else if (ap_final) {
                prominence = (uint8_t)((128u + profile->prominence_q8) / 2u);
            }
            if (phrase_wh)
                acoustic |= NANOTTS_ACOUSTIC_WH_QUESTION;
            if (phrase_command)
                acoustic |= NANOTTS_ACOUSTIC_COMMAND;

            acoustic |= onset_acoustic;
            if (onset == NANOTTS_PH_H && !ap_initial)
                acoustic |= NANOTTS_ACOUSTIC_WEAK_H;
            if (ap_initial && nasal_phone(onset) &&
                (impl->current_style == NANOTTS_KO_STYLE_MODERN_SEOUL ||
                 impl->current_style == NANOTTS_KO_STYLE_CONVERSATIONAL))
                acoustic |= NANOTTS_ACOUSTIC_DENASALIZED;

            if (onset != NANOTTS_PH_SILENCE) {
                uint8_t odur = onset_duration(onset, ap_initial, profile);
                if (i > 0u && nasal_phone(onset) &&
                    ko_boundary_strength(s->boundary_before) <= NANOTTS_BOUNDARY_SPACE &&
                    nasal_phone(coda_phone(impl->syllables[i - 1u].coda)))
                    odur = scaled_percent(odur, 84u);
                if (!nanotts_push_event_ex(impl, onset, first_flags, odur,
                                           p0, p0, prominence, acoustic))
                    return NANOTTS_ERR_TOO_MANY_EVENTS;
                first_flags = 0u;
            }

            if (!push_vowels(impl, s, first_flags,
                    coda == NANOTTS_PH_SILENCE ? end_flags : 0u,
                    p0, p1, vowel_duration, prominence, acoustic))
                return NANOTTS_ERR_TOO_MANY_EVENTS;

            if (coda != NANOTTS_PH_SILENCE) {
                bool sonorant = nanotts_phone_is_sonorant(coda);
                uint8_t duration = scaled_percent(sonorant ? 78u : 82u,
                                                  profile->consonant_q8);
                if (nasal_phone(coda) && i + 1u < impl->syllable_count) {
                    uint8_t next_acoustic = 0u;
                    nanotts_phone_t next = onset_phone(
                        impl->syllables[i + 1u].onset,
                        impl->syllables[i + 1u].vowel, &next_acoustic);
                    if (nasal_phone(next)) duration = scaled_percent(duration, 88u);
                }
                if (phrase_edge)
                    duration = scaled_percent(duration, sonorant ? 112u : 106u);
                if (!nanotts_push_event_ex(impl, coda, end_flags, duration,
                                           p1, p1, prominence, acoustic))
                    return NANOTTS_ERR_TOO_MANY_EVENTS;
            }
        }

        if (ap_end < impl->syllable_count && !no_pause) {
            uint8_t boundary = impl->syllables[ap_end].boundary_before;
            if (ko_boundary_strength(boundary) == NANOTTS_BOUNDARY_NONE ||
                ko_boundary_strength(boundary) == NANOTTS_BOUNDARY_SPACE)
                boundary = NANOTTS_BOUNDARY_SPACE;
            if (!push_pause(impl, boundary, phrase_question, profile))
                return NANOTTS_ERR_TOO_MANY_EVENTS;
        }
        ap_start = ap_end;
    }

    if (!no_pause) {
        bool question = false;
        if (impl->token_count)
            question = (impl->tokens[impl->token_count - 1u].flags &
                        KO_TOKEN_QUESTION_END) != 0u;
        if (!push_pause(impl, impl->trailing_boundary, question, profile))
            return NANOTTS_ERR_TOO_MANY_EVENTS;
    }
    return impl->event_count ? NANOTTS_OK : NANOTTS_ERR_EMPTY_INPUT;
}
