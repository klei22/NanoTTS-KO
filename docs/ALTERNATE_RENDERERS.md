# Alternate renderers and recorded voices

NanoTTS-KO 2 separates Korean linguistic planning from waveform generation at
the `nanotts_event_t` boundary. An application may call
`nanotts_parse_text_ex()`, inspect or cache `nanotts_events()`, and feed those
events to a different renderer instead of calling `nanotts_render_events()`.

Each event carries:

```text
Korean phone/allophone ID
word, syllable, AP, and phrase flags
relative duration
continuous start/end F0 target
prominence
weak-h, palatalization, denasalization, question, command, and focus flags
```

## LPC or LSF micro-unit backend

A higher-quality MCU voice can reuse the frontend and replace only the bundled
formant renderer. A recommended unit pack stores:

```text
onset-to-vowel transitions
vowel centers
vowel-to-coda transitions
coda-to-onset transitions
special lax/tense/aspirated releases
sibilant and affricate trajectories
liquid, nasal, glide, and phrase-edge variants
```

Per unit, store quantized LSF/LPC trajectories, gain, pitch marks, voicing,
aperiodicity, and an independently licensed residual or excitation class. Join
inside stable vowel regions and interpolate LSFs rather than raw LPC
coefficients.

The recorded corpus, annotations, extracted voice data, and generated pack need
explicit training and redistribution rights. NanoTTS-KO ships no recorded voice
material, so the MIT source package does not imply rights to a third-party
speaker or corpus.

## Host-precomputed events

For fixed prompts, run the linguistic frontend on a host and store the resulting
events in firmware. `nanotts_set_events()` validates and loads an event array.
This removes normalization and morphology work from the real-time path, though
the current event layout is not guaranteed as a stable serialized format across
major versions. A product should add its own versioned blob header and converter.
