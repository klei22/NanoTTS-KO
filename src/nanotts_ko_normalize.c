/* SPDX-License-Identifier: MIT */
#include "nanotts_internal.h"
#include <limits.h>

static bool is_digit_cp(uint32_t cp) { return cp >= '0' && cp <= '9'; }
static bool is_latin_cp(uint32_t cp)
{
    return (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z');
}
static bool is_space_cp(uint32_t cp)
{
    return cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' || cp == 0x3000u;
}
static bool is_comma_cp(uint32_t cp)
{
    return cp == ',' || cp == ';' || cp == ':' || cp == 0x3001u || cp == 0xff0cu;
}
static bool is_question_cp(uint32_t cp) { return cp == '?' || cp == 0xff1fu; }
static bool is_sentence_end_cp(uint32_t cp)
{
    return cp == '.' || cp == '!' || cp == 0x3002u || cp == 0xff01u;
}
static bool is_ignored_quote(uint32_t cp)
{
    return cp == '\'' || cp == 0x2019u || cp == '"' || cp == 0x201cu ||
           cp == 0x201du || cp == '(' || cp == ')' || cp == '[' || cp == ']' ||
           cp == '{' || cp == '}';
}


static size_t scan_number_end(const char *text, size_t length, size_t start)
{
    size_t i = start;
    if (i < length && text[i] == '-') ++i;
    while (i < length) {
        unsigned char c = (unsigned char)text[i];
        if (c >= '0' && c <= '9') {
            ++i;
            continue;
        }
        if (c == '%' && i > start) return i + 1u;
        if ((c == '.' || c == ':' || c == '-' || c == '/' || c == ',') &&
            i > start && i + 1u < length &&
            text[i - 1u] >= '0' && text[i - 1u] <= '9' &&
            text[i + 1u] >= '0' && text[i + 1u] <= '9') {
            ++i;
            continue;
        }
        break;
    }
    return i;
}

static nanotts_result_t add_token(nanotts_impl_t *impl, size_t start, size_t length,
                                  ko_token_type_t type, uint8_t boundary)
{
    nanotts_ko_token_t *t;
    if (impl->token_count >= NANOTTS_MAX_TOKENS) return NANOTTS_ERR_TOO_MANY_TOKENS;
    if (start > UINT32_MAX || length > UINT16_MAX) return NANOTTS_ERR_UNSUPPORTED_TEXT;
    t = &impl->tokens[impl->token_count++];
    memset(t, 0, sizeof(*t));
    t->byte_start = (uint32_t)start;
    t->byte_length = (uint16_t)length;
    t->type = (uint8_t)type;
    t->boundary_before = boundary;
    return NANOTTS_OK;
}

nanotts_result_t ko_scan_tokens(nanotts_impl_t *impl, const char *text,
    size_t length, nanotts_parse_info_t *info)
{
    size_t off = 0u;
    uint8_t pending = NANOTTS_BOUNDARY_PHRASE;
    uint8_t trailing = NANOTTS_BOUNDARY_NONE;
    bool trailing_question = false;
    impl->token_count = 0u;
    impl->utterance_flags = 0u;

    while (off < length) {
        size_t start = off, end, probe;
        uint32_t cp;
        nanotts_result_t r;
        if (!ko_utf8_decode(text, length, &off, &cp)) {
            if (info) info->error_byte = start;
            return NANOTTS_ERR_UTF8;
        }
        if (ko_is_hangul(cp)) {
            end = off;
            while (end < length) {
                uint32_t next;
                probe = end;
                if (!ko_utf8_decode(text, length, &probe, &next)) {
                    if (info) info->error_byte = end;
                    return NANOTTS_ERR_UTF8;
                }
                if (!ko_is_hangul(next)) break;
                end = probe;
            }
            r = add_token(impl, start, end - start, KO_TOKEN_HANGUL, pending);
            if (r != NANOTTS_OK) return r;
            pending = NANOTTS_BOUNDARY_NONE;
            trailing = NANOTTS_BOUNDARY_NONE;
            trailing_question = false;
            off = end;
            continue;
        }
        if (is_digit_cp(cp)) {
            /* Retain only separators that are between digits.  This keeps a
             * sentence-final period or comma out of the numeric token. */
            end = scan_number_end(text, length, start);
            r = add_token(impl, start, end - start, KO_TOKEN_NUMBER, pending);
            if (r != NANOTTS_OK) return r;
            pending = NANOTTS_BOUNDARY_NONE;
            trailing = NANOTTS_BOUNDARY_NONE;
            trailing_question = false;
            off = end;
            continue;
        }
        if (is_latin_cp(cp)) {
            end = off;
            while (end < length) {
                unsigned char c = (unsigned char)text[end];
                if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) ++end;
                else break;
            }
            r = add_token(impl, start, end - start, KO_TOKEN_LATIN, pending);
            if (r != NANOTTS_OK) return r;
            pending = NANOTTS_BOUNDARY_NONE;
            trailing = NANOTTS_BOUNDARY_NONE;
            trailing_question = false;
            off = end;
            continue;
        }
        if (cp == 0x2103u || cp == 0x20a9u || cp == '%' || cp == 0xff05u) {
            r = add_token(impl, start, off - start, KO_TOKEN_SYMBOL, pending);
            if (r != NANOTTS_OK) return r;
            pending = NANOTTS_BOUNDARY_NONE;
            trailing = NANOTTS_BOUNDARY_NONE;
            trailing_question = false;
            continue;
        }
        if (is_space_cp(cp)) {
            if (ko_boundary_strength(pending) < NANOTTS_BOUNDARY_SPACE)
                pending = NANOTTS_BOUNDARY_SPACE;
            if (impl->token_count) trailing = NANOTTS_BOUNDARY_SPACE;
        } else if (is_comma_cp(cp) || cp == 0x2014u) {
            pending = NANOTTS_BOUNDARY_COMMA;
            trailing = NANOTTS_BOUNDARY_COMMA;
            trailing_question = false;
            impl->utterance_flags |= KO_UTTERANCE_CONTINUATION;
        } else if (is_question_cp(cp)) {
            pending = (uint8_t)(NANOTTS_BOUNDARY_PHRASE | KO_BOUNDARY_QUESTION);
            trailing = NANOTTS_BOUNDARY_PHRASE;
            trailing_question = true;
            impl->utterance_flags |= KO_UTTERANCE_QUESTION;
            if (impl->token_count)
                impl->tokens[impl->token_count - 1u].flags |= KO_TOKEN_QUESTION_END;
        } else if (is_sentence_end_cp(cp)) {
            pending = NANOTTS_BOUNDARY_PHRASE;
            trailing = NANOTTS_BOUNDARY_PHRASE;
            trailing_question = false;
            if (cp == '!' || cp == 0xff01u) {
                impl->utterance_flags |= KO_UTTERANCE_COMMAND;
                if (impl->token_count)
                    impl->tokens[impl->token_count - 1u].flags |= KO_TOKEN_COMMAND_END;
            }
        } else if (cp == '-' && off < length && text[off] >= '0' && text[off] <= '9') {
            /* A leading minus belongs to the following numeric token. */
            end = scan_number_end(text, length, start);
            r = add_token(impl, start, end - start, KO_TOKEN_NUMBER, pending);
            if (r != NANOTTS_OK) return r;
            pending = NANOTTS_BOUNDARY_NONE;
            trailing = NANOTTS_BOUNDARY_NONE;
            trailing_question = false;
            off = end;
        } else if (!is_ignored_quote(cp)) {
            if (info) {
                info->error_byte = start;
                info->error_codepoint = cp;
            }
            return NANOTTS_ERR_UNSUPPORTED_TEXT;
        }
    }
    if (!impl->token_count) return NANOTTS_ERR_EMPTY_INPUT;
    impl->trailing_boundary = trailing;
    if (trailing_question) impl->trailing_boundary |= KO_BOUNDARY_QUESTION;
    return NANOTTS_OK;
}

