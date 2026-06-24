# Connected-phone transitions

> NanoTTS-KO 2.0 retains this 1.3 renderer behavior beneath the new linguistic frontend.

NanoTTS-KO 1.3 treats an event boundary as the center of an overlapping
articulatory transition rather than as an instantaneous acoustic switch.

This change was prompted by two audible joins in `감사합니다`:

```text
감사       /kam.sa/       ㅁ -> ㅅ
합니다     /ham.ni.da/    ㅁ -> ㄴ
```

The Korean frontend already produced the intended phone sequence. The defect
was in the renderer: independent phone envelopes, rapidly changing nasal and
fricative filters, and dormant filter history could create a dip or a short
transient at the join.

## Signal path in 1.3

For each connected pair, the renderer now:

1. Computes the acoustic endpoint of the left phone and the start point of the
   right phone.
2. Builds one shared midpoint from source, formant, bandwidth, nasal, noise,
   aspiration, and gain parameters.
3. Approaches that midpoint from both sides over a pair-specific overlap
   interval.
4. Ramps frication and aspiration rather than enabling them at full strength.
5. Drives inactive filter paths with zero so they cannot accumulate hidden
   state.
6. Applies a short, bounded residual bridge in the sample domain if filter
   memory still leaves a mismatch.

The bridge predicts the next sample from the preceding slope, limits that
slope according to the phone classes, and decays the correction with a smooth
curve. It is deliberately short: the acoustic model remains responsible for
the transition, while the bridge removes only the residual discontinuity.

## Pair classes

The transition window is selected once per event pair:

| Pair | Typical overlap |
|---|---:|
| Nasal to nasal | 17 ms |
| Nasal to another sonorant | 14 ms |
| Other sonorant to sonorant | 11 ms |
| Nasal to fricative | 12 ms |
| Other sonorant to fricative | 10 ms |
| Affricate to sonorant | 8 ms |
| Any stop-like boundary | 2.5 ms |
| Other connected phones | 5-7 ms |

Stops retain short windows because closure and release are meaningful cues.
Using one long crossfade for every pair would blur Korean stop and affricate
contrasts.

## Adjacent nasal timing

Adjacent Korean nasals are not rendered as two full isolated consonants.
For a connected nasal cluster, the frontend shortens the coda and following
onset while the renderer moves continuously between their place-dependent
nasal poles, zeros, and neighboring-vowel transitions.

This is a general rule. There is no phrase-specific audio or pronunciation
entry for `합니다`.

## Regression tests

`tests/test_transitions.c` parses and renders `감사합니다`, then verifies:

- the generalized adjacent-nasal duration overlap;
- bounded exact sample steps at `ㅁ -> ㅅ` and `ㅁ -> ㄴ`;
- bounded local derivatives around both joins;
- no abrupt five-millisecond energy collapse or surge.

The broader release analysis also exercises nasal/fricative, nasal/nasal,
vowel/nasal, liquid, stop, coda, affricate, and phrase transitions.

## MCU considerations

The correction state is part of the existing renderer context and requires no
heap allocation or second synthesis path. It adds arithmetic per connected
boundary, not a language lookup in the sample loop.

For best transitions:

```bash
./setup.sh --control-ms 1 --coeff-stride 1
```

The default coefficient stride of two samples is a good quality/CPU balance.
The compact profile uses a stride of four and should be checked on target
hardware.

DMA, I2S, DAC, or PWM output must remain continuous across callback blocks.
A peripheral restart or PWM duty jump can create a click even when the
generated PCM is continuous; see `DECLICKING.md`.

## Remaining intentional edges

Some large short-time energy changes are correct speech cues:

- release of a tense or aspirated stop;
- transition from an unreleased coda into a tense consonant;
- affricate release;
- phrase-initial stop burst.

Automated checks therefore distinguish exact sample discontinuities from
intended changes over several milliseconds. Smoothing every large derivative
would make the signal quieter but would also reduce Korean consonant
intelligibility.
