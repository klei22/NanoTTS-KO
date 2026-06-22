# Clean-room provenance

NanoTTS-KO is an original MIT-licensed implementation.

## Excluded material

The repository does not contain or link to eSpeak/eSpeak NG code, language
rules, dictionaries, voices, SAM code, or another TTS implementation. No GPL
source or voice data is embedded in the runtime.

## Linguistic implementation

Hangul decomposition follows the arithmetic organization standardized by
Unicode. Korean pronunciation behavior was implemented from published
linguistic descriptions and public Korean standard-pronunciation rules, then
expressed as new bounded C logic. Built-in pronunciation spellings were written
specifically for this project and are covered by the repository's MIT license.

The 1.1 regression data records pronunciation pairs, not copied program tables
or sampled voice material. Application users may remove the built-in lexicon at
compile time.

## Acoustic implementation

The renderer is a new compact source/filter synthesizer based on general
published speech-production and formant-synthesis principles. Filter code,
source model, acoustic tables, timing envelopes, source-quality cues, and
prosody code were authored for NanoTTS-KO.

Korean vowel targets were selected from published Seoul Korean formant
measurements, rounded into a coherent synthetic male voice, and further tuned
using generated-waveform diagnostics. The Korean three-way obstruent model uses
published qualitative and quantitative relationships among release timing,
aspiration, following-vowel F0, and phonation. These are independently authored
numeric parameters, not copied software tables.

## Voice and recordings

No recorded speaker data is shipped. Generated examples are output of this MIT
runtime. Any future sampled, residual, LPC, or unit-selection voice must include
an explicit recording and derivative-data license before distribution.

## Reference trail

Primary design references include:

- The Unicode Standard's Hangul syllable decomposition algorithm.
- National Institute of Korean Language, Standard Pronunciation rules.
- Published Seoul Korean accentual-phrase research.
- Published Korean three-way stop timing, F0, and voice-quality studies.
- Published modern Seoul Korean vowel-formant measurements.

Repository code and tables remain independently authored under MIT.