static nanotts_result_t append_literal(nanotts_impl_t *impl, const char *s,
    uint8_t *boundary, uint8_t token_index, nanotts_parse_info_t *info)
{
    nanotts_result_t r = ko_append_hangul(impl, s, strlen(s), *boundary,
                                         token_index, info);
    if (r == NANOTTS_OK) *boundary = NANOTTS_BOUNDARY_NONE;
    return r;
}

static const char *const DIGITS[10] =
    {"영","일","이","삼","사","오","육","칠","팔","구"};

static nanotts_result_t append_digits(nanotts_impl_t *impl, const char *digits,
    size_t length, uint8_t *boundary, uint8_t token_index,
    nanotts_parse_info_t *info)
{
    size_t i;
    for (i = 0u; i < length; ++i) {
        unsigned d;
        nanotts_result_t r;
        if (digits[i] < '0' || digits[i] > '9') continue;
        d = (unsigned)(digits[i] - '0');
        r = append_literal(impl, DIGITS[d], boundary, token_index, info);
        if (r != NANOTTS_OK) return r;
        *boundary = NANOTTS_BOUNDARY_SPACE;
    }
    return NANOTTS_OK;
}

static nanotts_result_t append_four(nanotts_impl_t *impl, unsigned value,
    uint8_t *boundary, uint8_t token_index, nanotts_parse_info_t *info)
{
    static const unsigned divs[4] = {1000u,100u,10u,1u};
    static const char *const units[4] = {"천","백","십",""};
    unsigned i;
    for (i = 0u; i < 4u; ++i) {
        unsigned d = value / divs[i];
        value %= divs[i];
        if (!d) continue;
        if (d != 1u || i == 3u) {
            nanotts_result_t r = append_literal(impl, DIGITS[d], boundary,
                                                token_index, info);
            if (r != NANOTTS_OK) return r;
        }
        if (*units[i]) {
            nanotts_result_t r = append_literal(impl, units[i], boundary,
                                                token_index, info);
            if (r != NANOTTS_OK) return r;
        }
    }
    return NANOTTS_OK;
}

