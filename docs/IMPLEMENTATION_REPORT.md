# NanoTTS-KO 2.0 implementation report

## Result

NanoTTS-KO was refactored from a spelling-oriented Korean frontend into a
bounded morphology-aware speech pipeline while preserving the zero-allocation,
portable-C MCU design.

```text
UTF-8 Korean
  -> tokenization and semantic normalization
  -> compact morpheme/POS analysis
  -> application and built-in pronunciation selection
  -> ordered Korean phonology
  -> clause/AP prosody and context-dependent allophones
  -> eight-byte acoustic events
  -> de-clicked source/filter renderer
  -> PCM callback / PCM16LE / timer PWM
```

## Linguistic changes

- Token and morpheme tables retain source spans, POS, boundaries, and rule flags.
- Context distinguishes verbal `신고` from nominal `신고`.
- Passive/causative `안기다` is distinguished from nominalized `안기`.
- Attributive `-(으)ㄹ` is recognized before plausible following nouns.
- Homographic `-하고` is analyzed as a verbal ending only after a known verb
  stem and as a particle after ordinary nouns.
- Verb-only cluster fortition is no longer overgeneralized to noun forms such
  as `여덟도`.
- The rule order preserves lexical onset/coda information until
  morphology-sensitive fortition, liaison, cluster resolution, `ㅎ` behavior,
  neutralization, nasalization, and lateralization have run.
- Prescriptive, modern Seoul, clear-device, conversational, and learner
  policies select pronunciation and prosody variants.

## Text normalization

The bounded normalizer supports native/Sino Korean number selection, counters,
dates, clock times, decimals, fractions, percentages, signed quantities,
telephone-like digit strings, dotted versions and IP-style strings, currency,
temperature, Latin acronyms, and common embedded-device units.

Numeric punctuation is parsed structurally. A date is accepted as a date only
when its shape is valid; grouping commas remain inside numbers while terminal
sentence punctuation remains a phrase boundary.

## Prosody and acoustic plan

- Question, command, statement, and continuation modality is attached to the
  relevant clause rather than globally to the utterance.
- Yes/no and wh-questions receive distinct final contours.
- AP grouping uses morphology, particles, endings, token length, and explicit
  boundaries rather than only a fixed syllable count.
- Events carry continuous F0 endpoints, prominence, palatalized-sibilant,
  weak-`ㅎ`, phrase-initial denasalization, focus, question, and command cues.
- The 1.3 connected-phone overlap and de-click renderer remains underneath the
  richer plan.

## Static cost

GCC 14.2 x86-64 `-Os` object-section measurements:

| Configuration | Code + initialized data | Internal context |
|---|---:|---:|
| Full linguistic frontend | 55,514 B | 11,696 B |
| Compact no-`libm` frontend | 55,534 B | 7,088 B |
| Full without built-in lexicon | 50,178 B | 11,696 B |
| Compact spelling-only frontend | 45,346 B | 6,576 B |

The full and compact public contexts are 16,384 and 8,192 bytes respectively.
Final MCU flash and RAM must be read from the target linker map.

## Validation

- 133 written/pronunciation pairs: 36 spelling-productive, 17
  morphology-conditioned, and 80 lexical/compound cases.
- 30,000 randomized Hangul inputs and 30,000 mixed semantic-normalization inputs
  per full fuzz run.
- Full GCC, compact no-`libm`, no-lexicon, spelling-only, Clang no-`libm`, and
  combined Address/Undefined sanitizer profiles pass.
- GCC `-Werror -fanalyzer` completes without a core diagnostic.
- Clean CMake install and external `find_package(NanoTTS 2.0)` consumer pass.
- Generated 24 kHz release samples contain no clipped signed-16-bit samples.

## Important limitation

This release significantly improves linguistic accuracy and prosodic planning,
but the bundled voice remains a compact original formant/source-filter voice.
No recorded LPC/micro-unit or neural Korean voice was added because no
properly licensed native-speaker corpus was supplied. The event boundary and
`nanotts_set_events()` permit such a backend to reuse the entire new frontend.

The compact analyzer is not a full desktop morphological parser. Proper names,
Hanja, uncommon compounds, foreign words, and genuinely semantic ambiguities
may require the application lexicon callback or host-side preprocessing.
