# Korean frontend

## Scope

The frontend targets compact, modern Seoul-oriented Korean pronunciation. It is
rule based, deterministic, and does not require a morphological analyzer or a
large runtime dictionary.

## Hangul decoding

Precomposed syllables are decomposed arithmetically from Unicode's Hangul
syllable block into onset, vowel, and optional coda indices. Modern conjoining
Jamo (`U+1100` series) are also accepted. Compatibility Jamo are intentionally
not normalized in the core because general Unicode normalization would add
code and tables.

## Processing order

The frontend stores four bytes per syllable and applies bounded in-place passes:

1. Vowel-initial liaison and palatalization.
2. Complex final-consonant resolution.
3. `ㅎ` aspiration and deletion behavior.
4. `ㄴ`/`ㄹ` lateral and nasal interactions.
5. Seven-way final neutralization.
6. Nasal assimilation.
7. Post-coda tensification.
8. Phone-event, timing, and accentual-phrase generation.

Representative productive cases include:

```text
국민  -> 궁민-like phone sequence
신라  -> 실라
먹다  -> 먹따
학교  -> 학꾜
꽃이  -> 꼬치
많다  -> 만타
설날  -> 설랄
한 일 -> 한 닐 in connected speech
```

Sound changes may cross a space only when `NANOTTS_OPT_LINK_EOJEOL` is enabled.
That flag is on by default for one-call synthesis. Clear it for careful,
word-separated pronunciation.

## Lexical pronunciation

Some Korean tensification, `ㄴ` insertion, and complex-coda outcomes require
lexical or morphological knowledge. The removable built-in lexicon covers a
compact high-value set, including forms such as:

```text
서울역 -> 서울력
꽃잎   -> 꼰닙
식용유 -> 시굥뉴
갈등   -> 갈뜽
발전   -> 발쩐
넓게   -> 널께
떫지   -> 떨찌
```

`tests/data/ko_pronunciations.tsv` contains 76 written/pronunciation pairs. The
application lexicon is checked before the built-in table and can override any
word.

## Codas

Surface codas are reduced to the Korean seven-coda system:

```text
/k̚ n t̚ l m p̚ ŋ/
```

The renderer has dedicated silent closure events for `/k̚ t̚ p̚/`; these do
not emit a release burst.

## Vowels and glides

The acoustic inventory distinguishes:

```text
a, eo, o, u, eu, i, ae, e, y, w, ui
```

Compound Hangul vowels are represented as glide-plus-vowel sequences. Modern
`ㅚ` is rendered as `w + e`. Initial `의` uses the moving `ui` nucleus, whose
formants travel from `/ɯ/` toward `/i/`; after a consonantal onset, `ㅢ` defaults
to `i` in this compact modern-oriented policy.

## Timing

The frontend applies context-specific duration percentages rather than leaving
all phones at table defaults:

- AP-initial and medial lax/tense/aspirated onsets differ;
- glides are shorter than full vowels;
- vowels in closed syllables are slightly shorter;
- AP-final and phrase-final vowels/codas are lengthened;
- coda sonorants are shorter and darker than onsets;
- automatic AP pauses can be disabled.

## Accentual phrases

Text is grouped into bounded accentual phrases using syllable and word limits.
The first onset selects a low-initial or high-initial pattern; tense,
aspirated, fricative, and `ㅎ` onsets begin high. Targets are stored in the
existing event records, so the renderer does not inspect Korean orthography.

Commas create continuation boundaries. Question marks place a rise on the
corresponding phrase boundary, including internal questions followed by more
text.

## Numbers and Latin letters

Unsigned decimal integers are expanded through `조`. A leading-zero sequence is
read digit by digit, which suits identifiers such as `007` or `010`. ASCII
letters are spoken using Korean letter names.

This is not a complete Korean text-normalization system. Dates, decimal points,
currency, units, telephone grouping, counters, and context-sensitive native
number forms should be normalized by the application when exact wording
matters.

## Application lexicon

Overrides return a UTF-8 Hangul pronunciation spelling:

```c
static int lookup(
    void *user,
    const char *word,
    size_t word_bytes,
    const char **spoken,
    size_t *spoken_bytes)
{
    if (matches(word, word_bytes, "제품명")) {
        *spoken = "원하는발음";
        *spoken_bytes = NANOTTS_NPOS;
        return 1;
    }
    return 0;
}
```

The callback runs once per Hangul token, never in the DSP loop.