static nanotts_result_t append_sino_u64(nanotts_impl_t *impl, uint64_t value,
    uint8_t *boundary, uint8_t token_index, nanotts_parse_info_t *info)
{
    static const char *const big[] = {"","만","억","조","경"};
    unsigned groups[5] = {0u,0u,0u,0u,0u};
    unsigned gc = 0u, i;
    if (!value) return append_literal(impl, DIGITS[0], boundary, token_index, info);
    while (value && gc < 5u) {
        groups[gc++] = (unsigned)(value % 10000u);
        value /= 10000u;
    }
    if (value) return NANOTTS_ERR_UNSUPPORTED_TEXT;
    for (i = gc; i > 0u; --i) {
        unsigned gi = i - 1u;
        if (groups[gi]) {
            nanotts_result_t r = NANOTTS_OK;
            if (!(gi == 1u && groups[gi] == 1u))
                r = append_four(impl, groups[gi], boundary, token_index, info);
            if (r != NANOTTS_OK) return r;
            if (gi) {
                r = append_literal(impl, big[gi], boundary, token_index, info);
                if (r != NANOTTS_OK) return r;
            }
        }
    }
    return NANOTTS_OK;
}

static bool parse_uint(const char *s, size_t n, uint64_t *value)
{
    size_t i;
    uint64_t v = 0u;
    if (!n) return false;
    for (i = 0u; i < n; ++i) {
        unsigned d;
        if (s[i] == ',') continue;
        if (s[i] < '0' || s[i] > '9') return false;
        d = (unsigned)(s[i] - '0');
        if (v > (UINT64_MAX - d) / 10u) return false;
        v = v * 10u + d;
    }
    *value = v;
    return true;
}

static nanotts_result_t append_native_under_100(nanotts_impl_t *impl,
    unsigned value, bool counter_form, uint8_t *boundary, uint8_t token_index,
    nanotts_parse_info_t *info)
{
    static const char *const ones_plain[10] =
        {"","하나","둘","셋","넷","다섯","여섯","일곱","여덟","아홉"};
    static const char *const ones_counter[10] =
        {"","한","두","세","네","다섯","여섯","일곱","여덟","아홉"};
    static const char *const tens[10] =
        {"","열","스물","서른","마흔","쉰","예순","일흔","여든","아흔"};
    unsigned t = value / 10u, o = value % 10u;
    nanotts_result_t r;
    if (!value || value >= 100u) return append_sino_u64(impl, value, boundary,
                                                        token_index, info);
    if (t) {
        const char *ten = (counter_form && value == 20u) ? "스무" : tens[t];
        r = append_literal(impl, ten, boundary, token_index, info);
        if (r != NANOTTS_OK) return r;
    }
    if (o) {
        r = append_literal(impl, counter_form ? ones_counter[o] : ones_plain[o],
                           boundary, token_index, info);
        if (r != NANOTTS_OK) return r;
    }
    return NANOTTS_OK;
}

