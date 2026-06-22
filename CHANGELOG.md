# Changelog

## 1.2.0-ko

- Replaced direct-form-I filter state with transposed direct-form II for a
  smaller, smoother time-varying resonator implementation.
- Added per-sample de-zippering for vocal-tract, noise, nasal, source, and gain
  parameters while retaining 1 ms target planning.
- Added stable high-rate interpolation toward newly designed biquad coefficients
  (every two samples by default) instead of replacing them at control boundaries.
- Added raised-cosine fade edges only where a phone touches real silence,
  preserving connected-phone coarticulation while eliminating hard cuts.
- Clear hidden source and resonator history inside pauses so a later accent
  phrase cannot reveal stale filter energy.
- Replaced the abrupt tap closure gate with a smooth closure/release curve.
- Added a de-click regression test that checks every long-silence edge.
- Added before/after transition diagnostics and MCU playback guidance.

## 1.1.0-ko

- Raised the full-quality acoustic control rate from 4 ms to 1 ms; the compact
  setup profile defaults to 2 ms to reduce MCU CPU cost.
- Rebalanced the renderer around periodic vowel/sonorant energy instead of
  broadband release and frication energy.
- Added four voiced detail resonators alongside the four-resonator cascade.
- Added two nasal poles and two place-specific antiresonances for `ㅁ/ㄴ/ㅇ`.
- Added coda-sonorant coloring, adjacent-vowel nasal carryover, and intervocalic
  `ㅎ` lenition.
- Reworked lax, tense, and aspirated stop/affricate envelopes in milliseconds,
  including closure, burst, aspiration, source quality, and onset-F0 behavior.
- Moved obstruent-conditioned F0 cues onto the following vowel, including when
  a `ㅑ/ㅕ/ㅛ/ㅠ` or `ㅘ/ㅝ/...` glide intervenes.
- Added mild glottal jitter and shimmer, source spectral-tilt control, and
  smoother phrase declination and final contours.
- Retuned Korean vowels from published modern Seoul measurements; kept `애/에`
  deliberately close while preserving distinct phone IDs.
- Added a moving `/ɯi/` phone for initial `의`.
- Added context-sensitive onset, vowel, glide, coda, AP-final, and phrase-final
  timing.
- Expanded the compact pronunciation lexicon with standard lexical
  tensification and complex-coda forms.
- Added a 76-pair pronunciation regression suite and expanded the smoke corpus
  from 37 to 57 prompts.
- Increased the full public context to continue fitting 4096 bytes; measured
  internal use is 3584 bytes on the validation host.

## 1.0.0-ko

- Refactored NanoTTS into a Korean-only product.
- Removed multilingual selection, language registry, and IPA runtime.
- Added arithmetic Hangul/Jamo decoding and a bounded Korean G2P pipeline.
- Added liaison, palatalization, coda clusters, `ㅎ` aspiration, final
  neutralization, nasalization, lateralization, and tensification.
- Added a Korean-only phone inventory with lax, tense, and aspirated series.
- Added unreleased coda stop events.
- Added Korean-specific source/filter acoustics and accentual-phrase prosody.
- Added internal question and comma-continuation contours.
- Added Korean number and Latin-letter expansion.
- Added an optional compact exception lexicon and application lexicon hook.
- Retained modular PCM16LE and timer-PWM output adapters.
- Added compact/no-`libm` MCU profiles, regression tests, randomized parser
  testing, examples, and Korean-specific documentation.
