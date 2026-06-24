/* SPDX-License-Identifier: MIT */
#ifndef NANOTTS_NANOTTS_H
#define NANOTTS_NANOTTS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NANOTTS_VERSION_MAJOR 2
#define NANOTTS_VERSION_MINOR 0
#define NANOTTS_VERSION_PATCH 0

#ifndef NANOTTS_MAX_EVENTS
#define NANOTTS_MAX_EVENTS 640u
#endif
#ifndef NANOTTS_MAX_SYLLABLES
#define NANOTTS_MAX_SYLLABLES 192u
#endif
#ifndef NANOTTS_MAX_TOKENS
#define NANOTTS_MAX_TOKENS 96u
#endif
#ifndef NANOTTS_MAX_MORPHEMES
#define NANOTTS_MAX_MORPHEMES 192u
#endif
#ifndef NANOTTS_CONTEXT_BYTES
#define NANOTTS_CONTEXT_BYTES 16384u
#endif
#ifndef NANOTTS_NPOS
#define NANOTTS_NPOS ((size_t)-1)
#endif

typedef enum nanotts_result {
    NANOTTS_OK = 0,
    NANOTTS_ERR_ARGUMENT,
    NANOTTS_ERR_SAMPLE_RATE,
    NANOTTS_ERR_UTF8,
    NANOTTS_ERR_UNSUPPORTED_TEXT,
    NANOTTS_ERR_TOO_MANY_TOKENS,
    NANOTTS_ERR_TOO_MANY_MORPHEMES,
    NANOTTS_ERR_TOO_MANY_SYLLABLES,
    NANOTTS_ERR_TOO_MANY_EVENTS,
    NANOTTS_ERR_EMPTY_INPUT,
    NANOTTS_ERR_CALLBACK_ABORTED,
    NANOTTS_ERR_OUTPUT_TOO_SMALL,
    NANOTTS_ERR_FEATURE_DISABLED,
    NANOTTS_ERR_INTERNAL
} nanotts_result_t;

typedef enum nanotts_final_tone {
    NANOTTS_FINAL_FALL = 0,
    NANOTTS_FINAL_RISE,
    NANOTTS_FINAL_CONTINUE,
    NANOTTS_FINAL_LEVEL,
    NANOTTS_FINAL_AUTO
} nanotts_final_tone_t;

/* Pronunciation and prosody policies. */
typedef enum nanotts_ko_style {
    NANOTTS_KO_STYLE_PRESCRIPTIVE = 0,
    NANOTTS_KO_STYLE_MODERN_SEOUL,
    NANOTTS_KO_STYLE_CLEAR_DEVICE,
    NANOTTS_KO_STYLE_CONVERSATIONAL,
    NANOTTS_KO_STYLE_LEARNER,
    NANOTTS_KO_STYLE_COUNT
} nanotts_ko_style_t;

typedef enum nanotts_ko_pos {
    NANOTTS_KO_POS_UNKNOWN = 0,
    NANOTTS_KO_POS_NOUN,
    NANOTTS_KO_POS_PROPER_NOUN,
    NANOTTS_KO_POS_VERB_STEM,
    NANOTTS_KO_POS_ADJECTIVE_STEM,
    NANOTTS_KO_POS_ENDING,
    NANOTTS_KO_POS_PARTICLE,
    NANOTTS_KO_POS_PREFIX,
    NANOTTS_KO_POS_SUFFIX,
    NANOTTS_KO_POS_COUNTER,
    NANOTTS_KO_POS_NUMBER,
    NANOTTS_KO_POS_FOREIGN
} nanotts_ko_pos_t;

enum {
    NANOTTS_OPT_NO_AUTOPAUSE       = 1u << 0,
    NANOTTS_OPT_LINK_EOJEOL        = 1u << 1,
    NANOTTS_OPT_NO_BUILTIN_LEXICON = 1u << 2,
    NANOTTS_OPT_NO_MORPHOLOGY      = 1u << 3,
    NANOTTS_OPT_NO_NORMALIZATION   = 1u << 4,
    NANOTTS_OPT_SPELL_LATIN        = 1u << 5
};

