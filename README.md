# NanoTTS-KO

NanoTTS-KO is a Korean-only, MIT-licensed text-to-speech engine designed for
microcontrollers and other resource-constrained targets. It converts UTF-8
Korean text directly to mono signed 16-bit PCM without heap allocation,
filesystem access, threads, an operating system, or an audio-device dependency.

Version 2.0 replaces the spelling-only frontend with a bounded Korean linguistic
pipeline:

```text
UTF-8 Korean text
  -> token classification and semantic normalization
  -> compact morpheme/POS analysis
  -> style-dependent pronunciation candidates
  -> ordered Korean phonology
  -> accentual-phrase, duration, and continuous-F0 planning
  -> Korean acoustic events
  -> de-clicked source/filter renderer
  -> PCM callback
  -> I2S / DAC / PCM16LE / timer-PWM / host WAV
```

The analyzer is deliberately smaller than a desktop Korean NLP stack. It is
large enough to distinguish important pronunciation contexts such as verbal
`신고` from nominal `신고`, passive/causative `안기다` from nominalized `안기`,
and attributive `-(으)ㄹ` before a following noun. Applications can override
ambiguous names, compounds, and domain vocabulary through a zero-copy lexicon
callback.

## Highlights

- Modern precomposed Hangul and modern conjoining Jamo input.
- Compact token and morpheme metadata with no runtime allocation.
- Context-sensitive native and Sino-Korean number readings.
- Dates, times, decimals, fractions, percentages, telephone-like digit strings,
  version/IP-style dotted sequences, currencies, temperatures, and common MCU
  measurement units.
- Morphology-conditioned fortition, including verbal endings, passive versus
  nominalized `-기`, and attributive `-(으)ㄹ`.
- Liaison, palatalization, complex-coda resolution, `ㅎ` behavior, final
  neutralization, nasalization, lateralization, obstruent fortition, selected
  `ㄴ` insertion, and a removable lexical exception layer.
- Five selectable policies: prescriptive, modern Seoul, clear-device,
  conversational, and learner.
- Clause-local statement, yes/no-question, wh-question, command, continuation,
  and phrase-final contours.
- Separate lax, tense, and aspirated stops and affricates, palatalized sibilants,
  unreleased coda closures, nasal poles/zeros, glide trajectories, and connected
  phone overlap.
- Raw PCM16LE and timer-compare PWM adapters for MCU deployment.
- Deterministic C99 core, optional no-`libm` build, and removable normalization,
  morphology, and built-in lexicon modules.

## Build

Full-quality build and tests:

```bash
./setup.sh
```

Compact PWM-oriented profile:

```bash
./setup.sh --compact --no-libm --outputs pwm
```

A spelling-only minimum frontend can remove the two largest linguistic stages:

```bash
./setup.sh --compact --no-libm --no-morphology --no-normalization --outputs pwm
```

Equivalent CMake use:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Command line

Generate a 24 kHz WAV:

```bash
./build-make-ko2-all/nanotts \
  --sample-rate 24000 \
  --style modern \
  --text "안녕하세요. 한국어 음성 합성기입니다." \
  -o speech.wav
```

Inspect contextual analysis and acoustic events:

```bash
./build-make-ko2-all/nanotts \
  --text "신을 신고하고 열두 시 삼십 분에 출발합니다." \
  --dump-morph --dump-phones \
  -o analysis.wav
```

Compare morphology-dependent readings:

```bash
./build-make-ko2-all/nanotts --text "신을 신고" --dump-morph -o verb.wav
./build-make-ko2-all/nanotts --text "혼인 신고" --dump-morph -o noun.wav
./build-make-ko2-all/nanotts --text "안기다. 안기." --dump-morph -o gi.wav
```

Use a clearer embedded-device delivery:

```bash
./build-make-ko2-all/nanotts \
  --style clear --expression 115 \
  --text "경고. 배터리가 부족합니다." \
  -o alert.wav
```

Raw MCU-oriented output:

```bash
./build-make-ko2-all/nanotts \
  --text "문이 열렸습니다." --format pcm -o speech.pcm16le

./build-make-ko2-all/nanotts \
  --text "문이 열렸습니다." --format pwm --pwm-top 1023 \
  -o speech.pwm16le
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

nanotts_init(&tts, 16000u);
nanotts_default_options(&options);
options.style = NANOTTS_KO_STYLE_CLEAR_DEVICE;

nanotts_speak_text(
    &tts,
    "배터리가 부족합니다.",
    NANOTTS_NPOS,
    &options,
    audio_write,
    &audio_device,
    NULL);
```

`nanotts_t` is caller-owned static storage. The renderer streams fixed-size
blocks and never allocates memory.

## Pronunciation overrides

Korean spelling alone cannot fully determine morphology, lexical fortition,
proper-name readings, Hanja readings, or all acceptable connected-speech
variants. Use `nanotts_set_lexicon_ex()` for application vocabulary. The
extended callback receives the selected style and can return both a Hangul
pronunciation spelling and morphology flags.

The built-in exception layer is removable:

```bash
./setup.sh --no-lexicon
```

## Quality scope

NanoTTS-KO 2.0 makes the linguistic and prosodic plan substantially richer,
but the bundled voice is still an original compact formant/source-filter voice,
not a recorded or neural Korean speaker. No sampled Korean voice data is
shipped. A future LPC/micro-unit backend needs a properly licensed native-speaker
corpus; the current event boundary is designed so such a renderer can reuse the
same normalizer, morphology, phonology, and prosody stages.

Formal native-listener transcription and MOS testing remains necessary before
claiming a human quality score or deploying safety-critical public prompts
without review.

## Documentation

- `docs/ARCHITECTURE.md`
- `docs/IMPLEMENTATION_REPORT.md`
- `docs/KOREAN_FRONTEND.md`
- `docs/NORMALIZATION.md`
- `docs/PROSODY.md`
- `docs/ACOUSTICS.md`
- `docs/ALTERNATE_RENDERERS.md`
- `docs/API.md`
- `docs/PORTING.md`
- `docs/OUTPUTS.md`
- `docs/DECLICKING.md`
- `docs/TRANSITIONS.md`
- `docs/FOOTPRINT.md`
- `docs/VALIDATION.md`
- `PROVENANCE.md`

## License

MIT. See `LICENSE` and `PROVENANCE.md`.
