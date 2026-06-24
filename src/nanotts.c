/* SPDX-License-Identifier: MIT */
#include "nanotts_internal.h"
#include <string.h>

void nanotts_default_text_options(nanotts_text_options_t *options)
{
    if (!options) return;
    memset(options, 0, sizeof(*options));
    options->flags = NANOTTS_OPT_LINK_EOJEOL | NANOTTS_OPT_SPELL_LATIN;
    options->style = (uint8_t)NANOTTS_KO_STYLE_MODERN_SEOUL;
}

void nanotts_default_options(nanotts_options_t *options)
{
    if (!options) return;
    memset(options, 0, sizeof(*options));
    options->rate_q8 = 256u;
    options->volume = 220u;
    options->final_tone = (uint8_t)NANOTTS_FINAL_AUTO;
    options->flags = NANOTTS_OPT_LINK_EOJEOL | NANOTTS_OPT_SPELL_LATIN;
    options->style = (uint8_t)NANOTTS_KO_STYLE_MODERN_SEOUL;
    options->expression_q8 = 128u;
}

nanotts_result_t nanotts_init(nanotts_t *tts, uint32_t sample_rate)
{
    nanotts_impl_t *impl;
    if (!tts) return NANOTTS_ERR_ARGUMENT;
    if (sample_rate < NANOTTS_MIN_SAMPLE_RATE ||
        sample_rate > NANOTTS_MAX_SAMPLE_RATE)
        return NANOTTS_ERR_SAMPLE_RATE;
    memset(tts->bytes, 0, NANOTTS_CONTEXT_BYTES);
    impl = nanotts_impl(tts);
    impl->magic = NANOTTS_MAGIC;
    impl->sample_rate = sample_rate;
    impl->rng = 0x6d2b79f5u;
    impl->current_style = (uint8_t)NANOTTS_KO_STYLE_MODERN_SEOUL;
    return NANOTTS_OK;
}

void nanotts_reset(nanotts_t *tts)
{
    nanotts_impl_t *impl;
    uint32_t rate;
    nanotts_lexicon_fn lexicon;
    nanotts_lexicon_ex_fn lexicon_ex;
    void *lexicon_user;
    void *lexicon_ex_user;
    if (!tts) return;
    impl = nanotts_impl(tts);
    if (impl->magic != NANOTTS_MAGIC) return;
    rate = impl->sample_rate;
    lexicon = impl->lexicon;
    lexicon_ex = impl->lexicon_ex;
    lexicon_user = impl->lexicon_user;
    lexicon_ex_user = impl->lexicon_ex_user;
    memset(tts->bytes, 0, NANOTTS_CONTEXT_BYTES);
    impl = nanotts_impl(tts);
    impl->magic = NANOTTS_MAGIC;
    impl->sample_rate = rate;
    impl->rng = 0x6d2b79f5u;
    impl->current_style = (uint8_t)NANOTTS_KO_STYLE_MODERN_SEOUL;
    impl->lexicon = lexicon;
    impl->lexicon_user = lexicon_user;
    impl->lexicon_ex = lexicon_ex;
    impl->lexicon_ex_user = lexicon_ex_user;
}

uint32_t nanotts_sample_rate(const nanotts_t *tts)
{
    const nanotts_impl_t *impl;
    if (!tts) return 0u;
    impl = nanotts_impl_const(tts);
    return impl->magic == NANOTTS_MAGIC ? impl->sample_rate : 0u;
}

void nanotts_set_lexicon(nanotts_t *tts, nanotts_lexicon_fn lookup, void *user)
{
    nanotts_impl_t *impl;
    if (!tts) return;
    impl = nanotts_impl(tts);
    if (impl->magic != NANOTTS_MAGIC) return;
    impl->lexicon = lookup;
    impl->lexicon_user = user;
}

void nanotts_set_lexicon_ex(nanotts_t *tts, nanotts_lexicon_ex_fn lookup,
                            void *user)
{
    nanotts_impl_t *impl;
    if (!tts) return;
    impl = nanotts_impl(tts);
    if (impl->magic != NANOTTS_MAGIC) return;
    impl->lexicon_ex = lookup;
    impl->lexicon_ex_user = user;
}

bool nanotts_push_event_ex(
    nanotts_impl_t *impl, nanotts_phone_t phone, uint8_t flags,
    uint8_t duration_percent, int8_t pitch_start_q4, int8_t pitch_end_q4,
    uint8_t prominence_q8, uint8_t acoustic_flags)
{
    nanotts_event_t *event;
    if (!impl || (unsigned)phone >= (unsigned)NANOTTS_PH_COUNT ||
        impl->event_count >= NANOTTS_MAX_EVENTS)
        return false;
    event = &impl->events[impl->event_count++];
    event->phone = (uint8_t)phone;
    event->flags = flags;
    event->duration_percent = duration_percent ? duration_percent : 100u;
    event->pitch_semitones_q4 = pitch_start_q4;
    event->pitch_end_semitones_q4 = pitch_end_q4;
    event->prominence_q8 = prominence_q8 ? prominence_q8 : 128u;
    event->acoustic_flags = acoustic_flags;
    event->reserved = 0u;
    return true;
}