static bool native_counter(const char *word, size_t length)
{
    static const char *const counters[] = {
        "시","개","명","마리","살","잔","권","대","장","벌","켤레",
        "번","곳","사람","채","송이","자루","통","그릇"
    };
    size_t i;
    for (i = 0u; i < sizeof(counters) / sizeof(counters[0]); ++i)
        if (ko_word_equal(word, length, counters[i])) return true;
    return false;
}

static bool find_one(const char *s, size_t n, char needle, size_t *position)
{
    size_t i, found = NANOTTS_NPOS;
    for (i = 0u; i < n; ++i) {
        if (s[i] == needle) {
            if (found != NANOTTS_NPOS) return false;
            found = i;
        }
    }
    if (found == NANOTTS_NPOS) return false;
    *position = found;
    return true;
}

static bool find_two(const char *s, size_t n, char needle, size_t *a, size_t *b)
{
    size_t i, count = 0u;
    for (i = 0u; i < n; ++i) {
        if (s[i] == needle) {
            if (count == 0u) *a = i;
            else if (count == 1u) *b = i;
            ++count;
        }
    }
    return count == 2u;
}


static size_t count_char(const char *s, size_t n, char needle)
{
    size_t i, count = 0u;
    for (i = 0u; i < n; ++i) if (s[i] == needle) ++count;
    return count;
}

