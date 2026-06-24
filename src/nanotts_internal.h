/* SPDX-License-Identifier: MIT */
#ifndef NANOTTS_INTERNAL_H
#define NANOTTS_INTERNAL_H

#include "nanotts/nanotts.h"
#include <stdbool.h>
#include <string.h>

#ifndef NANOTTS_AUDIO_BLOCK
#define NANOTTS_AUDIO_BLOCK 128u
#endif
#ifndef NANOTTS_USE_LIBM
#define NANOTTS_USE_LIBM 1
#endif
#ifndef NANOTTS_ENABLE_BUILTIN_LEXICON
#define NANOTTS_ENABLE_BUILTIN_LEXICON 1
#endif
#ifndef NANOTTS_ENABLE_MORPHOLOGY
#define NANOTTS_ENABLE_MORPHOLOGY 1
#endif
#ifndef NANOTTS_ENABLE_NORMALIZATION
#define NANOTTS_ENABLE_NORMALIZATION 1
#endif
#ifndef NANOTTS_CONTROL_MS
#define NANOTTS_CONTROL_MS 1u
#endif
#ifndef NANOTTS_COEFF_STRIDE
#define NANOTTS_COEFF_STRIDE 2u
#endif
#if NANOTTS_CONTROL_MS < 1 || NANOTTS_CONTROL_MS > 4
#error "NANOTTS_CONTROL_MS must be between 1 and 4"
#endif
#if NANOTTS_COEFF_STRIDE < 1 || NANOTTS_COEFF_STRIDE > 4
#error "NANOTTS_COEFF_STRIDE must be between 1 and 4"
#endif
#define NANOTTS_PI_F 3.14159265358979323846f
#define NANOTTS_MIN_SAMPLE_RATE 8000u
#define NANOTTS_MAX_SAMPLE_RATE 24000u
#define NANOTTS_MAGIC 0x4b545432u

#define KO_S_BASE 0xAC00u
#define KO_L_BASE 0x1100u
#define KO_V_BASE 0x1161u
#define KO_T_BASE 0x11A7u
#define KO_L_COUNT 19u
#define KO_V_COUNT 21u
#define KO_T_COUNT 28u
#define KO_N_COUNT (KO_V_COUNT * KO_T_COUNT)
#define KO_S_COUNT (KO_L_COUNT * KO_N_COUNT)
#define KO_L_LATERAL 19u
#define KO_BOUNDARY_STRENGTH_MASK 0x03u
#define KO_BOUNDARY_QUESTION 0x80u

enum {
    NANOTTS_BOUNDARY_NONE = 0,
    NANOTTS_BOUNDARY_SPACE,
    NANOTTS_BOUNDARY_COMMA,
    NANOTTS_BOUNDARY_PHRASE
};

enum {
    KO_CODA_NONE = 0,
    KO_CODA_K,
    KO_CODA_N,
    KO_CODA_T,
    KO_CODA_L,
    KO_CODA_M,
    KO_CODA_P,
    KO_CODA_NG
};

typedef enum ko_token_type {
    KO_TOKEN_HANGUL = 0,
    KO_TOKEN_NUMBER,
    KO_TOKEN_LATIN,
    KO_TOKEN_SYMBOL
} ko_token_type_t;

enum {
    KO_TOKEN_OBJECT_CONTEXT = 1u << 0,
    KO_TOKEN_QUESTION_WORD  = 1u << 1,
    KO_TOKEN_KEEP_WITH_NEXT = 1u << 2,
    KO_TOKEN_STRONG_BREAK   = 1u << 3,
    KO_TOKEN_NORMALIZED     = 1u << 4,
    KO_TOKEN_COUNTER        = 1u << 5,
    KO_TOKEN_VERBAL         = 1u << 6,
    KO_TOKEN_NOUN           = 1u << 7,
    KO_TOKEN_QUESTION_END   = 1u << 8,
    KO_TOKEN_COMMAND_END    = 1u << 9
};

enum {
    KO_SYLLABLE_WORD_INITIAL = 1u << 0,
    KO_SYLLABLE_WORD_FINAL   = 1u << 1,
    KO_SYLLABLE_MORPH_INITIAL = 1u << 2,
    KO_SYLLABLE_MORPH_FINAL   = 1u << 3,
    KO_SYLLABLE_LONG          = 1u << 4,
    KO_SYLLABLE_FOCUS         = 1u << 5,
    KO_SYLLABLE_KEEP_NEXT     = 1u << 6,
    KO_SYLLABLE_AP_BREAK      = 1u << 7
};

enum {
    KO_UTTERANCE_QUESTION = 1u << 0,
    KO_UTTERANCE_WH_QUESTION = 1u << 1,
    KO_UTTERANCE_COMMAND = 1u << 2,
    KO_UTTERANCE_CONTINUATION = 1u << 3
};