bool nanotts_push_event(
    nanotts_impl_t *impl, nanotts_phone_t phone, uint8_t flags,
    uint8_t duration_percent, int8_t pitch_q4)
{
    return nanotts_push_event_ex(impl, phone, flags, duration_percent,
                                pitch_q4, pitch_q4, 128u, 0u);
}

nanotts_result_t nanotts_parse_text_ex(
    nanotts_t *tts, const char *text, size_t length,
    const nanotts_text_options_t *options, nanotts_parse_info_t *info)
{
    nanotts_impl_t *impl;
    nanotts_text_options_t defaults;
    nanotts_result_t result;
    if (info) {
        memset(info, 0, sizeof(*info));
        info->error_byte = NANOTTS_NPOS;
    }
    if (!tts || !text) return NANOTTS_ERR_ARGUMENT;
    impl = nanotts_impl(tts);
    if (impl->magic != NANOTTS_MAGIC) return NANOTTS_ERR_ARGUMENT;
    if (!options) {
        nanotts_default_text_options(&defaults);
        options = &defaults;
    }
    if (options->style >= (uint8_t)NANOTTS_KO_STYLE_COUNT)
        return NANOTTS_ERR_ARGUMENT;
    if (length == NANOTTS_NPOS) length = strlen(text);
    impl->event_count = 0u;
    impl->syllable_count = 0u;
    impl->token_count = 0u;
    impl->morpheme_count = 0u;
    impl->utterance_flags = 0u;
    impl->trailing_boundary = NANOTTS_BOUNDARY_NONE;
    result = nanotts_ko_parse_text_impl(impl, text, length, options, info);
    if (info) {
        info->event_count = impl->event_count;
        info->syllable_count = impl->syllable_count;
        info->token_count = impl->token_count;
        info->morpheme_count = impl->morpheme_count;
    }
    return result;
}

nanotts_result_t nanotts_parse_text(
    nanotts_t *tts, const char *text, size_t length,
    uint16_t parse_flags, nanotts_parse_info_t *info)
{
    nanotts_text_options_t options;
    nanotts_default_text_options(&options);
    options.flags = parse_flags;
    return nanotts_parse_text_ex(tts, text, length, &options, info);
}

nanotts_result_t nanotts_set_events(
    nanotts_t *tts, const nanotts_event_t *events, size_t count)
{
    nanotts_impl_t *impl;
    size_t i;
    if (!tts || (count && !events)) return NANOTTS_ERR_ARGUMENT;
    if (count > NANOTTS_MAX_EVENTS) return NANOTTS_ERR_TOO_MANY_EVENTS;
    impl = nanotts_impl(tts);
    if (impl->magic != NANOTTS_MAGIC) return NANOTTS_ERR_ARGUMENT;
    for (i = 0u; i < count; ++i) {
        if ((unsigned)events[i].phone >= (unsigned)NANOTTS_PH_COUNT)
            return NANOTTS_ERR_ARGUMENT;
    }
    if (count) memcpy(impl->events, events, count * sizeof(*events));
    impl->event_count = count;
    return NANOTTS_OK;
}

nanotts_result_t nanotts_render_events(
    nanotts_t *tts, const nanotts_options_t *options,
    nanotts_write_fn write, void *user)
{
    nanotts_impl_t *impl;
    nanotts_options_t defaults;
    if (!tts || !write) return NANOTTS_ERR_ARGUMENT;
    impl = nanotts_impl(tts);
    if (impl->magic != NANOTTS_MAGIC) return NANOTTS_ERR_ARGUMENT;
    if (!impl->event_count) return NANOTTS_ERR_EMPTY_INPUT;
    if (!options) {
        nanotts_default_options(&defaults);
        options = &defaults;
    }
    if (options->style >= (uint8_t)NANOTTS_KO_STYLE_COUNT)
        return NANOTTS_ERR_ARGUMENT;
    return nanotts_synth_render(impl, options, write, user);
}

nanotts_result_t nanotts_speak_text(
    nanotts_t *tts, const char *text, size_t length,
    const nanotts_options_t *options, nanotts_write_fn write,
    void *user, nanotts_parse_info_t *info)
{
    nanotts_options_t defaults;
    nanotts_text_options_t text_options;
    nanotts_result_t result;
    if (!options) {
        nanotts_default_options(&defaults);
        options = &defaults;
    }
    nanotts_default_text_options(&text_options);
    text_options.flags = options->flags;
    text_options.style = options->style;
    result = nanotts_parse_text_ex(tts, text, length, &text_options, info);
    if (result != NANOTTS_OK) return result;
    return nanotts_render_events(tts, options, write, user);
}

