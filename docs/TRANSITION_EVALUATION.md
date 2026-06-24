# NanoTTS-KO 1.3.0 connected-transition report

## Target problem

The written phrase `감사합니다` is converted to the intended event sequence:

```text
/k a m s a h a m n i d a/
       ^         ^
       m->s      m->n
```

The phrase did not require a pronunciation exception. The defects were in the
acoustic join:

- the nasal and fricative were independently windowed at `ㅁ -> ㅅ`;
- frication entered too close to full strength;
- filter paths could retain inaudible state and reveal it when their gain rose;
- `ㅁ -> ㄴ` used two nearly full isolated nasal durations;
- nasal poles, zeros, and neighboring-vowel transitions moved too quickly.

## General renderer changes

Version 1.3 does not patch `감사합니다` specifically. It adds a general
connected-phone layer:

1. Pair-specific overlap windows.
2. A shared source/filter midpoint approached from both phones.
3. Continuous voiced envelopes for connected sonorants.
4. Gradual frication and aspiration attack/release.
5. Zero drive for inactive filter paths.
6. A short sample-domain residual bridge with pair-specific slope limits.
7. General duration overlap for adjacent Korean nasal events.
8. Lower-noise affricate tails into following sonorants and vowels.

The overlap classes cover nasal/nasal, nasal/sonorant, sonorant/sonorant,
nasal/fricative, sonorant/fricative, affricate/sonorant, stop-like, and other
connected pairs.

## `감사합니다` diagnostics

All values use normalized signed 16-bit PCM at 16 kHz.

| Join | Diagnostic | 1.2.0 | 1.3.0 |
|---|---|---:|---:|
| `ㅁ -> ㅅ` | Exact boundary step | 0.01779 | 0.00894 |
| `ㅁ -> ㅅ` | Maximum derivative within +/-2 ms | 0.18729 | 0.01114 |
| `ㅁ -> ㅅ` | Five-millisecond RMS mismatch | 3.61 dB | 0.77 dB |
| `ㅁ -> ㅅ` | Six-millisecond spectral distance | 0.86258 | 0.50033 |
| `ㅁ -> ㄴ` | Maximum derivative within +/-2 ms | 0.01697 | 0.01343 |
| `ㅁ -> ㄴ` | Six-millisecond spectral distance | 0.22952 | 0.17412 |
| `ㅁ -> ㄴ` | Local envelope-slope diagnostic | 8.47 dB | 6.04 dB |
| `ㅁ -> ㄴ` | Combined nominal duration | 132.6 ms | 115.3 ms |

The old `ㅁ -> ㄴ` exact sample happened to land near a zero crossing, so its
single-sample value was already small. The useful improvements there are the
shorter overlapped cluster, reduced spectral step, and reduced envelope change.

## Broader transition corpus

The same 58 Korean inputs were rendered by 1.2 and 1.3. After excluding pause
boundaries, the analysis contained 346 connected joins.

| Diagnostic | 1.2.0 | 1.3.0 | Change |
|---|---:|---:|---:|
| Mean exact boundary step | 0.02282 | 0.00434 | -81.0% |
| 90th-percentile exact step | 0.04907 | 0.00601 | -87.7% |
| 99th-percentile exact step | 0.20463 | 0.00919 | -95.5% |
| Maximum exact step | 0.26395 | 0.01199 | -95.5% |
| Mean maximum derivative within +/-2 ms | 0.07401 | 0.02456 | -66.8% |
| Mean spectral distance | 0.48893 | 0.28026 | -42.7% |
| 90th-percentile spectral distance | 0.99914 | 0.55732 | -44.2% |

The 58 rendered outputs were non-empty, valid mono PCM, and contained no
clipped samples.

## Regression test

`tests/test_transitions.c` now verifies that `감사합니다`:

- retains the expected phone sequence;
- uses generalized adjacent-nasal duration overlap;
- stays under exact and local derivative limits at both target joins;
- avoids a sudden five-millisecond energy collapse or surge.

The test runs in full, compact, no-`libm`, and no-lexicon configurations.

## Remaining transition risks

The following are not fully solved:

- A compact formant renderer still sounds mechanical even when joins are
  sample-continuous.
- Korean `ㅁ/ㄴ/ㅇ` place cues use a small pole/zero model rather than
  recorded transitions.
- The adjacent-nasal duration factors are engineering defaults and still need
  native-listener tuning.
- Stop closure, tense release, aspiration, and affrication intentionally create
  large changes over several milliseconds. Globally smoothing those edges
  would reduce Korean consonant contrast.
- A PWM, DAC, I2S, DMA, amplifier, or speaker path can add clicks not present
  in the generated PCM.
- Human Korean transcription and preference testing remains necessary.

I cannot provide a human auditory judgment. The release therefore includes
random-access A/B WAV files, waveform plots, a spectrogram, per-boundary CSV
data, and deterministic tests so a Korean listener can judge the actual
perceptual result.

## Footprint

GCC 14.2 `-Os` x86-64 object-section measurements:

| Build | Code and initialized data |
|---|---:|
| Full Korean core | 32,609 B |
| Compact no-`libm` core | 32,734 B |
| Full core without built-in lexicon | 30,450 B |
| PCM adapter | 349 B |
| PWM adapter | 445 B |

The full core increased by about 3.25 KB over 1.2. Public context sizes remain
4,096 bytes for the full profile and 3,072 bytes for the compact profile.
Target MCU linker maps remain the source of truth.
