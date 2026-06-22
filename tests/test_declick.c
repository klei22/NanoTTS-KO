/* SPDX-License-Identifier: MIT */
#include "nanotts/nanotts.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#define SAMPLE_CAPACITY 120000u

static int16_t samples[SAMPLE_CAPACITY];
static size_t sample_count;

static int collect(void *user, const int16_t *input, size_t count)
{
    size_t i;
    (void)user;
    if (sample_count + count > SAMPLE_CAPACITY) return 1;
    for (i = 0u; i < count; ++i) samples[sample_count + i] = input[i];
    sample_count += count;
    return 0;
}

static uint32_t magnitude(int16_t x)
{
    return x < 0 ? (uint32_t)(-(int32_t)x) : (uint32_t)x;
}

int main(void)
{
    nanotts_t tts;
    nanotts_options_t options;
    size_t i = 0u;
    size_t long_silences = 0u;
    uint32_t worst_edge = 0u;

    assert(nanotts_init(&tts, 16000u) == NANOTTS_OK);
    nanotts_default_options(&options);
    sample_count = 0u;
    assert(nanotts_speak_text(
        &tts,
        "안녕하세요. 한국어 음성 합성기입니다.",
        NANOTTS_NPOS,
        &options,
        collect,
        NULL,
        NULL) == NANOTTS_OK);

    while (i < sample_count) {
        size_t start, end;
        if (samples[i] != 0) {
            ++i;
            continue;
        }
        start = i;
        while (i < sample_count && samples[i] == 0) ++i;
        end = i;
        if (end - start >= 80u) { /* At least 5 ms at 16 kHz. */
            uint32_t edge;
            ++long_silences;
            if (start > 0u) {
                edge = magnitude(samples[start - 1u]);
                if (edge > worst_edge) worst_edge = edge;
            }
            if (end < sample_count) {
                edge = magnitude(samples[end]);
                if (edge > worst_edge) worst_edge = edge;
            }
        }
    }

    assert(long_silences >= 3u);
    /* A hard cut in 1.1 could exceed 8,000 counts.  The raised-cosine edge
     * and de-zippering keep every long-silence transition below -30 dBFS. */
    assert(worst_edge < 1024u);
    return 0;
}
