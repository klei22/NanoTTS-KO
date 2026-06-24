# Public API

Include:

```c
#include <nanotts/nanotts.h>
```

## Lifecycle

```c
nanotts_t tts;
nanotts_options_t options;

nanotts_result_t r = nanotts_init(&tts, 16000u);
nanotts_default_options(&options);
```

`nanotts_t` is opaque caller-owned storage. Do not copy or modify it while
rendering. `nanotts_reset()` clears analysis, events, and DSP state while
preserving the sample rate and both application lexicon callbacks.

Supported sample rates are 8,000 through 24,000 Hz. Output is mono signed
`int16_t` PCM at the exact initialized rate.

## One-call synthesis

```c
options.style = NANOTTS_KO_STYLE_CLEAR_DEVICE;

nanotts_speak_text(
    &tts,
    "문이 열렸습니다.",
    NANOTTS_NPOS,
    &options,
    audio_write,
    user,
    &parse_info);
```

The callback receives a temporary PCM block. Return nonzero to abort.

## Parse and render separately

```c
nanotts_text_options_t text_options;
nanotts_default_text_options(&text_options);
text_options.style = NANOTTS_KO_STYLE_MODERN_SEOUL;

nanotts_parse_text_ex(
    &tts, text, NANOTTS_NPOS, &text_options, &info);

inspect(nanotts_morphemes(&tts), nanotts_morpheme_count(&tts));
inspect(nanotts_events(&tts), nanotts_event_count(&tts));

nanotts_render_events(&tts, &options, audio_write, user);
```

`nanotts_parse_text()` remains as a convenience wrapper using the default style.
`nanotts_set_events()` accepts validated acoustic events directly.

## Text options

```c
typedef struct nanotts_text_options {
    uint16_t flags;
    uint8_t style;
    uint8_t reserved;
} nanotts_text_options_t;
```

Important flags:

```text
NANOTTS_OPT_LINK_EOJEOL
NANOTTS_OPT_NO_AUTOPAUSE
NANOTTS_OPT_NO_BUILTIN_LEXICON
NANOTTS_OPT_NO_MORPHOLOGY
NANOTTS_OPT_NO_NORMALIZATION
NANOTTS_OPT_SPELL_LATIN
```

The style must be chosen during parsing because it can alter pronunciation as
well as timing.

## Render options

```text
rate_q8        256 = normal speed
pitch_cents    global pitch offset, -1200..1200
volume         0..255
final_tone     auto/fall/rise/continue/level
style          same style used for parsing
expression_q8  128 = normal pitch/prominence range
```

## Parse information

`nanotts_parse_info_t` reports:

```text
event, syllable, token, and morpheme counts
UTF-8 error byte and code point
application/built-in lexicon hits
normalization operations
selected pronunciation variants
```

## Morpheme inspection

```c
const nanotts_morpheme_t *m = nanotts_morphemes(&tts);
size_t count = nanotts_morpheme_count(&tts);
```

Each entry contains a syllable span, flags, token index, and compact POS ID.
Use `nanotts_ko_pos_name()` for diagnostics.

## Lexicon callbacks

Legacy callback:

```c
nanotts_set_lexicon(&tts, lookup, user);
```

Extended callback:

```c
nanotts_set_lexicon_ex(&tts, lookup_ex, user);
```

The extended callback receives the style and may return morphology flags in
addition to a Hangul pronunciation spelling. Application callbacks are checked
before the built-in exception table.

## Event format

Each event is eight bytes:

```c
typedef struct nanotts_event {
    uint8_t phone;
    uint8_t flags;
    uint8_t duration_percent;
    int8_t  pitch_semitones_q4;
    int8_t  pitch_end_semitones_q4;
    uint8_t prominence_q8;
    uint8_t acoustic_flags;
    uint8_t reserved;
} nanotts_event_t;
```

Pitch is a continuous start/end trajectory in sixteenth-semitone units.
Prominence uses 128 as the neutral scale. Acoustic flags carry allophone and
prosodic information such as palatalization, weak `ㅎ`, focus, command, and
phrase-initial denasalization.

This structure is suitable for in-process caching. It is not declared a stable
serialized wire format across major versions.

## Static sizing

Compile-time controls:

```text
NANOTTS_MAX_EVENTS
NANOTTS_MAX_SYLLABLES
NANOTTS_MAX_TOKENS
NANOTTS_MAX_MORPHEMES
NANOTTS_CONTEXT_BYTES
NANOTTS_AUDIO_BLOCK
NANOTTS_CONTROL_MS
NANOTTS_COEFF_STRIDE
```

The public capacity definitions must match in the library and consumer because
`nanotts_t` contains the configured opaque byte count. CMake exports these
public definitions automatically.
