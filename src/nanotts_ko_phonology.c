/* SPDX-License-Identifier: MIT */
#include "nanotts_internal.h"

/*
 * Ordered Korean surface-phonology pass.
 *
 * The spelling representation is retained in lexical_onset/lexical_coda while
 * onset/coda are destructively rewritten to the surface representation used by
 * the acoustic planner.  Morphology-sensitive fortition happens before
 * resyllabification and coda neutralization because those processes otherwise
 * erase the information needed by Rules 24--27 of the standard pronunciation
 * system.
 */

static bool boundary_allows(const nanotts_ko_syllable_t *right,
                            uint16_t flags, bool allow_space)
{
    uint8_t strength = ko_boundary_strength(right->boundary_before);
    if (strength == NANOTTS_BOUNDARY_NONE) return true;
    return allow_space && strength == NANOTTS_BOUNDARY_SPACE &&
           (flags & NANOTTS_OPT_LINK_EOJEOL) != 0u;
}

static bool vowel_i(uint8_t vowel) { return vowel == 20u; }
static bool vowel_i_or_y(uint8_t vowel)
{
    return vowel == 20u || vowel == 2u || vowel == 3u || vowel == 6u ||
           vowel == 7u || vowel == 12u || vowel == 17u;
}

static uint8_t coda_to_onset(uint8_t coda)
{
    switch (coda) {
    case 1: return 0u;   /* ㄱ */
    case 2: return 1u;   /* ㄲ */
    case 4: return 2u;   /* ㄴ */
    case 7: return 3u;   /* ㄷ */
    case 8: return 5u;   /* ㄹ */
    case 16: return 6u;  /* ㅁ */
    case 17: return 7u;  /* ㅂ */
    case 19: return 9u;  /* ㅅ */
    case 20: return 10u; /* ㅆ */
    case 22: return 12u; /* ㅈ */
    case 23: return 14u; /* ㅊ */
    case 24: return 15u; /* ㅋ */
    case 25: return 16u; /* ㅌ */
    case 26: return 17u; /* ㅍ */
    case 27: return 18u; /* ㅎ */
    default: return 0xffu;
    }
}

static bool lax_fortifiable(uint8_t onset)
{
    return onset == 0u || onset == 3u || onset == 7u ||
           onset == 9u || onset == 12u;
}

static uint8_t tense(uint8_t onset)
{
    switch (onset) {
    case 0: return 1u;
    case 3: return 4u;
    case 7: return 8u;
    case 9: return 10u;
    case 12: return 13u;
    default: return onset;
    }
}

static uint8_t aspirate(uint8_t onset)
{
    switch (onset) {
    case 0: return 15u;
    case 3: return 16u;
    case 7: return 17u;
    case 12: return 14u;
    default: return onset;
    }
}

static const nanotts_morpheme_t *morpheme_at(
    const nanotts_impl_t *impl, const nanotts_ko_syllable_t *syllable)
{
    if (syllable->morpheme_index == 0xffu ||
        syllable->morpheme_index >= impl->morpheme_count)
        return NULL;
    return &impl->morphemes[syllable->morpheme_index];
}

static bool is_morph_edge(const nanotts_ko_syllable_t *left,
                          const nanotts_ko_syllable_t *right)
{
    return left->morpheme_index != right->morpheme_index ||
           (left->flags & KO_SYLLABLE_MORPH_FINAL) != 0u ||
           (right->flags & KO_SYLLABLE_MORPH_INITIAL) != 0u;
}

