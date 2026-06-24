/* SPDX-License-Identifier: MIT */
#include "nanotts/nanotts.h"
#include <stddef.h>
#include <string.h>

static int lookup(
    void *user,
    nanotts_ko_style_t style,
    const char *word,
    size_t length,
    const char **spoken,
    size_t *spoken_length,
    uint16_t *morph_flags)
{
    (void)user;
    (void)style;
    if (length == strlen("서울역") && memcmp(word, "서울역", length) == 0) {
        *spoken = "서울력";
        *spoken_length = NANOTTS_NPOS;
        *morph_flags = 0u;
        return 1;
    }
    return 0;
}

static int discard(void *user, const int16_t *samples, size_t count)
{
    (void)user;
    (void)samples;
    (void)count;
    return 0;
}

int main(void)
{
    nanotts_t tts;
    nanotts_options_t options;
    if (nanotts_init(&tts, 16000u) != NANOTTS_OK) return 1;
    nanotts_set_lexicon_ex(&tts, lookup, NULL);
    nanotts_default_options(&options);
    return nanotts_speak_text(
        &tts, "서울역입니다.", NANOTTS_NPOS,
        &options, discard, NULL, NULL) != NANOTTS_OK;
}
