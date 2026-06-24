# Footprint

NanoTTS-KO 2.0 deliberately spends more flash and caller-owned RAM than the
1.x spelling-only frontend. The additional storage carries token, morpheme,
normalization, pronunciation-variant, and continuous-prosody state. There is
still no heap allocation.

## Runtime context

The public `nanotts_t` is compile-time-sized opaque storage. The validation
host used the following layouts:

| Profile | Events | Syllables | Tokens | Morphemes | Internal structure | Public context |
|---|---:|---:|---:|---:|---:|---:|
| Full linguistic frontend | 640 | 192 | 96 | 192 | 11,696 B | 16,384 B |
| Compact linguistic frontend | 320 | 128 | 64 | 128 | 7,088 B | 8,192 B |
| Compact spelling-only frontend | 320 | 128 | 64 | 64 | 6,576 B | 8,192 B |

The full public context intentionally has spare room for ABI variation and
future minor-version state. Relevant structure sizes on the validation host:

```text
acoustic event      8 B
Korean syllable    12 B
token               20 B
morpheme             8 B
```

Applications with tightly bounded prompts may lower all four capacities. The
compile-time context-size assertion prevents a configuration that is too small.
The library and its consumer must use matching public capacity definitions.

## Host object measurements

GCC 14.2, x86-64, `-Os`, measured as the sum of object-file `text + initialized
data` sections:

| Configuration | Code + initialized data |
|---|---:|
| Full frontend, lexicon, morphology, normalization | 55,514 B |
| Compact frontend, no `libm` | 55,534 B |
| Full frontend without built-in lexicon | 50,178 B |
| Compact spelling-only frontend, no lexicon/normalization/`libm` | 45,346 B |
| Raw PCM16LE adapter | 349 B |
| Timer-PWM adapter | 445 B |

The small host increase in the no-`libm` profile occurs because local math
approximations replace library calls. The result on an MCU depends on which
math routines the target linker would otherwise retain.

Approximate full-build object contributions were:

| Module | Code + initialized data |
|---|---:|
| Source/filter renderer | 20,338 B |
| Semantic normalization | 8,220 B |
| Built-in pronunciation lexicon | 8,170 B |
| Compact morphology analyzer | 5,727 B |
| Prosody/event planner | 4,104 B |
| Public API and event management | 2,831 B |
| Ordered Korean phonology | 2,088 B |
| Acoustic phone data | 2,087 B |
| UTF-8/Hangul/token support | 1,949 B |

These measurements exclude the CLI, tests, examples, startup code, C runtime,
platform audio driver, and any toolchain-provided math implementation. MCU
flash and RAM claims must be taken from the target linker map.

## Deployment choices

A practical product can choose among three profiles:

```text
maximum linguistic coverage:
    normalization + morphology + lexicon + renderer

smaller dynamic-text firmware:
    spelling frontend + selected application lexicon + renderer

smallest fixed-prompt firmware:
    host-precomputed events + renderer only
```

Version 2.0 exposes `nanotts_set_events()` so applications can store analyzed
acoustic events in firmware and omit dynamic text analysis from the runtime
build. The event structure is suitable for in-process caching, but it is not a
stable serialized wire format across major versions.