static nanotts_result_t append_number_token(nanotts_impl_t *impl,
    const char *s, size_t n, const char *next_word, size_t next_length,
    uint8_t boundary, uint8_t token_index, const nanotts_text_options_t *options,
    nanotts_parse_info_t *info)
{
    size_t p, p2;
    uint64_t value;
    bool negative = n && s[0] == '-';
    bool percent = n && s[n - 1u] == '%';
    size_t begin = negative ? 1u : 0u;
    size_t end = percent ? n - 1u : n;
    nanotts_result_t r;

    if (negative) {
        r = append_literal(impl, "마이너스", &boundary, token_index, info);
        if (r != NANOTTS_OK) return r;
    }

    if (!(options->flags & NANOTTS_OPT_NO_NORMALIZATION) &&
        find_two(s + begin, end - begin, '-', &p, &p2)) {
        uint64_t year, month, day;
        p += begin;
        p2 += begin;
        if (p - begin == 4u && p2 - p - 1u >= 1u &&
            p2 - p - 1u <= 2u && end - p2 - 1u >= 1u &&
            end - p2 - 1u <= 2u &&
            parse_uint(s + begin, p - begin, &year) &&
            parse_uint(s + p + 1u, p2 - p - 1u, &month) &&
            parse_uint(s + p2 + 1u, end - p2 - 1u, &day) &&
            month >= 1u && month <= 12u && day >= 1u && day <= 31u) {
            r = append_sino_u64(impl, year, &boundary, token_index, info);
            if (r == NANOTTS_OK) r = append_literal(impl, "년", &boundary, token_index, info);
            if (r == NANOTTS_OK) r = append_sino_u64(impl, month, &boundary, token_index, info);
            if (r == NANOTTS_OK) r = append_literal(impl, "월", &boundary, token_index, info);
            if (r == NANOTTS_OK) r = append_sino_u64(impl, day, &boundary, token_index, info);
            if (r == NANOTTS_OK) r = append_literal(impl, "일", &boundary, token_index, info);
            if (r == NANOTTS_OK && info) ++info->normalization_count;
            return r;
        }
    }

    if (!(options->flags & NANOTTS_OPT_NO_NORMALIZATION) &&
        find_one(s + begin, end - begin, ':', &p)) {
        uint64_t hour, minute;
        p += begin;
        if (parse_uint(s + begin, p - begin, &hour) &&
            parse_uint(s + p + 1u, end - p - 1u, &minute) &&
            hour <= 24u && minute <= 59u) {
            if (hour == 0u) r = append_literal(impl, "영", &boundary, token_index, info);
            else r = append_native_under_100(impl, (unsigned)hour, true,
                                             &boundary, token_index, info);
            if (r == NANOTTS_OK) r = append_literal(impl, "시", &boundary, token_index, info);
            if (r == NANOTTS_OK) r = append_sino_u64(impl, minute, &boundary, token_index, info);
            if (r == NANOTTS_OK) r = append_literal(impl, "분", &boundary, token_index, info);
            if (r == NANOTTS_OK && info) ++info->normalization_count;
            return r;
        }
    }

    if (!(options->flags & NANOTTS_OPT_NO_NORMALIZATION) &&
        find_one(s + begin, end - begin, '/', &p)) {
        uint64_t numerator, denominator;
        p += begin;
        if (parse_uint(s + begin, p - begin, &numerator) &&
            parse_uint(s + p + 1u, end - p - 1u, &denominator) && denominator) {
            r = append_sino_u64(impl, denominator, &boundary, token_index, info);
            if (r == NANOTTS_OK) r = append_literal(impl, "분의", &boundary, token_index, info);
            if (r == NANOTTS_OK) r = append_sino_u64(impl, numerator, &boundary, token_index, info);
            if (r == NANOTTS_OK && info) ++info->normalization_count;
            return r;
        }
    }

    if (!(options->flags & NANOTTS_OPT_NO_NORMALIZATION) &&
        count_char(s + begin, end - begin, '.') > 1u) {
        size_t segment = begin, i;
        for (i = begin; i <= end; ++i) {
            if (i == end || s[i] == '.') {
                uint64_t part;
                if (i == segment || !parse_uint(s + segment, i - segment, &part))
                    return append_digits(impl, s + begin, end - begin,
                                         &boundary, token_index, info);
                r = append_sino_u64(impl, part, &boundary, token_index, info);
                if (r != NANOTTS_OK) return r;
                if (i < end) {
                    r = append_literal(impl, "점", &boundary, token_index, info);
                    if (r != NANOTTS_OK) return r;
                }
                segment = i + 1u;
            }
        }
        if (percent) {
            r = append_literal(impl, "퍼센트", &boundary, token_index, info);
            if (r != NANOTTS_OK) return r;
        }
        if (info) ++info->normalization_count;
        return NANOTTS_OK;
    }

    if (!(options->flags & NANOTTS_OPT_NO_NORMALIZATION) &&
        find_one(s + begin, end - begin, '.', &p)) {
        p += begin;
        if (parse_uint(s + begin, p - begin, &value)) {
            r = append_sino_u64(impl, value, &boundary, token_index, info);
            if (r == NANOTTS_OK) r = append_literal(impl, "점", &boundary, token_index, info);
            if (r == NANOTTS_OK)
                r = append_digits(impl, s + p + 1u, end - p - 1u,
                                  &boundary, token_index, info);
            if (r == NANOTTS_OK && percent)
                r = append_literal(impl, "퍼센트", &boundary, token_index, info);
            if (r == NANOTTS_OK && info) ++info->normalization_count;
            return r;
        }
    }

    if (!parse_uint(s + begin, end - begin, &value)) {
        r = append_digits(impl, s + begin, end - begin, &boundary,
                          token_index, info);
        if (r == NANOTTS_OK && info) ++info->normalization_count;
        return r;
    }

    if ((end - begin > 1u && s[begin] == '0') || end - begin > 19u ||
        (options->flags & NANOTTS_OPT_NO_NORMALIZATION)) {
        r = append_digits(impl, s + begin, end - begin, &boundary,
                          token_index, info);
    } else if (next_word && native_counter(next_word, next_length) && value < 100u) {
        r = append_native_under_100(impl, (unsigned)value, true, &boundary,
                                    token_index, info);
    } else {
        r = append_sino_u64(impl, value, &boundary, token_index, info);
    }
    if (r == NANOTTS_OK && percent)
        r = append_literal(impl, "퍼센트", &boundary, token_index, info);
    if (r == NANOTTS_OK && info) ++info->normalization_count;
    return r;
}

static const char *const LATIN_NAMES[26] = {
    "에이","비","씨","디","이","에프","지","에이치","아이","제이",
    "케이","엘","엠","엔","오","피","큐","알","에스","티","유",
    "브이","더블유","엑스","와이","제트"
};

