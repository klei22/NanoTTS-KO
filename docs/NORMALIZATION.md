# Korean text normalization

NanoTTS-KO 2 includes a bounded semantic normalizer intended for device prompts,
measurements, status messages, and ordinary numeric text.

## Supported forms

| Input class | Example | Typical spoken plan |
|---|---|---|
| Integer | `2048` | Sino-Korean cardinal |
| Leading-zero digits | `007` | digit by digit |
| Native counter | `12시`, `12명`, `12개` | native Korean under 100 |
| Sino counter/unit | `12분` | Sino-Korean |
| Time | `12:30` | hour/minute form |
| Date | `2026-06-24` | year/month/day form |
| Decimal | `3.5` | integer, `점`, digits |
| Fraction | `1/3` | denominator `분의` numerator |
| Percentage | `85%` | number plus `퍼센트` |
| Signed number | `-12` | `마이너스` plus number |
| Grouped number | `1,000` | grouping comma ignored |
| Telephone-like string | `010-1234-5678` | digit sequence |
| Version/IP-like string | `1.2.3.4` | components separated by `점` |
| Currency | `₩5000` or `5000₩` | number plus `원` where tokenized |
| Temperature | `24℃` | number plus `도` |
| Latin acronym | `PWM` | Korean letter names |
| Measurement unit | `3.3V`, `16kHz`, `4MB` | number plus Korean unit name |

Recognized Latin unit symbols currently include:

```text
kg mg g
km cm mm m
Hz kHz MHz GHz
V mV A mA W kW
KB MB GB
```

## Context-sensitive numbers

Numbers before supported counters may use native Korean forms:

```text
12시  -> 열두 시
12명  -> 열두 명
12개  -> 열두 개
```

Ordinary quantities and Sino counters remain Sino-Korean:

```text
12분  -> 십이 분
12원  -> 십이 원
```

The counter table is intentionally bounded. Applications that require legal,
financial, medical, or domain-specific verbalization should normalize those
forms explicitly or extend the table.

## Punctuation isolation

A numeric scanner accepts punctuation only when it occurs between digits. A
sentence-final period or comma is therefore not consumed by the number token:

```text
"1,000." -> number token `1,000` + declarative boundary
```

Dates are chosen only when the token has a valid year-month-day shape. Other
hyphenated numeric sequences fall back to digit reading.

## Disabling normalization

At runtime:

```c
options.flags |= NANOTTS_OPT_NO_NORMALIZATION;
```

At build time:

```bash
./setup.sh --no-normalization
```

With normalization disabled, numeric material is read conservatively as digits
and the semantic tables are removed from the binary.

## Known limitations

The bundled normalizer does not attempt unrestricted Korean text understanding.
It does not currently provide complete handling for:

```text
arbitrary mathematical expressions
all Korean counters and classifier preferences
financial wording and cheque-style numbers
full URL/email verbalization
Hanja number contexts
English word pronunciation
time zones and locale-specific date ambiguity
all SI prefixes and compound units
```

For critical prompts, pass already-normalized Korean text. The library keeps
normalization separate from pronunciation so application policy remains in
control.