enum {
    NANOTTS_EVENT_WORD_END       = 1u << 0,
    NANOTTS_EVENT_AP_START       = 1u << 1,
    NANOTTS_EVENT_AP_END         = 1u << 2,
    NANOTTS_EVENT_PHRASE_END     = 1u << 3,
    NANOTTS_EVENT_QUESTION       = 1u << 4,
    NANOTTS_EVENT_SYLLABLE_START = 1u << 5,
    NANOTTS_EVENT_LONG           = 1u << 6,
    NANOTTS_EVENT_CONTINUATION   = 1u << 7
};

enum {
    NANOTTS_ACOUSTIC_PALATALIZED    = 1u << 0,
    NANOTTS_ACOUSTIC_AP_INITIAL     = 1u << 1,
    NANOTTS_ACOUSTIC_AP_FINAL       = 1u << 2,
    NANOTTS_ACOUSTIC_WEAK_H         = 1u << 3,
    NANOTTS_ACOUSTIC_DENASALIZED    = 1u << 4,
    NANOTTS_ACOUSTIC_WH_QUESTION    = 1u << 5,
    NANOTTS_ACOUSTIC_COMMAND        = 1u << 6,
    NANOTTS_ACOUSTIC_FOCUSED        = 1u << 7
};

enum {
    NANOTTS_MORPH_OBJECT_PARTICLE = 1u << 0,
    NANOTTS_MORPH_TOPIC_PARTICLE  = 1u << 1,
    NANOTTS_MORPH_UI_PARTICLE     = 1u << 2,
    NANOTTS_MORPH_PASSIVE_CAUSATIVE = 1u << 3,
    NANOTTS_MORPH_NOMINALIZER_GI  = 1u << 4,
    NANOTTS_MORPH_ATTRIBUTIVE_L   = 1u << 5,
    NANOTTS_MORPH_KEEP_WITH_NEXT  = 1u << 6,
    NANOTTS_MORPH_STRONG_BREAK    = 1u << 7,
    NANOTTS_MORPH_COUNTER_NATIVE  = 1u << 8,
    NANOTTS_MORPH_COUNTER_SINO    = 1u << 9
};

/* Korean-only acoustic inventory. Coda stops are unreleased closures. */
typedef enum nanotts_phone {
    NANOTTS_PH_SILENCE = 0,
    NANOTTS_PH_SHORT_PAUSE,
    NANOTTS_PH_AP_PAUSE,
    NANOTTS_PH_PHRASE_PAUSE,

    NANOTTS_PH_A,
    NANOTTS_PH_EO,
    NANOTTS_PH_O,
    NANOTTS_PH_U,
    NANOTTS_PH_EU,
    NANOTTS_PH_I,
    NANOTTS_PH_AE,
    NANOTTS_PH_E,
    NANOTTS_PH_Y,
    NANOTTS_PH_W,

    NANOTTS_PH_K_LAX,
    NANOTTS_PH_K_TENSE,
    NANOTTS_PH_K_ASP,
    NANOTTS_PH_T_LAX,
    NANOTTS_PH_T_TENSE,
    NANOTTS_PH_T_ASP,
    NANOTTS_PH_P_LAX,
    NANOTTS_PH_P_TENSE,
    NANOTTS_PH_P_ASP,
    NANOTTS_PH_C_LAX,
    NANOTTS_PH_C_TENSE,
    NANOTTS_PH_C_ASP,
    NANOTTS_PH_S_LAX,
    NANOTTS_PH_S_TENSE,
    NANOTTS_PH_S_LAX_PAL,
    NANOTTS_PH_S_TENSE_PAL,
    NANOTTS_PH_H,

    NANOTTS_PH_M,
    NANOTTS_PH_N,
    NANOTTS_PH_NG,
    NANOTTS_PH_L,
    NANOTTS_PH_R,

    NANOTTS_PH_K_CODA,
    NANOTTS_PH_T_CODA,
    NANOTTS_PH_P_CODA,

    /* Moving high-central-to-front vowel used for word-initial ㅢ. */
    NANOTTS_PH_UI,

    NANOTTS_PH_COUNT
} nanotts_phone_t;

/* Eight-byte render event. Pitch is a continuous start/end trajectory. */
typedef struct nanotts_event {
    uint8_t phone;
    uint8_t flags;
    uint8_t duration_percent;
    int8_t pitch_semitones_q4;
    int8_t pitch_end_semitones_q4;
    uint8_t prominence_q8;
    uint8_t acoustic_flags;
    uint8_t reserved;
} nanotts_event_t;

