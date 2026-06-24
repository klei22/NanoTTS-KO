# Korean linguistic frontend

## Scope

NanoTTS-KO 2 uses a compact morphology-aware Korean frontend. It is designed to
capture high-value pronunciation distinctions within a bounded MCU-friendly
implementation; it is not a replacement for a full desktop morphological
analyzer and large pronunciation dictionary.

## Hangul decoding

Precomposed Hangul syllables are decomposed arithmetically into onset, vowel,
and coda indices. Modern conjoining Jamo are accepted. Compatibility Jamo are
not generally normalized because a full Unicode normalization table would add
substantial data.

The frontend retains both:

```text
lexical onset/coda   original spelling information
surface onset/coda   mutable pronunciation result
```

This prevents early coda neutralization from erasing information needed by
later morphology-conditioned rules.

## Token and morpheme metadata

Each token stores its source byte range, normalized syllable span, boundary,
type, and analysis flags. Each morpheme stores:

```text
syllable span
token index
part of speech
morphology flags
```

Recognized high-value categories include nouns, proper nouns, verb/adjective
stems, endings, particles, suffixes, counters, numbers, and foreign material.

## Important contextual distinctions

### `신고`

```text
신을 신고   -> verbal 신-고; ending onset fortifies
혼인 신고   -> noun 신고; onset remains lax
```

The compact analyzer uses object-particle and token context. Applications with
more ambiguous syntax should provide a lexicon override.

### Passive/causative versus nominalized `-기`

```text
안기다 -> passive/causative suffix; no fortition
안기   -> nominalizer; fortition applies
```

The same mechanism covers a bounded set of common stems and is extensible.

### Attributive `-(으)ㄹ`

The analyzer recognizes both separate-syllable `-을` and coda `-ㄹ` forms when
a plausible following noun is present:

```text
먹을 것
할 수
만날 사람
갈 곳
```

This avoids globally interpreting every final `ㄹ` or `을` as an attributive
ending.

## Ordered surface rules

The main passes are:

1. Morphology-conditioned fortition.
2. Compound/contextual `ㄴ` insertion candidates.
3. Liaison and resyllabification.
4. Palatalization.
5. Complex-coda selection.
6. `ㅎ` aspiration, weakening, and style-dependent deletion.
7. Lateralization.
8. Seven-coda neutralization.
9. Nasal assimilation.
10. Obstruent-conditioned fortition.
11. Contextual phone/allophone expansion.

The built-in regression corpus includes 133 written/pronunciation pairs covering
productive rules and lexical outcomes.

## Productive versus lexical behavior

Some changes are productive and handled by rules:

```text
국밥   -> 국빱
꽃이   -> 꼬치
국민   -> 궁민
신라   -> 실라
많다   -> 만타
할 수  -> 할쑤
```

Other classes are lexically restricted and remain dictionary entries:

```text
발전   -> 발쩐
서울역 -> 서울력
문고리 -> 문꼬리
아침밥 -> 아침빱
꽃잎   -> 꼰닙
```

A key negative contrast is preserved:

```text
여덟도 -> 여덜도
```

Noun `ㄼ` does not receive the verb-only fortition used in forms such as
`넓게`.

## Vowel and glide variants

Styles can choose among permitted or common realizations:

```text
particle 의
noninitial 의
ㅒ/ㅖ variants
modern ㅐ/ㅔ merger policy
맛있다 / 멋있다 prescriptive versus modern variants
```

Initial `의` uses a moving `/ɯi/` event in careful styles. Glides are emitted as
trajectories rather than full static vowels.

## Sibilant allophones

`ㅅ` and `ㅆ` before `/i/` and y-glides receive explicit palatalized phone IDs.
This allows separate noise spectra and prevents tests or voice data from
pretending that all Korean sibilants are acoustically identical.

## Application lexicon

The extended callback receives style and can attach morphology flags:

```c
static int lookup_ex(
    void *user,
    nanotts_ko_style_t style,
    const char *word,
    size_t word_bytes,
    const char **spoken,
    size_t *spoken_bytes,
    uint16_t *morph_flags);
```

Return a Hangul pronunciation spelling. The text is parsed immediately; the
library does not retain the returned pointer.

Use this for proper names, product names, Hanja-derived vocabulary, specialized
compounds, and preferred regional variants.

## Remaining linguistic limitations

The built-in analyzer does not provide unrestricted lemma recovery, Hanja
reading, full Korean compound segmentation, statistical parse ranking, English
loanword G2P, or a large proper-name dictionary. Ambiguous inputs can therefore
still require an override or host-side normalization.
