# Changelog

## 2.0.0-ko

- Replaced the spelling-only frontend with statically bounded token, morpheme,
  POS, and morphology metadata.
- Added semantic normalization for native/Sino numbers, counters, dates, times,
  decimals, fractions, percentages, telephone-like strings, dotted versions,
  currency, temperature, Latin acronyms, and common MCU units.
- Added morphology-conditioned readings for verbal versus nominal `신고`,
  passive/causative versus nominalized `-기`, and attributive `-(으)ㄹ`.
- Reordered phonology so morphology-sensitive fortition runs before liaison,
  complex-coda reduction, and coda neutralization.
- Corrected overgeneralization of verb-only fortition to noun clusters such as
  `여덟도`.
- Expanded the independently authored pronunciation regression corpus from 76
  to 133 pairs, including the standard Rule 23-28 examples and lexical compound
  fortition.
- Added prescriptive, modern-Seoul, clear-device, conversational, and learner
  pronunciation/prosody profiles.
- Added clause-local yes/no, wh-question, command, statement, and continuation
  contours; question modality no longer leaks into a later clause.
- Expanded acoustic events from four to eight bytes with continuous pitch
  endpoints, prominence, and Korean allophone/prosody flags.
- Added explicit palatalized lax and tense sibilants, weak-`ㅎ` and optional
  phrase-initial denasalization cues.
- Added extended lexicon callbacks with style and morphology information.
- Added 60,000 randomized Hangul and mixed-text parser cases and dedicated
  morphology, normalization, style, and prosody tests.
- Increased the full and compact static profiles to hold the richer linguistic
  representation; normalization, morphology, and the built-in lexicon remain
  independently removable.

## 1.3.0-ko

- Reworked connected-phone joins around a shared acoustic midpoint instead of
  treating every event boundary as an instantaneous source/filter switch.
- Added pair-specific overlap windows for nasal, sonorant, fricative,
  affricate, and stop transitions.
- Removed artificial attack/release dips between continuously voiced
  sonorants.
- Added gradual frication and aspiration onset/offset envelopes, including
  lower-noise affricate tails into following vowels.
- Prevented inactive voiced and noise filter paths from accumulating hidden
  state that could reappear as a transient.
- Added a short sample-domain boundary bridge with pair-specific slope limits;
  it corrects residual filter-state mismatch while preserving stop releases.
- Added generalized adjacent-nasal duration overlap, improving sequences such
  as `ㅁㄴ` in `합니다` without a phrase-specific pronunciation exception.
- Added regression coverage for the `ㅁ→ㅅ` and `ㅁ→ㄴ` joins in
  `감사합니다`, plus a transition-heavy corpus analysis.

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
