# Architecture

NanoTTS-KO 2 is a statically bounded Korean speech pipeline. All variable-size
working sets are arrays inside caller-owned `nanotts_t`; there is no heap and no
hidden global mutable state.

## Data flow

```text
UTF-8 bytes
  |
  v
scanner ------------------------------ token table
  |                                      |
  v                                      v
semantic normalizer ---------------- Korean syllable table
  |                                      |
  v                                      v
compact morphology ---------------- morpheme/POS table
  |                                      |
  v                                      v
style/lexical selection -------- underlying pronunciation
  |
  v
ordered phonology --------------- surface onset/vowel/coda
  |
  v
prosody/allophone planner -------- 8-byte acoustic events
  |
  v
source/filter renderer ----------- int16 PCM callback
```

## Static working sets

The full default profile reserves:

```text
640 acoustic events
192 Korean syllables
96 text tokens
192 morphemes
16,384 bytes of opaque context
```

The compact setup profile uses:

```text
320 acoustic events
128 Korean syllables
64 text tokens
128 morphemes
8,192 bytes of opaque context
```

Capacities are compile-time definitions. The library returns a specific
`TOO_MANY_*` error rather than allocating or truncating.

## Token stage

The scanner recognizes Hangul, ASCII numbers, ASCII Latin strings, supported
symbols, whitespace, and punctuation. Punctuation is attached to the relevant
clause-ending token so a question in one clause cannot leak its contour into a
later statement.

## Normalization stage

The semantic normalizer converts structured tokens into spoken Hangul while
retaining the original token index. A number followed by a supported counter
can choose a native Korean form; ordinary numeric quantities use Sino-Korean
forms. See `NORMALIZATION.md`.

## Morphology stage

The compact analyzer assigns POS and morphology flags to bounded spans of
syllables. It is a deterministic suffix and context analyzer, not a complete
statistical Korean parser. High-value distinctions are encoded explicitly:

```text
verb stem + ending
noun + particle
passive/causative -기
nominalizing -기
attributive -(으)ㄹ
counter and number
particle 의
question and command endings
```

The public morpheme table supports diagnostics and application testing. The
extended lexicon callback can attach morphology flags to an override.

## Pronunciation policy

Styles select among legitimate variants and timing/prosody profiles before
rendering:

```text
prescriptive
modern Seoul
clear device
conversational
learner
```

The engine currently resolves candidates at parse time rather than retaining a
large runtime lattice. `parse_info.variant_count` reports selected alternate
forms.

## Ordered phonology

Underlying spelling is retained separately from the mutable surface form.
Morphology-conditioned rules run before liaison and coda neutralization so the
information they require is not erased. See `KOREAN_FRONTEND.md`.

## Event boundary

The frontend emits eight-byte events:

```text
phone ID
structural flags
duration scale
pitch start
pitch end
prominence
acoustic/allophone flags
reserved byte
```

This is the stable internal boundary for alternative renderers. A future
LPC-unit or neural-vocoder backend can consume the same linguistic plan without
reimplementing Korean text processing. The event structure is an in-process API,
not yet a versioned serialized wire format.

## Renderer

The bundled renderer uses a Korean-specific source/filter voice with continuous
source and coefficient smoothing, overlapping phone gestures, multiple voiced
and noise resonators, nasal poles and antiresonances, and explicit stop stages.
It streams PCM through a caller callback. Output adapters are separate libraries,
so the core never knows about WAV files, I2S, DACs, or PWM hardware.

## Compile-time removal

The following modules are independently removable:

```cmake
-DNANOTTS_ENABLE_NORMALIZATION=OFF
-DNANOTTS_ENABLE_MORPHOLOGY=OFF
-DNANOTTS_ENABLE_BUILTIN_LEXICON=OFF
-DNANOTTS_BUILD_PCM=OFF
-DNANOTTS_BUILD_PWM=OFF
-DNANOTTS_USE_LIBM=OFF
```

No runtime dispatch is paid for a module that is compiled out.