typedef enum nanotts_phone_kind {
    NANOTTS_KIND_PAUSE = 0,
    NANOTTS_KIND_VOWEL,
    NANOTTS_KIND_GLIDE,
    NANOTTS_KIND_STOP_LAX,
    NANOTTS_KIND_STOP_TENSE,
    NANOTTS_KIND_STOP_ASP,
    NANOTTS_KIND_AFFRICATE_LAX,
    NANOTTS_KIND_AFFRICATE_TENSE,
    NANOTTS_KIND_AFFRICATE_ASP,
    NANOTTS_KIND_FRICATIVE_LAX,
    NANOTTS_KIND_FRICATIVE_TENSE,
    NANOTTS_KIND_FRICATIVE_H,
    NANOTTS_KIND_NASAL,
    NANOTTS_KIND_LATERAL,
    NANOTTS_KIND_TAP,
    NANOTTS_KIND_CODA_STOP
} nanotts_phone_kind_t;

typedef struct nanotts_phone_def {
    uint8_t kind;
    uint8_t duration_ms;
    uint8_t voiced_q8;
    uint8_t noise_q8;
    uint8_t aspiration_q8;
    uint8_t amp_q8;
    uint8_t open_q8;
    uint8_t tilt_q8;
    int8_t onset_pitch_q4;
    uint8_t reserved;
    uint16_t f1, f2, f3, f4;
    uint16_t e1, e2, e3, e4;
    uint16_t noise_center;
    uint16_t noise_bandwidth;
} nanotts_phone_def_t;

typedef struct nanotts_biquad {
    float b0, b1, b2, a1, a2;
    float z1, z2;
} nanotts_biquad_t;

typedef struct nanotts_biquad_coeffs {
    float b0, b1, b2, a1, a2;
} nanotts_biquad_coeffs_t;

typedef struct nanotts_filter_targets {
    nanotts_biquad_coeffs_t voiced[4];
    nanotts_biquad_coeffs_t voiced_detail[4];
    nanotts_biquad_coeffs_t noise[3];
    nanotts_biquad_coeffs_t nasal[2];
    nanotts_biquad_coeffs_t nasal_zero[2];
} nanotts_filter_targets_t;

typedef struct nanotts_params {
    float f[4];
    float bw[4];
    float detail_gain[4];
    float voiced, noise, aspiration, amplitude;
    float noise_center, noise_bw, nasal;
    float nasal_pole[2], nasal_bw[2];
    float nasal_zero[2], nasal_zero_bw[2];
    float source_open, source_return, source_tilt;
} nanotts_params_t;

typedef struct nanotts_ko_syllable {
    uint8_t onset;
    uint8_t vowel;
    uint8_t coda;
    uint8_t boundary_before;
    uint8_t lexical_onset;
    uint8_t lexical_coda;
    uint8_t token_index;
    uint8_t morpheme_index;
    uint16_t morph_flags;
    uint8_t pos;
    uint8_t flags;
} nanotts_ko_syllable_t;

typedef struct nanotts_ko_token {
    uint32_t byte_start;
    uint16_t byte_length;
    uint16_t syllable_start;
    uint16_t syllable_count;
    uint16_t morpheme_start;
    uint8_t morpheme_count;
    uint8_t boundary_before;
    uint8_t type;
    uint8_t reserved;
    uint16_t flags;
} nanotts_ko_token_t;

typedef struct nanotts_impl {
    uint32_t magic;
    uint32_t sample_rate;
    uint32_t rng;
    nanotts_lexicon_fn lexicon;
    nanotts_lexicon_ex_fn lexicon_ex;
    void *lexicon_user;
    void *lexicon_ex_user;

    uint8_t current_style;
    uint8_t utterance_flags;
    uint8_t trailing_boundary;
    uint8_t reserved0;

    size_t event_count;
    nanotts_event_t events[NANOTTS_MAX_EVENTS];
    size_t syllable_count;
    nanotts_ko_syllable_t syllables[NANOTTS_MAX_SYLLABLES];
    size_t token_count;
    nanotts_ko_token_t tokens[NANOTTS_MAX_TOKENS];
    size_t morpheme_count;
    nanotts_morpheme_t morphemes[NANOTTS_MAX_MORPHEMES];

    float phase;
    float previous_glottal_flow;
    float source_tilt_1, source_tilt_2;
    float previous_noise;
    float dc_prev_x, dc_prev_y;
    nanotts_biquad_t voiced_filters[4];
    nanotts_biquad_t voiced_detail_filters[4];
    nanotts_biquad_t noise_filters[3];
    nanotts_biquad_t nasal_filters[2];
    nanotts_biquad_t nasal_zero_filters[2];
    float aspiration_lowpass;
    float radiation_previous;
    float cycle_jitter;
    float cycle_shimmer;
    int16_t audio_block[NANOTTS_AUDIO_BLOCK];
    size_t audio_count;
} nanotts_impl_t;