static void morphology_conditioned_tensing(nanotts_impl_t *impl,
                                            uint16_t flags)
{
    size_t i;
    for (i = 0u; i + 1u < impl->syllable_count; ++i) {
        nanotts_ko_syllable_t *left = &impl->syllables[i];
        nanotts_ko_syllable_t *right = &impl->syllables[i + 1u];
        const nanotts_morpheme_t *lm, *rm;
        bool verbal_boundary;

        if (!boundary_allows(right, flags, true) ||
            !lax_fortifiable(right->onset))
            continue;

        lm = morpheme_at(impl, left);
        rm = morpheme_at(impl, right);

        /* Attributive -(으)ㄹ before a following lexical onset.  This rule may
         * cross an eojeol boundary, so the morphology flag is more useful than
         * an orthographic adjacency heuristic. */
        if ((left->morph_flags & NANOTTS_MORPH_ATTRIBUTIVE_L) != 0u &&
            left->lexical_coda == 8u) {
            right->onset = tense(right->onset);
            continue;
        }

        if (!lm || !rm || !is_morph_edge(left, right)) continue;
        verbal_boundary = lm->pos == NANOTTS_KO_POS_VERB_STEM ||
                          lm->pos == NANOTTS_KO_POS_ADJECTIVE_STEM;
        if (!verbal_boundary) continue;

        /* Passive/causative -기 is explicitly exempt, while nominalizing -기
         * participates in fortition. */
        if ((rm->flags & NANOTTS_MORPH_PASSIVE_CAUSATIVE) != 0u) continue;

        if (rm->pos == NANOTTS_KO_POS_ENDING ||
            (rm->flags & NANOTTS_MORPH_NOMINALIZER_GI) != 0u) {
            uint8_t c = left->lexical_coda;
            if (c == 4u || c == 5u || c == 10u || c == 16u ||
                c == 11u || c == 13u) {
                right->onset = tense(right->onset);
            }
        }
    }
}

static bool compound_n_insertion_allowed(const nanotts_impl_t *impl,
                                         size_t left_index,
                                         uint16_t flags)
{
    const nanotts_ko_syllable_t *right = &impl->syllables[left_index + 1u];
    const nanotts_ko_token_t *left_token;
    const nanotts_ko_token_t *right_token;
    if ((flags & NANOTTS_OPT_LINK_EOJEOL) == 0u ||
        ko_boundary_strength(right->boundary_before) != NANOTTS_BOUNDARY_SPACE)
        return false;
    left_token = &impl->tokens[impl->syllables[left_index].token_index];
    right_token = &impl->tokens[right->token_index];
    /* Do not insert across a strong syntactic/prosodic boundary.  Keep-with-
     * next markers, counters, and noun compounds are the most plausible compact
     * contexts; lexical exceptions cover the irregular core vocabulary. */
    if ((left_token->flags & KO_TOKEN_STRONG_BREAK) != 0u) return false;
    if ((left_token->flags & KO_TOKEN_KEEP_WITH_NEXT) != 0u) return true;
    if ((right_token->flags & KO_TOKEN_COUNTER) != 0u) return true;
    return (left_token->flags & KO_TOKEN_NOUN) != 0u &&
           (right_token->flags & KO_TOKEN_NOUN) != 0u;
}