static bool ascii_equal_ci(const char *a, size_t length, const char *b)
{
    size_t i;
    if (strlen(b) != length) return false;
    for (i = 0u; i < length; ++i) {
        unsigned char x = (unsigned char)a[i];
        unsigned char y = (unsigned char)b[i];
        if (x >= 'a' && x <= 'z') x = (unsigned char)(x - 'a' + 'A');
        if (y >= 'a' && y <= 'z') y = (unsigned char)(y - 'a' + 'A');
        if (x != y) return false;
    }
    return true;
}

static const char *latin_unit_name(const char *s, size_t length)
{
    static const struct { const char *symbol; const char *spoken; } units[] = {
        {"kg","킬로그램"},{"mg","밀리그램"},{"g","그램"},
        {"km","킬로미터"},{"cm","센티미터"},{"mm","밀리미터"},
        {"m","미터"},{"kHz","킬로헤르츠"},{"MHz","메가헤르츠"},
        {"GHz","기가헤르츠"},{"Hz","헤르츠"},{"mV","밀리볼트"},
        {"V","볼트"},{"mA","밀리암페어"},{"A","암페어"},
        {"kW","킬로와트"},{"W","와트"},{"GB","기가바이트"},
        {"MB","메가바이트"},{"KB","킬로바이트"}
    };
    size_t i;
    for (i = 0u; i < sizeof(units) / sizeof(units[0]); ++i)
        if (ascii_equal_ci(s, length, units[i].symbol)) return units[i].spoken;
    return NULL;
}

static nanotts_result_t append_latin(nanotts_impl_t *impl, const char *s,
    size_t length, uint8_t boundary, uint8_t token_index,
    const nanotts_text_options_t *options, nanotts_parse_info_t *info)
{
    size_t i;
    const char *unit = NULL;
    if (!(options->flags & NANOTTS_OPT_NO_NORMALIZATION) && token_index > 0u &&
        impl->tokens[token_index - 1u].type == KO_TOKEN_NUMBER)
        unit = latin_unit_name(s, length);
    if (unit) {
        if (info) ++info->normalization_count;
        return ko_append_hangul(impl, unit, strlen(unit), boundary,
                                token_index, info);
    }
    for (i = 0u; i < length; ++i) {
        unsigned char c = (unsigned char)s[i];
        unsigned index;
        nanotts_result_t r;
        if (c >= 'a' && c <= 'z') c = (unsigned char)(c - 'a' + 'A');
        if (c < 'A' || c > 'Z') continue;
        index = (unsigned)(c - 'A');
        r = append_literal(impl, LATIN_NAMES[index], &boundary, token_index, info);
        if (r != NANOTTS_OK) return r;
        boundary = NANOTTS_BOUNDARY_SPACE;
    }
    if (info) ++info->normalization_count;
    return NANOTTS_OK;
}

static nanotts_result_t append_word(nanotts_impl_t *impl, const char *word,
    size_t length, uint8_t boundary, uint8_t token_index,
    const nanotts_text_options_t *options, nanotts_parse_info_t *info)
{
    const char *spoken = NULL;
    size_t spoken_length = 0u;
    uint16_t morph_flags = 0u;
    bool selected_variant = false;
    size_t before = impl->syllable_count, i;

    if (impl->lexicon_ex && impl->lexicon_ex(impl->lexicon_ex_user,
            (nanotts_ko_style_t)options->style, word, length, &spoken,
            &spoken_length, &morph_flags)) {
        if (!spoken) return NANOTTS_ERR_ARGUMENT;
        if (spoken_length == NANOTTS_NPOS) spoken_length = strlen(spoken);
        if (info) ++info->lexicon_hits;
    } else if (impl->lexicon && impl->lexicon(impl->lexicon_user, word, length,
               &spoken, &spoken_length)) {
        if (!spoken) return NANOTTS_ERR_ARGUMENT;
        if (spoken_length == NANOTTS_NPOS) spoken_length = strlen(spoken);
        if (info) ++info->lexicon_hits;
    } else if (!(options->flags & NANOTTS_OPT_NO_BUILTIN_LEXICON)) {
        spoken = ko_builtin_pronunciation(word, length,
            (nanotts_ko_style_t)options->style, &spoken_length, &morph_flags,
            &selected_variant);
        if (spoken && info) ++info->lexicon_hits;
    }
    if (!spoken) {
        spoken = word;
        spoken_length = length;
    }
    if (selected_variant && info) ++info->variant_count;
    {
        nanotts_result_t r = ko_append_hangul(impl, spoken, spoken_length,
                                              boundary, token_index, info);
        if (r != NANOTTS_OK) return r;
    }
    for (i = before; i < impl->syllable_count; ++i)
        impl->syllables[i].morph_flags |= morph_flags;
    return NANOTTS_OK;
}

