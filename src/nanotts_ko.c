/* SPDX-License-Identifier: MIT */
#include "nanotts_internal.h"

nanotts_result_t nanotts_ko_parse_text_impl(
    nanotts_impl_t *impl, const char *text, size_t length,
    const nanotts_text_options_t *options, nanotts_parse_info_t *info)
{
    nanotts_result_t result;
    nanotts_text_options_t active;
    if (!impl || !text || !options) return NANOTTS_ERR_ARGUMENT;
    active = *options;
#if !NANOTTS_ENABLE_MORPHOLOGY
    active.flags |= NANOTTS_OPT_NO_MORPHOLOGY;
#endif
#if !NANOTTS_ENABLE_NORMALIZATION
    active.flags |= NANOTTS_OPT_NO_NORMALIZATION;
#endif
    options = &active;
    if (options->style >= (uint8_t)NANOTTS_KO_STYLE_COUNT)
        return NANOTTS_ERR_ARGUMENT;

    impl->current_style = options->style;
    result = ko_scan_tokens(impl, text, length, info);
    if (result != NANOTTS_OK) return result;
    result = ko_normalize_tokens(impl, text, length, options, info);
    if (result != NANOTTS_OK) return result;
    result = ko_analyze_morphology(impl, text, length, options, info);
    if (result != NANOTTS_OK) return result;
    ko_apply_style_variants(impl, (nanotts_ko_style_t)options->style, info);
    ko_apply_phonology(impl, options->flags);
    result = ko_emit_events(impl, options);

    if (info) {
        info->event_count = impl->event_count;
        info->syllable_count = impl->syllable_count;
        info->token_count = impl->token_count;
        info->morpheme_count = impl->morpheme_count;
    }
    return result;
}