size_t nanotts_event_count(const nanotts_t *tts)
{
    const nanotts_impl_t *impl;
    if (!tts) return 0u;
    impl = nanotts_impl_const(tts);
    return impl->magic == NANOTTS_MAGIC ? impl->event_count : 0u;
}

size_t nanotts_event_capacity(void) { return NANOTTS_MAX_EVENTS; }
size_t nanotts_syllable_capacity(void) { return NANOTTS_MAX_SYLLABLES; }
size_t nanotts_context_bytes(void) { return NANOTTS_CONTEXT_BYTES; }

size_t nanotts_token_count(const nanotts_t *tts)
{
    const nanotts_impl_t *impl;
    if (!tts) return 0u;
    impl = nanotts_impl_const(tts);
    return impl->magic == NANOTTS_MAGIC ? impl->token_count : 0u;
}

size_t nanotts_morpheme_count(const nanotts_t *tts)
{
    const nanotts_impl_t *impl;
    if (!tts) return 0u;
    impl = nanotts_impl_const(tts);
    return impl->magic == NANOTTS_MAGIC ? impl->morpheme_count : 0u;
}

const nanotts_event_t *nanotts_events(const nanotts_t *tts)
{
    const nanotts_impl_t *impl;
    if (!tts) return NULL;
    impl = nanotts_impl_const(tts);
    return impl->magic == NANOTTS_MAGIC ? impl->events : NULL;
}

const nanotts_morpheme_t *nanotts_morphemes(const nanotts_t *tts)
{
    const nanotts_impl_t *impl;
    if (!tts) return NULL;
    impl = nanotts_impl_const(tts);
    return impl->magic == NANOTTS_MAGIC ? impl->morphemes : NULL;
}

const char *nanotts_ko_style_name(nanotts_ko_style_t style)
{
    switch (style) {
    case NANOTTS_KO_STYLE_PRESCRIPTIVE: return "prescriptive";
    case NANOTTS_KO_STYLE_MODERN_SEOUL: return "modern-seoul";
    case NANOTTS_KO_STYLE_CLEAR_DEVICE: return "clear-device";
    case NANOTTS_KO_STYLE_CONVERSATIONAL: return "conversational";
    case NANOTTS_KO_STYLE_LEARNER: return "learner";
    default: return "unknown";
    }
}

const char *nanotts_ko_pos_name(nanotts_ko_pos_t pos)
{
    switch (pos) {
    case NANOTTS_KO_POS_UNKNOWN: return "unknown";
    case NANOTTS_KO_POS_NOUN: return "noun";
    case NANOTTS_KO_POS_PROPER_NOUN: return "proper-noun";
    case NANOTTS_KO_POS_VERB_STEM: return "verb-stem";
    case NANOTTS_KO_POS_ADJECTIVE_STEM: return "adjective-stem";
    case NANOTTS_KO_POS_ENDING: return "ending";
    case NANOTTS_KO_POS_PARTICLE: return "particle";
    case NANOTTS_KO_POS_PREFIX: return "prefix";
    case NANOTTS_KO_POS_SUFFIX: return "suffix";
    case NANOTTS_KO_POS_COUNTER: return "counter";
    case NANOTTS_KO_POS_NUMBER: return "number";
    case NANOTTS_KO_POS_FOREIGN: return "foreign";
    default: return "unknown";
    }
}

const char *nanotts_strerror(nanotts_result_t result)
{
    switch (result) {
    case NANOTTS_OK: return "success";
    case NANOTTS_ERR_ARGUMENT: return "invalid argument or uninitialized context";
    case NANOTTS_ERR_SAMPLE_RATE: return "unsupported sample rate";
    case NANOTTS_ERR_UTF8: return "invalid UTF-8 input";
    case NANOTTS_ERR_UNSUPPORTED_TEXT: return "unsupported input character";
    case NANOTTS_ERR_TOO_MANY_TOKENS: return "too many text tokens";
    case NANOTTS_ERR_TOO_MANY_MORPHEMES: return "too many Korean morphemes";
    case NANOTTS_ERR_TOO_MANY_SYLLABLES: return "too many Hangul syllables";
    case NANOTTS_ERR_TOO_MANY_EVENTS: return "too many acoustic events";
    case NANOTTS_ERR_EMPTY_INPUT: return "input produced no speech";
    case NANOTTS_ERR_CALLBACK_ABORTED: return "audio callback aborted synthesis";
    case NANOTTS_ERR_OUTPUT_TOO_SMALL: return "output buffer is too small";
    case NANOTTS_ERR_FEATURE_DISABLED: return "feature disabled in this build";
    case NANOTTS_ERR_INTERNAL: return "internal error";
    default: return "unknown error";
    }
}

const char *nanotts_version_string(void) { return "2.0.0-ko"; }
