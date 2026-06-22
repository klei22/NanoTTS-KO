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
#define NANOTTS_MAGIC 0x4b545453u

enum {
    NANOTTS_BOUNDARY_NONE = 0,
    NANOTTS_BOUNDARY_SPACE,
    NANOTTS_BOUNDARY_COMMA,
    NANOTTS_BOUNDARY_PHRASE
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
    /* Transposed direct-form II state.  This representation is compact and
     * behaves more gracefully when coefficients are smoothly modulated. */
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
} nanotts_ko_syllable_t;

typedef struct nanotts_impl {
    uint32_t magic;
    uint32_t sample_rate;
    uint32_t rng;
    nanotts_lexicon_fn lexicon;
    void *lexicon_user;

    size_t event_count;
    nanotts_event_t events[NANOTTS_MAX_EVENTS];
    size_t syllable_count;
    nanotts_ko_syllable_t syllables[NANOTTS_MAX_SYLLABLES];

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

const nanotts_phone_def_t *nanotts_phone_def(nanotts_phone_t phone);
bool nanotts_phone_is_vowel(nanotts_phone_t phone);
bool nanotts_phone_is_pause(nanotts_phone_t phone);
bool nanotts_phone_is_sonorant(nanotts_phone_t phone);
bool nanotts_phone_is_high_onset(nanotts_phone_t phone);

nanotts_result_t nanotts_ko_parse_text_impl(
    nanotts_impl_t *impl, const char *text, size_t length,
    uint8_t parse_flags, nanotts_parse_info_t *info);
nanotts_result_t nanotts_synth_render(
    nanotts_impl_t *impl, const nanotts_options_t *options,
    nanotts_write_fn write, void *user);
bool nanotts_push_event(
    nanotts_impl_t *impl, nanotts_phone_t phone, uint8_t flags,
    uint8_t duration_percent, int8_t pitch_q4);

#endif
