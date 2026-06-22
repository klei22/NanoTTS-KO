# Validation

This file records engineering validation for NanoTTS-KO 1.2.0. It does not
claim human intelligibility, word-error-rate, naturalness, or MOS scores.

## Automated tests

The CTest suite covers:

- API initialization, bounds, errors, reset, and callback aborts.
- UTF-8 errors and unsupported characters.
- Korean G2P regression sequences.
- Liaison, connected-speech `ㄴ` insertion, palatalization, aspiration,
  tensification, nasalization, lateralization, and complex codas.
- Initial moving `ㅢ` and compound-vowel events.
- Internal questions and comma continuation boundaries.
- Accentual-phrase grouping and deterministic synthesis.
- Long-silence edge continuity and pause de-clicking.
- PCM16LE byte order and chunking.
- PWM endpoint mapping, inversion, and chunking.
- 20,000 deterministic randomized Hangul parser calls, with periodic renders.
- A 76-pair written/pronunciation regression file.

In a normal lexicon-enabled build, all 76 pronunciation pairs are tested. In a
no-lexicon build, the 60 entries explicitly marked `lex` are skipped and the 16
productive-rule pairs still run.

## Build matrix

| Build | Compiler/math | Adapters | Targets / coefficient stride | Context | Result |
|---|---|---|---|---:|---|
| Full quality | GCC 14.2 + `libm` | PCM + PWM | 1 ms / 2 samples | 4096 B | 8/8 pass |
| Compact | GCC 14.2, no `libm` | PWM | 2 ms / 4 samples | 3072 B | 7/7 pass |
| Full/no lexicon | GCC 14.2 + `libm` | PCM + PWM | 1 ms / 2 samples | 4096 B | 8/8 pass |
| Core only | GCC 14.2 + `libm` | none | 1 ms / 2 samples | 4096 B | 6/6 pass |
| Full | Clang 17, no `libm` | PCM + PWM | 1 ms / 2 samples | 4096 B | 8/8 pass |
| Sanitized | GCC ASan + UBSan | PCM + PWM | 1 ms / 2 samples | 4096 B | 8/8 pass |

Strict GCC compilation with `-Werror -fanalyzer` also completes without a core
or adapter diagnostic.

## Text corpus

`tests/data/ko_smoke.txt` contains 57 inputs spanning:

- eight vowel categories, moving `ㅢ`, and compound vowels;
- lax/tense/aspirated stop and affricate contrasts;
- all major surface coda classes;
- nasals, liquids, glides, intervocalic `ㅎ`, and coda transitions;
- high-value productive sound changes;
- built-in exception-lexicon forms;
- statements, questions, commas, and multi-sentence text;
- integers, leading-zero sequences, Latin acronyms, and MCU alert phrases.

All 57 inputs parse and produce non-empty WAV output in the release check.

## De-click evaluation

Version 1.1 and 1.2 rendered the same representative 24 kHz phrase:

```text
안녕하세요. 한국어 음성 합성기입니다.
```

Phone-boundary positions were obtained from the event stream. A silence edge
was included when an exact-zero run lasted at least 5 ms. Values below are
normalized to full-scale signed 16-bit PCM.

| Diagnostic | 1.1 | 1.2 | Change |
|---|---:|---:|---:|
| Mean absolute phone-boundary jump | 0.06593 | 0.01206 | -81.7% |
| 90th-percentile phone-boundary jump | 0.15017 | 0.02107 | -86.0% |
| Largest amplitude touching a long silence run | 0.06610 | 0.00616 | -90.7% |
| Largest sample-to-sample step | 0.45010 | 0.40616 | -9.8% |

A tap/liquid-heavy probe was also rendered:

```text
라라라. 아라아라. 우리 라디오를 들어요.
```

| Diagnostic | 1.1 | 1.2 | Change |
|---|---:|---:|---:|
| Largest amplitude touching a long silence run | 0.11749 | 0.00903 | -92.3% |
| 99.9th-percentile sample step | 0.19861 | 0.15991 | -19.5% |
| Largest sample-to-sample step | 0.51663 | 0.40869 | -20.9% |

The remaining large individual steps are mostly intentional stop releases and
fricative excitation rather than pause cuts. These diagnostics show improved
continuity but do not substitute for listening tests.

## Earlier acoustic evaluation

Thirty matched 24 kHz prompts were rendered by versions 1.0 and 1.1. They cover
vowels, glides, nasals, stop contrasts, sound changes, lexical exceptions,
questions, numbers, long sentences, and safety prompts.

Mean diagnostics over the 30 prompts:

| Diagnostic | 1.0 | 1.1 |
|---|---:|---:|
| Duration | 2.886 s | 2.750 s |
| Active RMS | 0.0793 | 0.1261 |
| Absolute peak | 0.5605 | 0.6382 |
| Samples at/above 0.899 | 0% | 0% |
| Spectral centroid | 1511.5 Hz | 525.7 Hz |
| Power at/above 3 kHz | 21.60% | 5.42% |
| Power at/above 5 kHz | 15.23% | 3.85% |
| Spectral flatness | 0.1851 | 0.0470 |
| 99.9th-percentile sample difference | 0.3660 | 0.2967 |

The measurements show that 1.1 is less globally dominated by broad
noise/release energy and has stronger periodic vowel/sonorant structure. They
do not by themselves prove better intelligibility or naturalness.

## Three-way onset-F0 check

A PYIN diagnostic measured the first-vowel windows of matched two-syllable
probes at 24 kHz:

| Probe class | Median estimated first-vowel F0 |
|---|---:|
| Lax | 107.1 Hz |
| Tense | 134.9 Hz |
| Aspirated | 141.3 Hz |

This verifies that the intended generated cue reaches the vowel, rather than
remaining attached only to an unvoiced consonant event. Release timing and
voice quality are modeled separately and are not summarized by this F0 test.

## Relative host CPU diagnostic

Rendering the 3.37-second demonstration phrase 200 times at 24 kHz took a
median of about 1.45 seconds in 1.1 and 1.77 seconds in the default 1.2 profile
on the validation host. The approximately 22% increase is a comparative host
diagnostic, not an MCU real-time guarantee.

## Package integration

The project installs CMake targets:

```text
NanoTTS::NanoTTS
NanoTTS::PCM
NanoTTS::PWM
```

A clean external project is configured with `find_package(NanoTTS 1.2)`, linked
against the installed targets, built, and executed as part of release
validation.

## Interpretation and remaining work

Objective checks are useful for catching clipping, unstable filters, missing
pitch cues, excessive broadband noise, discontinuities, regressions, and
package failures. They cannot substitute for native Korean listeners. A formal
randomized transcription and preference study remains necessary before claiming
a human quality score or deploying safety-critical public prompts without
review.
