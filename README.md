# NanoTTS-KO

NanoTTS-KO is a Korean-only, MIT-licensed text-to-speech engine for small
microcontrollers. It converts UTF-8 Hangul directly to signed 16-bit PCM using
a deterministic Korean-specific source/filter synthesizer. The core has no heap
allocation, filesystem, threading, operating-system, or audio-device dependency.

Version 1.2 adds a continuous de-clicking signal path on top of the Korean
quality renderer, spending additional per-sample arithmetic to remove hard cuts,
filter zippering, and stale-state transitions:

```text
UTF-8 Korean text
  -> Hangul arithmetic decomposition
  -> ordered Korean sound-change rules and exception overrides
  -> syllable timing and accentual-phrase targets
  -> Korean phone events
  -> high-resolution Korean source/filter renderer
  -> PCM callback
  -> I2S / DAC / PCM16LE / timer-PWM / host WAV
```

## Highlights

- Korean-only public API and firmware path.
- Precomposed Hangul and modern conjoining Jamo input.
- Three-way lax, tense, and aspirated stops and affricates with distinct release,
  aspiration, source-quality, and following-vowel F0 cues.
- Dedicated unreleased final `/k̚ t̚ p̚/` closures.
- Liaison, palatalization, complex-coda resolution, aspiration, final
  neutralization, nasalization, lateralization, tensification, and selected
  connected-speech `ㄴ` insertion.
- Coherent modern-Seoul-oriented vowel targets and a moving initial `ㅢ` nucleus.
- Two nasal poles, two place-dependent antiresonances, coda-sonorant variants,
  intervocalic `ㅎ` lenition, and immediate-neighbor coarticulation.
- Seoul-style compact accentual-phrase pitch patterns plus statement, question,
  and continuation contours.
- Korean integers, leading-zero digit strings, and Latin letter names.
- Compact built-in pronunciation exceptions plus an application lexicon hook.
- Direct PCM callbacks and optional raw PCM16LE and timer-PWM adapters.
- Continuous source/gain smoothing and high-rate resonator de-zippering.
- Raised-cosine silence edges and pause-masked state reset for click-free joins.
- C99, zero heap, deterministic output, and optional no-`libm` builds.

## Build

Full-quality host or FPU-MCU build:

```bash
./setup.sh
```

The default quality profile plans targets every 1 ms and advances filter
coefficients every two samples. A smaller, lower-CPU PWM-oriented profile uses
2 ms targets and a four-sample coefficient stride automatically:

```bash
./setup.sh --compact --no-libm --outputs pwm
```

Override the control period explicitly when benchmarking a target:

```bash
./setup.sh --control-ms 2
./setup.sh --compact --control-ms 1 --coeff-stride 2 --no-libm --outputs pwm
```

Equivalent CMake use:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DNANOTTS_CONTROL_MS=1 \
  -DNANOTTS_COEFF_STRIDE=2
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Command line

```bash
./build-make-ko-all/nanotts \
  --sample-rate 24000 \
  --text "안녕하세요. 한국어 음성 합성기입니다." \
  -o speech.wav
```

Inspect frontend events:

```bash
./build-make-ko-all/nanotts \
  --text "국민, 신라, 학교, 꽃이, 갈등, 넓게" \
  --dump-phones -o rules.wav
```

Raw MCU-oriented output:

```bash
./build-make-ko-all/nanotts --text "배터리가 부족합니다." \
  --format pcm -o speech.pcm16le

./build-make-ko-all/nanotts --text "문이 열렸습니다." \
  --format pwm --pwm-top 1023 -o speech.pwm16le
```

## MCU API

```c
#include <nanotts/nanotts.h>

static int audio_write(void *user, const int16_t *samples, size_t count)
{
    return i2s_dma_submit(user, samples, count) ? 0 : 1;
}

static nanotts_t tts;
nanotts_options_t options;

nanotts_init(&tts, 16000);
nanotts_default_options(&options);

nanotts_speak_text(
    &tts,
    "배터리가 부족합니다.",
    NANOTTS_NPOS,
    &options,
    audio_write,
    &audio_device,
    NULL);
```

`nanotts_t` is caller-owned static storage. The renderer streams blocks to the
callback and never allocates memory.

## Input and pronunciation policy

NanoTTS-KO accepts modern precomposed Hangul, modern conjoining Jamo, ASCII
digits and letters, and common Korean/ASCII punctuation. Korean pronunciation
is not fully recoverable from spelling without morphology and lexical
knowledge. Product names, personal names, Sino-Korean compounds, and critical
prompts should use `nanotts_set_lexicon()` when the built-in pronunciation is
not the desired one.

## Quality scope

The renderer is optimized for intelligibility per byte, not neural-TTS
naturalness. Version 1.2 preserves the Korean vowel, stop, nasal, glide, coda, and phrase
model from 1.1 while smoothing phone and silence transitions, but it remains a
recognizably synthetic formant voice. Automated acoustic and pronunciation
checks are included; a formal native-listener transcription or MOS study has
not yet been completed.

See:

- `docs/QUALITY.md`
- `docs/KOREAN_FRONTEND.md`
- `docs/ACOUSTICS.md`
- `docs/API.md`
- `docs/PORTING.md`
- `docs/OUTPUTS.md`
- `docs/DECLICKING.md`
- `docs/FOOTPRINT.md`
- `docs/VALIDATION.md`
- `PROVENANCE.md`

## License

MIT. See `LICENSE` and `PROVENANCE.md`.
