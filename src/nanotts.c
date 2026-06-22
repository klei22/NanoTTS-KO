/* SPDX-License-Identifier: MIT */
#include "nanotts_internal.h"
#include <string.h>

void nanotts_default_options(nanotts_options_t *options)
{
    if (!options) return;
    memset(options, 0, sizeof(*options));
    options->rate_q8 = 256u;
    options->volume = 220u;
    options->final_tone = (uint8_t)NANOTTS_FINAL_AUTO;
    options->flags = NANOTTS_OPT_LINK_EOJEOL;
}

nanotts_result_t nanotts_init(nanotts_t *tts, uint32_t sample_rate)
{
    nanotts_impl_t *impl;
    if (!tts) return NANOTTS_ERR_ARGUMENT;
    if (sample_rate < NANOTTS_MIN_SAMPLE_RATE ||
        sample_rate > NANOTTS_MAX_SAMPLE_RATE) return NANOTTS_ERR_SAMPLE_RATE;
    memset(tts->bytes, 0, NANOTTS_CONTEXT_BYTES);
    impl = nanotts_impl(tts);
    impl->magic = NANOTTS_MAGIC;
    impl->sample_rate = sample_rate;
    impl->rng = 0x6d2b79f5u;
    return NANOTTS_OK;
}

void nanotts_reset(nanotts_t *tts)
{
    nanotts_impl_t *impl;
    uint32_t rate;
    nanotts_lexicon_fn lex;
    void *user;
    if (!tts) return;
    impl = nanotts_impl(tts);
    if (impl->magic != NANOTTS_MAGIC) return;
    rate = impl->sample_rate;
    lex = impl->lexicon;
    user = impl->lexicon_user;
    memset(tts->bytes, 0, NANOTTS_CONTEXT_BYTES);
    impl = nanotts_impl(tts);
    impl->magic = NANOTTS_MAGIC;
    impl->sample_rate = rate;
    impl->rng = 0x6d2b79f5u;
    impl->lexicon = lex;
    impl->lexicon_user = user;
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

bool nanotts_push_event(
    nanotts_impl_t *impl, nanotts_phone_t phone, uint8_t flags,
    uint8_t duration_percent, int8_t pitch_q4)
{
    nanotts_event_t *event;
    if (!impl || (unsigned)phone >= (unsigned)NANOTTS_PH_COUNT ||
        impl->event_count >= NANOTTS_MAX_EVENTS) return false;
    event = &impl->events[impl->event_count++];
    event->phone = (uint8_t)phone;
    event->flags = flags;
    event->duration_percent = duration_percent ? duration_percent : 100u;
    event->pitch_semitones_q4 = pitch_q4;
    return true;
}

nanotts_result_t nanotts_parse_text(
    nanotts_t *tts, const char *text, size_t length,
    uint8_t parse_flags, nanotts_parse_info_t *info)
{
    nanotts_impl_t *impl;
    nanotts_result_t result;
    if (info) { memset(info, 0, sizeof(*info)); info->error_byte = NANOTTS_NPOS; }
    if (!tts || !text) return NANOTTS_ERR_ARGUMENT;
    impl = nanotts_impl(tts);
    if (impl->magic != NANOTTS_MAGIC) return NANOTTS_ERR_ARGUMENT;
    if (length == NANOTTS_NPOS) length = strlen(text);
    impl->event_count = 0u;
    impl->syllable_count = 0u;
    result = nanotts_ko_parse_text_impl(impl, text, length, parse_flags, info);
    if (info) {
        info->event_count = impl->event_count;
        info->syllable_count = impl->syllable_count;
    }
    return result;
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
    for (i = 0u; i < count; ++i)
        if ((unsigned)events[i].phone >= (unsigned)NANOTTS_PH_COUNT)
            return NANOTTS_ERR_ARGUMENT;
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
    if (!options) { nanotts_default_options(&defaults); options = &defaults; }
    return nanotts_synth_render(impl, options, write, user);
}

nanotts_result_t nanotts_speak_text(
    nanotts_t *tts, const char *text, size_t length,
    const nanotts_options_t *options, nanotts_write_fn write,
    void *user, nanotts_parse_info_t *info)
{
    nanotts_options_t defaults;
    nanotts_result_t result;
    if (!options) { nanotts_default_options(&defaults); options = &defaults; }
    result = nanotts_parse_text(tts, text, length, options->flags, info);
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
const nanotts_event_t *nanotts_events(const nanotts_t *tts)
{
    const nanotts_impl_t *impl;
    if (!tts) return NULL;
    impl = nanotts_impl_const(tts);
    return impl->magic == NANOTTS_MAGIC ? impl->events : NULL;
}

const char *nanotts_strerror(nanotts_result_t result)
{
    switch (result) {
    case NANOTTS_OK: return "success";
    case NANOTTS_ERR_ARGUMENT: return "invalid argument or uninitialized context";
    case NANOTTS_ERR_SAMPLE_RATE: return "unsupported sample rate";
    case NANOTTS_ERR_UTF8: return "invalid UTF-8 input";
    case NANOTTS_ERR_UNSUPPORTED_TEXT: return "unsupported input character";
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
const char *nanotts_version_string(void) { return "1.2.0-ko"; }
