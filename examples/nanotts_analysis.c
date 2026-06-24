/* SPDX-License-Identifier: MIT */
#include "nanotts/nanotts.h"
#include <stdio.h>

int main(void)
{
    static nanotts_t tts;
    nanotts_text_options_t options;
    nanotts_parse_info_t info;
    const nanotts_morpheme_t *morphemes;
    size_t i;

    if (nanotts_init(&tts, 16000u) != NANOTTS_OK) return 1;
    nanotts_default_text_options(&options);
    options.style = (uint8_t)NANOTTS_KO_STYLE_CLEAR_DEVICE;

    if (nanotts_parse_text_ex(&tts, "신을 신고하고 12시 30분에 출발합니다.",
                             NANOTTS_NPOS, &options, &info) != NANOTTS_OK)
        return 1;

    printf("tokens=%lu morphemes=%lu syllables=%lu events=%lu normalized=%lu\n",
           (unsigned long)info.token_count,
           (unsigned long)info.morpheme_count,
           (unsigned long)info.syllable_count,
           (unsigned long)info.event_count,
           (unsigned long)info.normalization_count);

    morphemes = nanotts_morphemes(&tts);
    for (i = 0u; i < nanotts_morpheme_count(&tts); ++i) {
        printf("%lu: token=%u syllables=%u+%u pos=%s flags=0x%04x\n",
               (unsigned long)i,
               (unsigned)morphemes[i].token_index,
               (unsigned)morphemes[i].syllable_start,
               (unsigned)morphemes[i].syllable_count,
               nanotts_ko_pos_name((nanotts_ko_pos_t)morphemes[i].pos),
               (unsigned)morphemes[i].flags);
    }
    return 0;
}