static void palatalize_and_link(nanotts_impl_t *impl, uint16_t flags)
{
    size_t i;
    for (i = 0u; i + 1u < impl->syllable_count; ++i) {
        nanotts_ko_syllable_t *a = &impl->syllables[i];
        nanotts_ko_syllable_t *b = &impl->syllables[i + 1u];
        uint8_t moved;
        if (!boundary_allows(b, flags, true)) continue;

        if (compound_n_insertion_allowed(impl, i, flags) && a->coda != 0u &&
            b->onset == 11u && vowel_i_or_y(b->vowel)) {
            b->onset = 2u;
            continue;
        }

        /* ㄷ/ㅌ + grammatical 이 and corresponding ㅎ combinations. */
        if (vowel_i(b->vowel) && b->onset == 11u) {
            if (a->coda == 7u) { a->coda = 0u; b->onset = 12u; continue; }
            if (a->coda == 25u) { a->coda = 0u; b->onset = 14u; continue; }
            if (a->coda == 13u) { a->coda = 8u; b->onset = 14u; continue; }
        }
        if (vowel_i(b->vowel) && b->onset == 18u &&
            (a->coda == 7u || a->coda == 25u)) {
            a->coda = 0u;
            b->onset = 14u;
            continue;
        }

        if (b->onset != 11u || !a->coda || a->coda == 21u) continue;
        switch (a->coda) {
        case 3: a->coda = 1u; b->onset = 9u; continue;  /* ㄳ */
        case 5: a->coda = 4u; b->onset = 12u; continue; /* ㄵ */
        case 6: a->coda = 0u; b->onset = 2u; continue;  /* ㄶ */
        case 9: a->coda = 8u; b->onset = 0u; continue;  /* ㄺ */
        case 10:a->coda = 8u; b->onset = 6u; continue;  /* ㄻ */
        case 11:a->coda = 8u; b->onset = 7u; continue;  /* ㄼ */
        case 12:a->coda = 8u; b->onset = 10u; continue; /* ㄽ */
        case 13:a->coda = 8u; b->onset = 16u; continue; /* ㄾ */
        case 14:a->coda = 8u; b->onset = 17u; continue; /* ㄿ */
        case 15:a->coda = 0u; b->onset = 5u; continue;  /* ㅀ */
        case 18:a->coda = 17u; b->onset = 9u; continue; /* ㅄ */
        default: break;
        }
        if (a->coda == 27u) {
            a->coda = 0u;
            continue;
        }
        moved = coda_to_onset(a->coda);
        if (moved != 0xffu) {
            a->coda = 0u;
            b->onset = moved;
        }
    }
}

static void resolve_clusters(nanotts_impl_t *impl, uint16_t flags)
{
    size_t i;
    for (i = 0u; i < impl->syllable_count; ++i) {
        nanotts_ko_syllable_t *a = &impl->syllables[i];
        nanotts_ko_syllable_t *b = i + 1u < impl->syllable_count ?
                                   &impl->syllables[i + 1u] : NULL;
        bool linked = b && boundary_allows(b, flags, true);
        switch (a->coda) {
        case 3: a->coda = 1u; break;
        case 5: a->coda = 4u; break; /* ㄵ: fortition is verb-morphology conditioned */
        case 6: a->coda = 4u; if (linked) b->onset = aspirate(b->onset); break;
        case 9:
            if (linked && b->onset == 0u) { a->coda = 8u; b->onset = 1u; }
            else a->coda = 1u;
            break;
        case 10: a->coda = 16u; break; /* ㄻ: fortition is verb-morphology conditioned */
        case 11: a->coda = 8u; break;  /* ㄼ */
        case 12: a->coda = 8u; break;  /* ㄽ */
        case 13: a->coda = 8u; break;  /* ㄾ */
        case 14: a->coda = 26u; break;
        case 15:
            a->coda = 8u;
            if (linked) {
                uint8_t x = aspirate(b->onset);
                b->onset = x != b->onset ? x : tense(b->onset);
            }
            break;
        case 18: a->coda = 17u; break;
        default: break;
        }
    }
}

static void h_rules(nanotts_impl_t *impl, uint16_t flags)
{
    size_t i;
    for (i = 0u; i + 1u < impl->syllable_count; ++i) {
        nanotts_ko_syllable_t *a = &impl->syllables[i];
        nanotts_ko_syllable_t *b = &impl->syllables[i + 1u];
        if (!boundary_allows(b, flags, true)) continue;
        if (a->coda == 27u) {
            uint8_t x = aspirate(b->onset);
            if (x != b->onset) { a->coda = 0u; b->onset = x; }
            else if (b->onset == 9u) { a->coda = 0u; b->onset = 10u; }
            else if (b->onset == 2u || b->onset == 6u) a->coda = 4u;
            else if (b->onset == 11u &&
                     impl->current_style == NANOTTS_KO_STYLE_CONVERSATIONAL)
                a->coda = 0u;
        } else if (b->onset == 18u) {
            switch (a->coda) {
            case 1: case 2: case 24: a->coda = 0u; b->onset = 15u; break;
            case 7: case 19: case 20: case 25: a->coda = 0u; b->onset = 16u; break;
            case 22: case 23: a->coda = 0u; b->onset = 14u; break;
            case 17: case 26: a->coda = 0u; b->onset = 17u; break;
            default:
                /* Intervocalic /h/ is retained as a weak acoustic gesture in
                 * careful styles and deleted in conversational style. */
                if (a->coda == 0u &&
                    impl->current_style == NANOTTS_KO_STYLE_CONVERSATIONAL)
                    b->onset = 11u;
                break;
            }
        }
    }
}

