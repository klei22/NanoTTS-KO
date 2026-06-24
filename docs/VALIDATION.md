# Validation

This file records engineering validation for NanoTTS-KO 2.0.0. It does not
claim a native-listener word-error rate, naturalness score, or MOS result.

## Automated suite

The default CTest build contains thirteen groups:

1. Public API, capacity, reset, callback-abort, and error handling.
2. Korean decomposition and productive phonology.
3. Morphology-dependent homographs and fortition.
4. Semantic number/date/time/unit normalization.
5. Prescriptive, modern, clear-device, conversational, and learner styles.
6. Clause-local Korean prosody and continuous pitch targets.
7. Deterministic synthesis and bounded PCM output.
8. Silence-edge de-clicking.
9. Connected-phone transition continuity.
10. Randomized Hangul and mixed-token robustness.
11. Written-to-pronunciation regression pairs.
12. Timer-PWM mapping and chunking.
13. PCM16LE serialization and chunking.

The robustness test performs 30,000 randomized Hangul parses and 30,000 mixed
Korean/number/date/time/fraction/phone/version/unit/Latin parses when semantic
normalization is compiled. Periodic cases are rendered through the complete DSP
path. Event, token, and morpheme bounds are checked on every iteration.

## Pronunciation regression data

`tests/data/ko_pronunciations.tsv` contains 133 written/pronunciation pairs:

| Class | Cases | Dependency |
|---|---:|---|
| Spelling-productive rules | 36 | Core phonology |
| Morphology-conditioned rules | 17 | Compact morphology analyzer |
| Lexical/compound exceptions | 80 | Built-in exception lexicon |

Coverage includes liaison, palatalization, `ㅎ` interactions, coda
neutralization, nasalization, lateralization, obstruent fortition, complex
codas, selected `ㄴ` insertion, verb-stem fortition, attributive `-(으)ㄹ`,
and official compound-fortition examples. Capability-specific builds skip only
cases whose required removable module is absent.

## Build matrix

Validation host: GCC 14.2, Clang 17.0, CMake 3.31, x86-64 Linux.

| Build | Important options | Result |
|---|---|---:|
| Full GCC | `libm`, lexicon, morphology, normalization, PCM, PWM | 13/13 pass |
| Compact GCC | no `libm`, 8 KB context, PWM only, 2 ms controls | 12/12 pass |
| No built-in lexicon | morphology and normalization retained | 13/13 pass |
| Spelling-only minimum | no lexicon, morphology, normalization, adapters, or `libm` | 11/11 pass |
| Clang | no `libm`, full linguistic frontend | 13/13 pass |
| AddressSanitizer + UndefinedBehaviorSanitizer | full frontend and adapters | 13/13 pass |

A separate GCC build with `-Werror -fanalyzer` completed without a core
diagnostic. Strict warnings include `-Wall -Wextra -Wpedantic -Wconversion
-Wshadow`.

## Package integration

The project installs these CMake targets when enabled:

```text
NanoTTS::NanoTTS
NanoTTS::PCM
NanoTTS::PWM
```

A clean external project was configured with `find_package(NanoTTS 2.0)`, linked
against the installed core target, built, and executed successfully.

## Configuration-specific behavior

Removing a module changes capability rather than silently retaining its data:

- Without the lexicon, lexical pronunciations such as the two accepted
  realizations of `맛있다` fall back to spelling rules.
- Without morphology, rules depending on verb stems, endings, nominalizers, or
  attributive forms are intentionally unavailable.
- Without normalization, numbers and Latin input are read through the literal
  spelling path.

Tests are compiled with the same feature definitions as the library and assert
only the guarantees available in that profile.

## Acoustic continuity

Version 2.0 retains the 1.3 renderer's shared boundary states,
phone-pair-specific overlap, gradual frication and aspiration edges, dormant
filter gating, de-zippered biquad coefficients, and residual transition bridge.
The existing de-click and connected-transition tests remain enabled and pass in
all renderer profiles.

The linguistic frontend now also supplies:

- continuous per-event pitch trajectories;
- accentual-phrase start/end flags;
- weak-`ㅎ`, palatalized-sibilant, denasalization, focus, question, and command
  acoustic flags;
- context-sensitive durations and prominence.

These checks catch discontinuities and missing cues but are not substitutes for
native Korean listening tests.

## Known validation limits

The bundled voice remains a compact original formant/source-filter voice. No
recorded Korean voice database or neural model is included. The release has not
yet undergone a randomized native-listener transcription or MOS study.

The compact morphology analyzer is intentionally bounded and does not equal a
large desktop lexical analyzer. Proper names, Hanja, uncommon compounds,
foreign words, and semantic ambiguities may need an application lexicon. Public
or safety-critical prompts should be reviewed by a native speaker or supplied
as explicit application pronunciations.
