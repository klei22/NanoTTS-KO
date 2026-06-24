/* SPDX-License-Identifier: MIT */
#include "nanotts/nanotts.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint32_t rng = 0x13579bdfu;
static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

static size_t utf8(uint32_t cp, char *p)
{
    if (cp < 0x800u) {
        p[0] = (char)(0xc0u | (cp >> 6));
        p[1] = (char)(0x80u | (cp & 63u));
        return 2u;
    }
    p[0] = (char)(0xe0u | (cp >> 12));
    p[1] = (char)(0x80u | ((cp >> 6) & 63u));
    p[2] = (char)(0x80u | (cp & 63u));
    return 3u;
}

static int discard(void *u, const int16_t *s, size_t n)
{
    (void)u;
    (void)s;
    (void)n;
    return 0;
}

static void check_events(const nanotts_t *tts, const nanotts_parse_info_t *info)
{
    const nanotts_event_t *events = nanotts_events(tts);
    size_t i;
    assert(info->event_count > 0u);
    assert(info->event_count <= nanotts_event_capacity());
    for (i = 0u; i < info->event_count; ++i) {
        assert(events[i].phone < NANOTTS_PH_COUNT);
        assert(events[i].duration_percent > 0u);
        assert(events[i].prominence_q8 > 0u);
    }
}

static void append_ascii(char *text, size_t *n, const char *s)
{
    size_t m = strlen(s);
    assert(*n + m < 500u);
    memcpy(text + *n, s, m);
    *n += m;
}

int main(void)
{
    nanotts_t tts;
    nanotts_options_t render_options;
    unsigned c;
    assert(nanotts_init(&tts, 16000u) == NANOTTS_OK);
    nanotts_default_options(&render_options);

    /* Broad random Hangul coverage exercises decomposition, phonology and DSP. */
    for (c = 0u; c < 30000u; ++c) {
        char text[256];
        size_t n = 0u;
        unsigned syllables = 1u + rnd() % 18u, i;
        nanotts_parse_info_t info;
        nanotts_result_t r;
        for (i = 0u; i < syllables; ++i) {
            uint32_t cp = 0xac00u + rnd() % 11172u;
            n += utf8(cp, text + n);
            if (i + 1u < syllables && rnd() % 5u == 0u) text[n++] = ' ';
        }
        if (rnd() % 3u == 0u) text[n++] = (rnd() & 1u) ? '?' : '.';
        text[n] = '\0';
        r = nanotts_parse_text(&tts, text, n, render_options.flags, &info);
        assert(r == NANOTTS_OK);
        check_events(&tts, &info);
        if (c % 3000u == 0u)
            assert(nanotts_render_events(&tts, &render_options, discard, NULL) ==
                   NANOTTS_OK);
    }

#if NANOTTS_ENABLE_NORMALIZATION
    /* Structured mixed-text fuzzing covers the semantic normalizer, compact
     * morphology analyzer, style variants and clause-local punctuation. */
    {
        static const char *const numbers[] = {
            "0","007","12","1,000","-12","3.5","12:30",
            "2026-06-24","1/3","85%","010-1234-5678","1.2.3.4"
        };
        static const char *const hangul[] = {
            "시","분","명","개","신고합니다","안기다","먹을 것",
            "어디에 가요","감사합니다","배터리가 부족합니다"
        };
        static const char *const units[] = {
            "V","mA","kHz","kg","MB","km"
        };
        static const char *const latin[] = {"USB","CPU","PWM","AI"};
        for (c = 0u; c < 30000u; ++c) {
            char text[512];
            size_t n = 0u;
            unsigned tokens = 1u + rnd() % 8u, i;
            nanotts_text_options_t options;
            nanotts_parse_info_t info;
            nanotts_result_t r;
            nanotts_default_text_options(&options);
            options.style = (uint8_t)(rnd() % (unsigned)NANOTTS_KO_STYLE_COUNT);
            if (rnd() % 7u == 0u) options.flags |= NANOTTS_OPT_NO_MORPHOLOGY;
            if (rnd() % 11u == 0u) options.flags |= NANOTTS_OPT_NO_NORMALIZATION;
            for (i = 0u; i < tokens; ++i) {
                unsigned kind = rnd() % 4u;
                if (i) text[n++] = ' ';
                if (kind == 0u)
                    append_ascii(text, &n, numbers[rnd() %
                        (sizeof(numbers) / sizeof(numbers[0]))]);
                else if (kind == 1u)
                    append_ascii(text, &n, hangul[rnd() %
                        (sizeof(hangul) / sizeof(hangul[0]))]);
                else if (kind == 2u)
                    append_ascii(text, &n, latin[rnd() %
                        (sizeof(latin) / sizeof(latin[0]))]);
                else {
                    append_ascii(text, &n, numbers[rnd() % 4u]);
                    append_ascii(text, &n, units[rnd() %
                        (sizeof(units) / sizeof(units[0]))]);
                }
            }
            if (rnd() % 2u == 0u) text[n++] = (rnd() % 3u == 0u) ? '?' : '.';
            text[n] = '\0';
            r = nanotts_parse_text_ex(&tts, text, n, &options, &info);
            assert(r == NANOTTS_OK);
            check_events(&tts, &info);
            assert(info.token_count <= NANOTTS_MAX_TOKENS);
            assert(info.morpheme_count <= NANOTTS_MAX_MORPHEMES);
        }
    }
#endif
    return 0;
}