static uint8_t neutral(uint8_t coda)
{
    switch (coda) {
    case 0: return KO_CODA_NONE;
    case 1: case 2: case 3: case 9: case 24: return KO_CODA_K;
    case 4: case 5: case 6: return KO_CODA_N;
    case 7: case 19: case 20: case 22: case 23: case 25: case 27:
        return KO_CODA_T;
    case 8: case 11: case 12: case 13: case 15: return KO_CODA_L;
    case 10: case 16: return KO_CODA_M;
    case 14: case 17: case 18: case 26: return KO_CODA_P;
    case 21: return KO_CODA_NG;
    default: return KO_CODA_NONE;
    }
}

static void lateralize(nanotts_impl_t *impl, uint16_t flags)
{
    size_t i;
    for (i = 0u; i + 1u < impl->syllable_count; ++i) {
        nanotts_ko_syllable_t *a = &impl->syllables[i];
        nanotts_ko_syllable_t *b = &impl->syllables[i + 1u];
        if (!boundary_allows(b, flags, true)) continue;
        if (a->coda == 4u && b->onset == 5u) {
            a->coda = 8u;
            b->onset = KO_L_LATERAL;
        } else if (a->coda == 8u && b->onset == 2u) {
            b->onset = KO_L_LATERAL;
        } else if ((a->coda == 16u || a->coda == 21u) && b->onset == 5u) {
            b->onset = 2u;
        }
    }
}

static void surface_rules(nanotts_impl_t *impl, uint16_t flags)
{
    size_t i;
    for (i = 0u; i < impl->syllable_count; ++i)
        impl->syllables[i].coda = neutral(impl->syllables[i].coda);

    for (i = 0u; i + 1u < impl->syllable_count; ++i) {
        nanotts_ko_syllable_t *a = &impl->syllables[i];
        nanotts_ko_syllable_t *b = &impl->syllables[i + 1u];
        if (!boundary_allows(b, flags, true)) continue;

        if (b->onset == 5u) {
            if (a->coda == KO_CODA_N || a->coda == KO_CODA_L) {
                a->coda = KO_CODA_L;
                b->onset = KO_L_LATERAL;
            } else if (a->coda != KO_CODA_NONE) {
                b->onset = 2u;
            }
        }
        if (b->onset == 2u || b->onset == 6u) {
            if (a->coda == KO_CODA_K) a->coda = KO_CODA_NG;
            else if (a->coda == KO_CODA_T) a->coda = KO_CODA_N;
            else if (a->coda == KO_CODA_P) a->coda = KO_CODA_M;
        }
        if (a->coda == KO_CODA_K || a->coda == KO_CODA_T ||
            a->coda == KO_CODA_P)
            b->onset = tense(b->onset);
    }
}

void ko_apply_phonology(nanotts_impl_t *impl, uint16_t flags)
{
    size_t i;
    if (!impl) return;
    /* Make repeated parsing deterministic even if a caller reuses a context. */
    for (i = 0u; i < impl->syllable_count; ++i) {
        impl->syllables[i].onset = impl->syllables[i].lexical_onset;
        impl->syllables[i].coda = impl->syllables[i].lexical_coda;
    }
    morphology_conditioned_tensing(impl, flags);
    palatalize_and_link(impl, flags);
    resolve_clusters(impl, flags);
    h_rules(impl, flags);
    lateralize(impl, flags);
    surface_rules(impl, flags);
}
