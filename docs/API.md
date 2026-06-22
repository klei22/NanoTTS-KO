# API

Include:

```c
#include <nanotts/nanotts.h>
```

## Lifecycle

```c
nanotts_t tts;
nanotts_options_t options;

nanotts_result_t r = nanotts_init(&tts, 16000);
nanotts_default_options(&options);
```

`nanotts_t` is opaque caller-owned storage. Do not copy it while rendering.
`nanotts_reset()` clears queued events and DSP state while preserving the sample
rate and lexicon callback.

Supported sample rates are 8000 through 24000 Hz. The library produces mono
signed `int16_t` PCM at the exact rate passed to `nanotts_init()`.

## One-call synthesis

```c
nanotts_speak_text(
    &tts,
    "문이 열렸습니다.",
    NANOTTS_NPOS,
    &options,
    audio_write,
    user,
    &parse_info);
```

The callback receives a temporary block. Return nonzero to abort synthesis.

## Parse and render separately

```c
nanotts_parse_text(&tts, text, NANOTTS_NPOS, options.flags, &info);
inspect(nanotts_events(&tts), nanotts_event_count(&tts));
nanotts_render_events(&tts, &options, audio_write, user);
```

This is useful for diagnostics and for caching event streams in an application.
`nanotts_set_events()` accepts validated Korean acoustic events directly.

## Options

- `rate_q8`: `256` is normal speed; accepted range is 96 through 640.
- `pitch_cents`: global pitch offset, `-1200..1200`.
- `volume`: `0..255`.
- `final_tone`: automatic, fall, rise, continue, or level.
- `NANOTTS_OPT_LINK_EOJEOL`: allow connected-speech rules across spaces.
- `NANOTTS_OPT_NO_AUTOPAUSE`: remove generated accentual-phrase pauses.
- `NANOTTS_OPT_NO_BUILTIN_LEXICON`: bypass the compact exception lexicon.

## Lexicon callback

```c
nanotts_set_lexicon(&tts, lookup, user);
```

A successful lookup returns a UTF-8 Hangul pronunciation spelling. The callback
owns the returned pointer and must keep it valid until it returns. The text is
parsed immediately; NanoTTS does not retain that pointer.

## Event format

Each event is four bytes:

```c
typedef struct nanotts_event {
    uint8_t phone;
    uint8_t flags;
    uint8_t duration_percent;
    int8_t pitch_semitones_q4;
} nanotts_event_t;
```

This is the boundary between the Korean frontend and renderer. It is suitable
for application-level caching, but it is not yet declared a stable serialized
wire format across major versions.

## Static sizing

Compile-time controls:

```text
NANOTTS_MAX_EVENTS
NANOTTS_MAX_SYLLABLES
NANOTTS_CONTEXT_BYTES
NANOTTS_AUDIO_BLOCK
NANOTTS_CONTROL_MS
NANOTTS_COEFF_STRIDE
```

`NANOTTS_CONTROL_MS` and `NANOTTS_COEFF_STRIDE` must be positive integers from
1 through 4. Lower values improve trajectory resolution and coefficient
smoothness but increase CPU cost. The default profile uses 1 ms and stride 2;
the compact setup profile uses 2 ms and stride 4.

The public size definitions must be identical for the library and consumer
because `nanotts_t` contains the selected opaque byte count. CMake exports the
required public definitions automatically.
