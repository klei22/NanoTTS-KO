# Clean-room provenance

NanoTTS-KO is an original MIT-licensed implementation.

## Excluded material

The repository does not contain or link to eSpeak/eSpeak NG code, language
rules, dictionaries, voices, SAM code, g2pK code, or another TTS implementation.
No GPL source, sampled voice, or third-party pronunciation table is embedded in
the runtime.

## Linguistic implementation

Hangul decomposition follows Unicode's published arithmetic organization.
Korean pronunciation behavior was independently implemented from public
standard-pronunciation descriptions and published linguistic research, then
expressed as new bounded C logic and independently authored tests.

Version 2's token, morphology, normalization, variant, phonology, and prosody
modules are original project code. The built-in exception spellings were
written specifically for this project. They are removable at compile time.

Public examples from standard pronunciation rules are used as behavioral
regression cases. The repository does not copy another program's rule engine,
source layout, generated dictionary, or acoustic data.

## Acoustic implementation

The renderer is a new compact source/filter synthesizer based on general
published speech-production and formant-synthesis principles. Filter code,
source model, acoustic tables, timing envelopes, source-quality cues, smoothing,
and prosody code were authored for NanoTTS-KO.

Korean vowel targets were rounded into one coherent synthetic voice and tuned
using generated-waveform diagnostics. Obstruent distinctions use independently
authored relationships among release timing, aspiration, following-vowel F0,
and source quality rather than copied software tables.

## Voice and recordings

No recorded speaker data is shipped. Generated examples are output of this MIT
runtime. Any future sampled, residual, LPC, unit-selection, or learned voice
must include explicit rights for the recordings, annotations, model training,
and redistribution before it is packaged.

## External comparison tools

Third-party Korean G2P or TTS tools may be used process-separately as behavioral
or listening references during development. Their source and data are not
incorporated into this MIT runtime.
