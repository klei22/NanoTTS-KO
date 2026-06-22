# Korean acoustic renderer

## Design goal

The renderer maximizes Korean intelligibility and contrast per unit of flash,
RAM, and CPU. It is an original source/filter implementation, not a port of
another synthesizer or a set of copied voice tables.

## Signal path

```text
band-limited glottal source -> four cascade resonators ---------┐
                             -> four parallel detail resonators -+-> radiation
noise source -> three place/frication filters ------------------+-> DC blocker
             -> aspiration path --------------------------------+-> limiter -> PCM
voiced source -> two nasal poles -> two antiresonances ----------┘
```

The full-quality build plans acoustic targets every 1 ms. `setup.sh --compact`
defaults to 2 ms unless `--control-ms` is supplied. Version 1.2 no longer
replaces those targets or biquad coefficients as a block: source and gain parameters are de-zippered per sample, and every filter moves
in small high-rate steps toward the next stable coefficient set. The default
coefficient stride is two samples; the compact profile uses four. The per-sample loop
contains no language rules, allocations, trigonometry, or division.


## Transition and click control

Digital clicks were previously possible at three locations: a phone-to-pause
hard cut, a coefficient replacement while a resonator held nonzero history, and
a new phrase revealing stale source/filter state. Version 1.2 addresses each
location directly:

- phones touching actual silence use a 5 ms attack and 7 ms release edge;
- connected phones remain overlapped and are blended through continuous
  parameter and coefficient motion;
- resonators use transposed direct-form II state;
- source and filter history is cleared only while a pause already masks it;
- the Korean tap closure uses a smooth gate rather than a binary switch.

This is not a broadband output low-pass filter, so fricative and stop-release
energy is retained. The additional cost is arithmetic in the sample loop rather
than language data or persistent context memory.

## Source model

The voiced source uses a compact asymmetric glottal pulse with configurable
open quotient, return phase, and spectral tilt. Mild deterministic cycle jitter
and shimmer reduce perfectly stationary buzz without requiring stored waveform
samples.

Source behavior can carry over briefly from a syllable onset:

- lax obstruents give a breathier, lower-F0 vowel onset;
- tense obstruents give a tighter, more pressed onset;
- aspirated obstruents give the longest aspiration and highest initial F0;
- intervocalic lax obstruents can carry weak voicing;
- intervocalic `ㅎ` is shortened and weakened relative to phrase-initial `ㅎ`.

## Korean three-way obstruent contrast

Lax, tense, and aspirated stops and affricates are separate acoustic kinds.
Their envelopes are staged in milliseconds rather than as a fixed fraction of
the whole phone:

```text
closure -> burst -> aspiration / delayed voicing -> vowel transition
```

Tense releases are compact with minimal aspiration. Lax releases are
intermediate and may become partly voiced medially. Aspirated releases have the
longest noise tail. The onset-conditioned F0 cue is applied to the following
vowel; a syllable search preserves it across an intervening `y` or `w` glide.

## Vowels

The voice has targets for:

```text
/a ʌ o u ɯ i ɛ e/
```

`애` and `에` are intentionally close for a modern Seoul-oriented voice while
remaining separate phone IDs. Four cascade formants carry the vocal-tract
shape, and four broad detail resonators reinforce F1 through F4 without forcing
all energy through a high-gain cascade.

Initial `의` uses a single moving nucleus whose formants transition from `/ɯ/`
toward `/i/`. Other compound vowels use short `y` or `w` glides followed by the
vowel target.

## Coarticulation

Only immediate neighboring events influence a phone. Vowels move toward
bilabial, coronal, palatal, velar, or glottal loci at their edges. This avoids
smearing formants across an intervening obstruent. A following-vowel target also
controls the endpoint of a glide rather than leaving every glide with one fixed
endpoint.

## Codas

`k-coda`, `t-coda`, and `p-coda` are silent unreleased closures. Their place is
carried by the final transition out of the preceding vowel; no English-style
release burst is added.

Coda nasals and laterals receive darker, shorter settings than comparable
onsets. Phrase-final codas and their preceding vowels can be lengthened by the
frontend.

## Nasals and liquids

`m`, `n`, and `ng` use separate formant targets, two low nasal resonances, and
two place-dependent antiresonances. Short nasal carryover into adjacent vowels
helps distinguish place while keeping the filter bank compact.

Onset/intervocalic `ㄹ` is a brief tap. Surface lateral and coda `ㄹ` use the
lateral event, with coda coloring applied acoustically.

## Prosody

The frontend places accentual-phrase pitch targets directly into four-byte
events. The renderer interpolates those targets and adds:

- gentle phrase declination;
- a late statement fall;
- a late question rise;
- a smaller continuation rise;
- the onset-conditioned F0 perturbation described above.

F0 is interpolated within every acoustic control block even though filter
coefficients are block-updated.

## Output protection

A DC blocker, voiced-dependent radiation shelf, and soft limiter provide output
headroom without flattening ordinary vowels. The final validation corpus has no
integer clipping at the default volume.

## Limitations

This remains a formant voice. It does not reproduce a natural speaker's
microprosody, detailed consonant release spectra, expressive timing, or every
regional realization. A larger optional LPC or sampled micro-unit backend could
reuse the same Korean frontend and event stream in a future release.