typedef char nanotts_context_size_check[
    (sizeof(nanotts_impl_t) <= NANOTTS_CONTEXT_BYTES) ? 1 : -1];

static inline nanotts_impl_t *nanotts_impl(nanotts_t *tts)
{ return (nanotts_impl_t *)(void *)tts->bytes; }
static inline const nanotts_impl_t *nanotts_impl_const(const nanotts_t *tts)
{ return (const nanotts_impl_t *)(const void *)tts->bytes; }
static inline float nanotts_clampf(float x, float lo, float hi)
{ return x < lo ? lo : (x > hi ? hi : x); }
static inline float nanotts_lerp(float a, float b, float t)
{ return a + (b - a) * t; }
static inline float nanotts_smoothstep(float t)
{
    t = nanotts_clampf(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
static inline uint8_t ko_boundary_strength(uint8_t b)
{ return (uint8_t)(b & KO_BOUNDARY_STRENGTH_MASK); }

const nanotts_phone_def_t *nanotts_phone_def(nanotts_phone_t phone);
bool nanotts_phone_is_vowel(nanotts_phone_t phone);
bool nanotts_phone_is_pause(nanotts_phone_t phone);
bool nanotts_phone_is_sonorant(nanotts_phone_t phone);
bool nanotts_phone_is_high_onset(nanotts_phone_t phone);

bool ko_utf8_decode(const char *text, size_t length, size_t *offset, uint32_t *cp);
bool ko_is_hangul(uint32_t cp);
bool ko_append_syllable(nanotts_impl_t *impl, uint8_t l, uint8_t v,
                        uint8_t t, uint8_t boundary, uint8_t token_index);
nanotts_result_t ko_append_hangul(nanotts_impl_t *impl, const char *text,
    size_t length, uint8_t boundary, uint8_t token_index,
    nanotts_parse_info_t *info);
size_t ko_hangul_syllable_count(const char *text, size_t length);
bool ko_word_equal(const char *word, size_t length, const char *literal);
bool ko_word_ends_with(const char *word, size_t length, const char *suffix,
                       size_t *prefix_length);

const char *ko_builtin_pronunciation(const char *word, size_t length,
    nanotts_ko_style_t style, size_t *out_length, uint16_t *morph_flags,
    bool *variant_selected);
bool ko_lexical_long_vowel(const char *word, size_t length, size_t *syllable_index);
bool ko_is_known_verb_stem(const char *word, size_t length);
bool ko_is_known_verb_syllables(const nanotts_ko_syllable_t *syllables,
                                size_t count, bool clear_final_coda);
bool ko_is_question_word(const char *word, size_t length);

nanotts_result_t ko_scan_tokens(nanotts_impl_t *impl, const char *text,
    size_t length, nanotts_parse_info_t *info);
nanotts_result_t ko_normalize_tokens(nanotts_impl_t *impl, const char *text,
    size_t length, const nanotts_text_options_t *options,
    nanotts_parse_info_t *info);
nanotts_result_t ko_analyze_morphology(nanotts_impl_t *impl, const char *text,
    size_t length, const nanotts_text_options_t *options,
    nanotts_parse_info_t *info);
void ko_apply_style_variants(nanotts_impl_t *impl, nanotts_ko_style_t style,
                             nanotts_parse_info_t *info);
void ko_apply_phonology(nanotts_impl_t *impl, uint16_t flags);
nanotts_result_t ko_emit_events(nanotts_impl_t *impl,
    const nanotts_text_options_t *options);

nanotts_result_t nanotts_ko_parse_text_impl(
    nanotts_impl_t *impl, const char *text, size_t length,
    const nanotts_text_options_t *options, nanotts_parse_info_t *info);
nanotts_result_t nanotts_synth_render(
    nanotts_impl_t *impl, const nanotts_options_t *options,
    nanotts_write_fn write, void *user);
bool nanotts_push_event_ex(
    nanotts_impl_t *impl, nanotts_phone_t phone, uint8_t flags,
    uint8_t duration_percent, int8_t pitch_start_q4, int8_t pitch_end_q4,
    uint8_t prominence_q8, uint8_t acoustic_flags);
bool nanotts_push_event(
    nanotts_impl_t *impl, nanotts_phone_t phone, uint8_t flags,
    uint8_t duration_percent, int8_t pitch_q4);

#endif
