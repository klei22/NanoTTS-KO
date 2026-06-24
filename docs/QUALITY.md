# Quality strategy

NanoTTS-KO 2 improves two independent dimensions:

```text
linguistic correctness: what should be pronounced
acoustic realization: how the selected plan is rendered
```

## Linguistic improvements in 2.0

The previous frontend contained only onset, vowel, coda, and boundary data. It
could not distinguish morphology-dependent homographs. Version 2 adds:

- token and morpheme tables;
- compact POS and suffix analysis;
- semantic normalization;
- prescriptive and contemporary pronunciation policies;
- morphology-conditioned fortition;
- context-sensitive attributive `-(으)ㄹ`;
- passive/causative versus nominalized `-기`;
- clause-local question and command interpretation;
- POS-aware accentual-phrase grouping;
- continuous pitch targets, prominence, and allophone flags;
- a 133-pair pronunciation regression corpus.

The implementation specifically preserves negative contrasts such as noun
`여덟도`, where verb-only fortition must not be overgeneralized.

## Acoustic improvements retained from 1.1-1.3

The bundled source/filter renderer includes:

- Korean-centered vowel targets;
- lax/tense/aspirated timing, source quality, and following-vowel F0 cues;
- palatalized sibilant allophones;
- moving glide and initial `ㅢ` trajectories;
- dedicated unreleased coda closures;
- multiple voiced and frication resonators;
- nasal poles and antiresonances;
- high-rate coefficient de-zippering;
- overlapping connected-phone gestures;
- raised-cosine silence edges and pause-masked state reset;
- bounded sample-domain residual bridging for remaining join mismatch.

These mechanisms reduce accidental clicks and broadband harshness without
removing intentional stop releases or useful frication cues.

## Styles

`clear-device` and `learner` spend more duration and prominence on boundaries;
`conversational` permits shorter timing and more common reductions. Style is a
pronunciation policy, not only a post-render speed control.

## What still limits naturalness

The voice remains a compact synthetic formant/source-filter voice. It has no
recorded residual, LPC unit database, neural acoustic model, or native-speaker
waveform corpus. The main remaining perceptual limitations are:

```text
mechanical spectral detail
limited speaker-specific coarticulation
simplified fricative and affricate spectra
simplified nasal place trajectories
no learned duration/prosody model
bounded lexicon and morphology
no unrestricted English/Hanja/proper-name pronunciation
```

## Path to a larger high-quality backend

The eight-byte event boundary is designed for an optional Korean voice pack:

```text
same frontend
  -> context-dependent Korean segments
  -> LPC/LSF micro-units or statistical acoustic parameters
  -> PCM
```

A useful MCU-oriented LPC unit voice would require a properly licensed native
Korean recording corpus covering onset-vowel, vowel-coda, coda-onset, sibilant,
affricate, liquid, glide, and prosodic contexts. No such recording is bundled,
so this release does not pretend to provide a human voice from synthetic tables.

## Evaluation policy

Automated tests can verify pronunciation sequences, bounds, clipping,
discontinuities, F0 ordering, filter stability, and deterministic output. They
cannot establish human intelligibility or naturalness. Native-listener blind
transcription, minimal-pair identification, and MOS/preference testing remain
required.
