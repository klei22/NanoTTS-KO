# Korean prosody and speaking styles

NanoTTS-KO plans prosody after morphology and surface phonology. The renderer
receives continuous pitch endpoints and durations; it does not inspect text or
Korean spelling.

## Prosodic units

The compact planner models:

```text
word/morpheme attachment
accentual phrase (AP)
comma/continuation phrase
sentence-final phrase
```

AP grouping uses POS and morphology cues first, then bounded length limits.
Particles, endings, counters, and `KEEP_WITH_NEXT` morphology are kept with
nearby lexical material. Strong endings and punctuation create phrase edges.

This is more accurate than the previous fixed word/syllable grouping, but it is
not a full syntactic parser or K-ToBI annotation system.

## Accentual-phrase contours

The first onset selects a low-initial or high-initial contour family. Tense,
aspirated, sibilant, and `ㅎ` onsets begin high; other onsets generally begin
low. Continuous start/end targets are interpolated across every event rather
than applying one pitch value to an entire phone.

## Clause-local modality

The planner attaches modality to the clause-ending token:

```text
yes/no question
wh-question
command/request-like ending
statement
continuation
```

An early question mark therefore does not force a later declarative clause to
rise. Wh-question words are marked separately from yes/no question endings and
receive a different final contour.

The caller can override the final contour with `options.final_tone`.

## Styles

| Style | Main intent |
|---|---|
| `prescriptive` | conservative standard variants and moderate timing |
| `modern-seoul` | default contemporary Seoul-oriented profile |
| `clear-device` | slower vowels/consonants, longer boundaries, stronger prominence |
| `conversational` | shorter timing, weaker boundaries, more common reductions |
| `learner` | slowest, clearest segmentation and strongest prominence |

Select a style before text parsing because it affects pronunciation variants as
well as rendering:

```c
nanotts_options_t options;
nanotts_default_options(&options);
options.style = NANOTTS_KO_STYLE_CLEAR_DEVICE;
```

CLI:

```bash
nanotts --style clear --text "경고. 배터리가 부족합니다." -o alert.wav
```

## Expression control

`expression_q8` scales pitch and prominence range without changing the chosen
pronunciation:

```text
128 = 100%
```

CLI values range from 45% through 165%:

```bash
nanotts --expression 115 --text "정말 괜찮아요?" -o expressive.wav
```

## Focus

Question words and selected focus flags receive greater prominence. The public
event representation already has a focused acoustic bit, allowing applications
or future frontends to set explicit focus without changing the renderer.

## Limitations

The planner does not infer arbitrary discourse focus, sarcasm, emotion,
quotation scope, or deep syntax. Sentence-ending morphology and punctuation
provide useful cues, but native review is still required for public-facing or
safety-critical prosody.