typedef struct nanotts_text_options {
    uint16_t flags;
    uint8_t style;
    uint8_t reserved;
} nanotts_text_options_t;

typedef struct nanotts_options {
    uint16_t rate_q8;
    int16_t pitch_cents;
    uint16_t flags;
    uint8_t volume;
    uint8_t final_tone;
    uint8_t style;
    uint8_t expression_q8;
    uint8_t reserved[2];
} nanotts_options_t;

typedef struct nanotts_parse_info {
    size_t event_count;
    size_t syllable_count;
    size_t token_count;
    size_t morpheme_count;
    size_t error_byte;
    uint32_t error_codepoint;
    size_t lexicon_hits;
    size_t normalization_count;
    size_t variant_count;
} nanotts_parse_info_t;

typedef struct nanotts_morpheme {
    uint16_t syllable_start;
    uint16_t syllable_count;
    uint16_t flags;
    uint8_t token_index;
    uint8_t pos;
} nanotts_morpheme_t;

typedef int (*nanotts_write_fn)(void *user, const int16_t *samples, size_t count);

typedef int (*nanotts_lexicon_fn)(
    void *user,
    const char *word_utf8,
    size_t word_length,
    const char **pronunciation_utf8,
    size_t *pronunciation_length);

typedef int (*nanotts_lexicon_ex_fn)(
    void *user,
    nanotts_ko_style_t style,
    const char *word_utf8,
    size_t word_length,
    const char **pronunciation_utf8,
    size_t *pronunciation_length,
    uint16_t *morph_flags);

typedef union nanotts {
    uint64_t _align_u64;
    double _align_double;
    void *_align_pointer;
    unsigned char bytes[NANOTTS_CONTEXT_BYTES];
} nanotts_t;

void nanotts_default_text_options(nanotts_text_options_t *options);
void nanotts_default_options(nanotts_options_t *options);
nanotts_result_t nanotts_init(nanotts_t *tts, uint32_t sample_rate);
void nanotts_reset(nanotts_t *tts);
uint32_t nanotts_sample_rate(const nanotts_t *tts);
void nanotts_set_lexicon(nanotts_t *tts, nanotts_lexicon_fn lookup, void *user);
void nanotts_set_lexicon_ex(nanotts_t *tts, nanotts_lexicon_ex_fn lookup, void *user);

nanotts_result_t nanotts_parse_text_ex(
    nanotts_t *tts, const char *text_utf8, size_t length,
    const nanotts_text_options_t *options, nanotts_parse_info_t *info);
nanotts_result_t nanotts_parse_text(
    nanotts_t *tts, const char *text_utf8, size_t length,
    uint16_t parse_flags, nanotts_parse_info_t *info);
nanotts_result_t nanotts_set_events(
    nanotts_t *tts, const nanotts_event_t *events, size_t count);
nanotts_result_t nanotts_render_events(
    nanotts_t *tts, const nanotts_options_t *options,
    nanotts_write_fn write, void *user);
nanotts_result_t nanotts_speak_text(
    nanotts_t *tts, const char *text_utf8, size_t length,
    const nanotts_options_t *options, nanotts_write_fn write,
    void *user, nanotts_parse_info_t *info);

size_t nanotts_event_count(const nanotts_t *tts);
size_t nanotts_event_capacity(void);
size_t nanotts_syllable_capacity(void);
size_t nanotts_token_count(const nanotts_t *tts);
size_t nanotts_morpheme_count(const nanotts_t *tts);
size_t nanotts_context_bytes(void);
const nanotts_event_t *nanotts_events(const nanotts_t *tts);
const nanotts_morpheme_t *nanotts_morphemes(const nanotts_t *tts);
const char *nanotts_phone_name(nanotts_phone_t phone);
const char *nanotts_ko_style_name(nanotts_ko_style_t style);
const char *nanotts_ko_pos_name(nanotts_ko_pos_t pos);
const char *nanotts_strerror(nanotts_result_t result);
const char *nanotts_version_string(void);

#ifdef __cplusplus
}
#endif
#endif
