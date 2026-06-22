# Quality improvements in 1.1 and 1.2

NanoTTS-KO 1.1 is a deliberately larger Korean quality release. Its goal is
not to imitate a neural voice; it is to make a small rule/formant engine carry
more of the acoustic information Korean listeners use to distinguish syllables
and words.


## Transition-quality work in 1.2

Version 1.1 increased acoustic detail but still changed a large set of
source/filter parameters at discrete control boundaries. It also wrote exact
zero throughout pauses, which could expose a hard cut when the preceding phone
had not reached zero. These mechanisms can sound like ticks, zippering, or
jagged joins even when the phone sequence is correct.

Version 1.2 adds four complementary controls:

1. Per-sample smoothing of source and gain controls.
2. High-rate movement of biquad coefficients toward each 1 ms target, every
   two samples by default and every four in the compact setup profile.
3. Raised-cosine edges only for phones adjacent to actual silence.
4. Safe source/filter-state clearing while a pause already masks the output.

The biquads now use transposed direct-form II state, and the tap closure is
shaped continuously. This avoids solving clicks by globally low-pass filtering
the final output, which would also remove useful Korean frication and release
cues.

On the representative 24 kHz demonstration phrase, the mean absolute sample jump at phone boundaries fell from 0.06593 to
0.01206, the 90th percentile fell from 0.15017 to 0.02107, and the largest
amplitude touching a long exact-silence run fell from 0.06610 to 0.00616. On a
tap/liquid-heavy probe, the corresponding long-silence edge fell from 0.11749
to 0.00903 and the 99.9th-percentile sample step fell from 0.19861 to 0.15991.
These are engineering diagnostics, not a human preference or intelligibility
score.

## Problems in 1.0

Engineering inspection of the 1.0 renderer identified several high-impact
limitations:

1. One static acoustic target was used for most of a phone.
2. Coefficients changed only every 4 ms.
3. Broadband stop/fricative energy often dominated weak vowel structure.
4. The lax/tense/aspirated F0 cue was largely attached to the consonant event,
   even though an unvoiced event cannot directly carry audible F0.
5. A glide could separate the onset from the vowel and lose that cue.
6. Nasal place depended mainly on oral formants and one generic nasal filter.
7. `ㅢ` was two disconnected phones rather than a continuous moving nucleus.
8. Onsets, codas, medial phones, and phrase-final phones reused overly similar
   timing.
9. Several standard lexical tensification and complex-coda forms were absent.

## Renderer changes

### Higher trajectory resolution

The full build plans new acoustic targets every 1 ms and moves resonator
coefficients toward those targets every two audio samples. The compact setup
profile plans every 2 ms and uses a four-sample coefficient stride to reduce MCU
CPU demand. F0 and source/gain controls are interpolated within each block.

### Stronger voiced structure

The renderer uses four cascade resonators plus four parallel broad detail
resonators. This gives vowels and sonorants controlled F1-F4 structure without
requiring extremely high cascade gain. Source spectral tilt and a
voiced-dependent radiation shelf are tuned to keep speech bright enough for
clarity without allowing every consonant to become a full-band noise block.

### Korean three-way contrast

Lax, tense, and aspirated stops/affricates have separate staged envelopes:
closure, release, aspiration, source onset, and transition. The following vowel
receives the onset-conditioned F0 and phonation cue. A backward syllable-onset
search preserves it through `y` and `w` glides.

### Korean vowel system

The eight vowel targets were retuned into one coherent modern-Seoul-oriented
male synthetic voice. `애/에` remain close. The initial `의` phone continuously
moves from `/ɯ/` to `/i/`.

### Nasals, liquids, and codas

Two nasal poles and two antiresonances provide distinct `ㅁ/ㄴ/ㅇ` patterns.
Adjacent vowels receive a short nasal transition. Coda sonorants are darker and
shorter than onsets. Stop codas remain unreleased and communicate place through
the preceding vowel transition.

### Timing and prosody

Phone durations now depend on AP position, medial versus initial onset,
obstruent series, closed syllable, coda status, and phrase-final position.
Accentual-phrase targets, onset perturbations, phrase declination, and final
statement/question/continuation contours are combined without consulting text
inside the DSP loop.

## Frontend changes

- Added a moving initial `ㅢ` event.
- Expanded standard lexical tensification examples.
- Expanded complex-coda exceptions.
- Added 76 written/pronunciation regression pairs.
- Expanded the end-to-end smoke corpus to 57 prompts.

The built-in lexicon is intentionally small and removable. Product names,
people, places, and critical domain vocabulary should still use the application
lexicon callback.

## Objective evaluation

The release evaluation renders 30 matched 24 kHz prompts from both 1.0 and 1.1.
No prompt clips in either set. Version 1.1 increases mean active RMS from 0.0793
to 0.1261 while reducing power above 3 kHz from 21.60% to 5.42% and reducing
the 99.9th percentile sample-to-sample difference from 0.3660 to 0.2967.

These shifts are consistent with a signal that has stronger periodic
vowel/sonorant structure and less globally dominant broadband release noise.
They are diagnostics, not listener scores.

A separate onset-conditioned F0 test gives the intended ordering in generated
first-vowel windows:

```text
lax       107.1 Hz
 tense     134.9 Hz
 aspirated 141.3 Hz
```

## Recommended listening test

For a meaningful human result, use at least several fluent/native Korean
listeners and randomize the version labels. Suggested tasks:

1. **Uncued transcription**: listeners type exactly what they hear.
2. **Minimal-contrast identification**: lax/tense/aspirated and nasal/coda
   choices.
3. **A/B preference**: which rendering is easier to understand, with a
   “no preference” option.
4. **Prompt suitability**: whether an embedded alert can be understood on the
   intended small speaker at the intended listening distance.

Report word or syllable error rate separately from naturalness. A voice can be
robotic yet intelligible, and a pleasant signal can still collapse important
Korean contrasts.

## Remaining quality limits

- No morphological analyzer or large pronunciation dictionary.
- Simplified handling of regional and speech-style variation.
- Synthetic release spectra rather than recorded residuals.
- Approximate microprosody and phrase grouping.
- One built-in voice identity.
- No native-listener score yet.

The next large quality step would be an optional MIT/CC0 Korean LPC or
micro-unit voice backend sharing the existing frontend and event stream. That
would cost hundreds of kilobytes rather than tens of kilobytes but could retain
the same MCU-oriented API.
