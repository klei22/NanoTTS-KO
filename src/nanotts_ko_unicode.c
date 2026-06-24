/* SPDX-License-Identifier: MIT */
#include "nanotts_internal.h"

bool ko_utf8_decode(const char *text, size_t length, size_t *offset, uint32_t *cp)
{
    const unsigned char *s = (const unsigned char *)text;
    size_t i = *offset;
    uint32_t value;
    unsigned need, j;
    if (i >= length) return false;
    if (s[i] < 0x80u) {
        *cp = s[i];
        *offset = i + 1u;
        return true;
    }
    if ((s[i] & 0xe0u) == 0xc0u) {
        value = (uint32_t)(s[i] & 0x1fu);
        need = 1u;
        if (value < 2u) return false;
    } else if ((s[i] & 0xf0u) == 0xe0u) {
        value = (uint32_t)(s[i] & 0x0fu);
        need = 2u;
    } else if ((s[i] & 0xf8u) == 0xf0u) {
        value = (uint32_t)(s[i] & 0x07u);
        need = 3u;
        if (value > 4u) return false;
    } else {
        return false;
    }
    if (i + need >= length) return false;
    for (j = 1u; j <= need; ++j) {
        if ((s[i + j] & 0xc0u) != 0x80u) return false;
        value = (value << 6) | (uint32_t)(s[i + j] & 0x3fu);
    }
    if ((need == 1u && value < 0x80u) ||
        (need == 2u && value < 0x800u) ||
        (need == 3u && value < 0x10000u) ||
        value > 0x10ffffu || (value >= 0xd800u && value <= 0xdfffu))
        return false;
    *cp = value;
    *offset = i + (size_t)need + 1u;
    return true;
}

bool ko_is_hangul(uint32_t cp)
{
    return (cp >= KO_S_BASE && cp < KO_S_BASE + KO_S_COUNT) ||
           (cp >= 0x1100u && cp <= 0x11ffu);
}

bool ko_append_syllable(nanotts_impl_t *impl, uint8_t l, uint8_t v,
                        uint8_t t, uint8_t boundary, uint8_t token_index)
{
    nanotts_ko_syllable_t *s;
    if (impl->syllable_count >= NANOTTS_MAX_SYLLABLES) return false;
    s = &impl->syllables[impl->syllable_count++];
    memset(s, 0, sizeof(*s));
    s->onset = l;
    s->vowel = v;
    s->coda = t;
    s->lexical_onset = l;
    s->lexical_coda = t;
    s->boundary_before = boundary;
    s->token_index = token_index;
    s->morpheme_index = 0xffu;
    s->pos = (uint8_t)NANOTTS_KO_POS_UNKNOWN;
    return true;
}

nanotts_result_t ko_append_hangul(nanotts_impl_t *impl, const char *text,
    size_t length, uint8_t boundary, uint8_t token_index,
    nanotts_parse_info_t *info)
{
    size_t off = 0u;
    bool first = true;
    while (off < length) {
        size_t here = off;
        uint32_t cp;
        uint8_t l, v, t;
        if (!ko_utf8_decode(text, length, &off, &cp)) {
            if (info) info->error_byte = here;
            return NANOTTS_ERR_UTF8;
        }
        if (cp >= KO_S_BASE && cp < KO_S_BASE + KO_S_COUNT) {
            uint32_t x = cp - KO_S_BASE;
            l = (uint8_t)(x / KO_N_COUNT);
            v = (uint8_t)((x % KO_N_COUNT) / KO_T_COUNT);
            t = (uint8_t)(x % KO_T_COUNT);
        } else if (cp >= KO_L_BASE && cp < KO_L_BASE + KO_L_COUNT) {
            size_t vat = off;
            uint32_t vcp = 0u;
            if (!ko_utf8_decode(text, length, &off, &vcp) ||
                vcp < KO_V_BASE || vcp >= KO_V_BASE + KO_V_COUNT) {
                if (info) {
                    info->error_byte = vat;
                    info->error_codepoint = vcp;
                }
                return NANOTTS_ERR_UNSUPPORTED_TEXT;
            }
            l = (uint8_t)(cp - KO_L_BASE);
            v = (uint8_t)(vcp - KO_V_BASE);
            t = 0u;
            if (off < length) {
                size_t tat = off;
                uint32_t tcp = 0u;
                if (!ko_utf8_decode(text, length, &off, &tcp)) {
                    if (info) info->error_byte = tat;
                    return NANOTTS_ERR_UTF8;
                }
                if (tcp > KO_T_BASE && tcp < KO_T_BASE + KO_T_COUNT)
                    t = (uint8_t)(tcp - KO_T_BASE);
                else
                    off = tat;
            }
        } else {
            if (info) {
                info->error_byte = here;
                info->error_codepoint = cp;
            }
            return NANOTTS_ERR_UNSUPPORTED_TEXT;
        }
        if (!ko_append_syllable(impl, l, v, t,
                first ? boundary : NANOTTS_BOUNDARY_NONE, token_index))
            return NANOTTS_ERR_TOO_MANY_SYLLABLES;
        first = false;
    }
    return NANOTTS_OK;
}

size_t ko_hangul_syllable_count(const char *text, size_t length)
{
    size_t off = 0u, count = 0u;
    while (off < length) {
        uint32_t cp;
        if (!ko_utf8_decode(text, length, &off, &cp)) return 0u;
        if (cp >= KO_S_BASE && cp < KO_S_BASE + KO_S_COUNT) {
            ++count;
        } else if (cp >= KO_L_BASE && cp < KO_L_BASE + KO_L_COUNT) {
            uint32_t vcp;
            if (!ko_utf8_decode(text, length, &off, &vcp) ||
                vcp < KO_V_BASE || vcp >= KO_V_BASE + KO_V_COUNT)
                return 0u;
            ++count;
            if (off < length) {
                size_t save = off;
                uint32_t tcp;
                if (!ko_utf8_decode(text, length, &off, &tcp)) return 0u;
                if (!(tcp > KO_T_BASE && tcp < KO_T_BASE + KO_T_COUNT)) off = save;
            }
        } else {
            return 0u;
        }
    }
    return count;
}

bool ko_word_equal(const char *word, size_t length, const char *literal)
{
    size_t n = strlen(literal);
    return n == length && memcmp(word, literal, length) == 0;
}

bool ko_word_ends_with(const char *word, size_t length, const char *suffix,
                       size_t *prefix_length)
{
    size_t n = strlen(suffix);
    if (n > length || memcmp(word + length - n, suffix, n) != 0) return false;
    if (prefix_length) *prefix_length = length - n;
    return true;
}