static nanotts_result_t append_symbol(nanotts_impl_t *impl, const char *s,
    size_t length, uint8_t boundary, uint8_t token_index,
    nanotts_parse_info_t *info)
{
    const char *spoken = NULL;
    if (length == 1u && s[0] == '%') spoken = "퍼센트";
    else if (length == 3u && memcmp(s, "\xe2\x84\x83", 3u) == 0) spoken = "도";
    else if (length == 3u && memcmp(s, "\xe2\x82\xa9", 3u) == 0) spoken = "원";
    else if (length == 3u && memcmp(s, "\xef\xbc\x85", 3u) == 0) spoken = "퍼센트";
    if (!spoken) return NANOTTS_ERR_UNSUPPORTED_TEXT;
    if (info) ++info->normalization_count;
    return ko_append_hangul(impl, spoken, strlen(spoken), boundary,
                            token_index, info);
}

static void mark_token_syllables(nanotts_impl_t *impl, size_t token_index,
                                 size_t start, size_t count)
{
    size_t i;
    nanotts_ko_token_t *token = &impl->tokens[token_index];
    token->syllable_start = (uint16_t)start;
    token->syllable_count = (uint16_t)count;
    if (!count) return;
    for (i = start; i < start + count; ++i) {
        impl->syllables[i].token_index = (uint8_t)token_index;
        if (i == start) impl->syllables[i].flags |= KO_SYLLABLE_WORD_INITIAL;
        if (i + 1u == start + count) impl->syllables[i].flags |= KO_SYLLABLE_WORD_FINAL;
    }
}

nanotts_result_t ko_normalize_tokens(nanotts_impl_t *impl, const char *text,
    size_t length, const nanotts_text_options_t *options,
    nanotts_parse_info_t *info)
{
    size_t i;
    (void)length;
    impl->syllable_count = 0u;
    for (i = 0u; i < impl->token_count; ++i) {
        nanotts_ko_token_t *token = &impl->tokens[i];
        const char *s = text + token->byte_start;
        size_t n = token->byte_length;
        size_t start = impl->syllable_count;
        nanotts_result_t r;
        if (token->type == KO_TOKEN_HANGUL) {
            if (ko_is_question_word(s, n)) token->flags |= KO_TOKEN_QUESTION_WORD;
            r = append_word(impl, s, n, token->boundary_before, (uint8_t)i,
                            options, info);
        } else if (token->type == KO_TOKEN_NUMBER) {
            const char *next_word = NULL;
            size_t next_length = 0u;
            if (i + 1u < impl->token_count &&
                impl->tokens[i + 1u].type == KO_TOKEN_HANGUL) {
                next_word = text + impl->tokens[i + 1u].byte_start;
                next_length = impl->tokens[i + 1u].byte_length;
                if (native_counter(next_word, next_length)) {
                    impl->tokens[i + 1u].flags |= KO_TOKEN_COUNTER;
                    token->flags |= KO_TOKEN_KEEP_WITH_NEXT;
                }
            }
            r = append_number_token(impl, s, n, next_word, next_length,
                                    token->boundary_before, (uint8_t)i,
                                    options, info);
            token->flags |= KO_TOKEN_NORMALIZED;
        } else if (token->type == KO_TOKEN_LATIN) {
            r = append_latin(impl, s, n, token->boundary_before,
                             (uint8_t)i, options, info);
            token->flags |= KO_TOKEN_NORMALIZED;
        } else {
            r = append_symbol(impl, s, n, token->boundary_before,
                              (uint8_t)i, info);
            token->flags |= KO_TOKEN_NORMALIZED;
        }
        if (r != NANOTTS_OK) return r;
        mark_token_syllables(impl, i, start, impl->syllable_count - start);
    }
    return impl->syllable_count ? NANOTTS_OK : NANOTTS_ERR_EMPTY_INPUT;
}
