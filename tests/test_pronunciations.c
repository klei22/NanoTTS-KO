/* SPDX-License-Identifier: MIT */
#include "nanotts/nanotts.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef NANOTTS_TEST_PRONUNCIATIONS_PATH
#define NANOTTS_TEST_PRONUNCIATIONS_PATH "tests/data/ko_pronunciations.tsv"
#endif
#ifndef NANOTTS_ENABLE_BUILTIN_LEXICON
#define NANOTTS_ENABLE_BUILTIN_LEXICON 1
#endif

static size_t content_phones(const nanotts_t *tts, nanotts_phone_t *out, size_t cap)
{
    const nanotts_event_t *events = nanotts_events(tts);
    size_t i, n = 0u;
    for (i = 0u; i < nanotts_event_count(tts); ++i) {
        nanotts_phone_t p = (nanotts_phone_t)events[i].phone;
        if (p == NANOTTS_PH_SILENCE || p == NANOTTS_PH_SHORT_PAUSE ||
            p == NANOTTS_PH_AP_PAUSE || p == NANOTTS_PH_PHRASE_PAUSE)
            continue;
        assert(n < cap);
        out[n++] = p;
    }
    return n;
}

static void strip_newline(char *s)
{
    size_t n = strlen(s);
    while (n && (s[n - 1u] == '\n' || s[n - 1u] == '\r')) s[--n] = '\0';
}

int main(void)
{
    FILE *f = fopen(NANOTTS_TEST_PRONUNCIATIONS_PATH, "rb");
    char line[512];
    unsigned tested = 0u, skipped = 0u;
    nanotts_t written_tts, spoken_tts;
    assert(f != NULL);
    assert(nanotts_init(&written_tts, 16000u) == NANOTTS_OK);
    assert(nanotts_init(&spoken_tts, 16000u) == NANOTTS_OK);

    while (fgets(line, (int)sizeof(line), f)) {
        char *written, *spoken, *kind, *tab;
        nanotts_phone_t a[NANOTTS_MAX_EVENTS], b[NANOTTS_MAX_EVENTS];
        size_t na, nb, i;
        nanotts_parse_info_t info;
        strip_newline(line);
        if (!line[0] || line[0] == '#') continue;
        written = line;
        tab = strchr(written, '\t'); assert(tab != NULL); *tab = '\0';
        spoken = tab + 1;
        tab = strchr(spoken, '\t'); assert(tab != NULL); *tab = '\0';
        kind = tab + 1;
#if !NANOTTS_ENABLE_BUILTIN_LEXICON
        if (strcmp(kind, "lex") == 0) { ++skipped; continue; }
#else
        (void)kind;
#endif
        assert(nanotts_parse_text(&written_tts, written, NANOTTS_NPOS,
            NANOTTS_OPT_LINK_EOJEOL, &info) == NANOTTS_OK);
        assert(nanotts_parse_text(&spoken_tts, spoken, NANOTTS_NPOS,
            NANOTTS_OPT_LINK_EOJEOL | NANOTTS_OPT_NO_BUILTIN_LEXICON,
            &info) == NANOTTS_OK);
        na = content_phones(&written_tts, a, NANOTTS_MAX_EVENTS);
        nb = content_phones(&spoken_tts, b, NANOTTS_MAX_EVENTS);
        if (na != nb) {
            fprintf(stderr, "%s -> %s: phone count %lu != %lu\n",
                written, spoken, (unsigned long)na, (unsigned long)nb);
        }
        assert(na == nb);
        for (i = 0u; i < na; ++i) {
            if (a[i] != b[i]) {
                fprintf(stderr, "%s -> %s phone %lu: %s != %s\n",
                    written, spoken, (unsigned long)i,
                    nanotts_phone_name(a[i]), nanotts_phone_name(b[i]));
            }
            assert(a[i] == b[i]);
        }
        ++tested;
    }
    fclose(f);
    printf("Korean pronunciation pairs passed: %u; skipped: %u\n", tested, skipped);
    return tested ? 0 : EXIT_FAILURE;
}
