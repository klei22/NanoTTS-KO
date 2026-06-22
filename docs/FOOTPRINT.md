# Footprint

NanoTTS-KO avoids runtime language selection, IPA parsing, and multilingual
acoustic tables. Version 1.2 intentionally spends additional code and per-sample arithmetic on
click-free Korean transitions while retaining the Korean-only renderer.

## Runtime context

The default configuration provides 512 events, 160 Hangul syllables, and a
4096-byte public context. The measured internal structure occupies 3464 bytes
on the validation host ABI.

The compact profile provides 256 events, 96 syllables, and a 3072-byte public
context. Its measured internal structure occupies 2056 bytes on the validation
host ABI.

## Host object measurements

GCC 14.2 `-Os` x86-64 object-section measurements (`text + initialized data`):

| Configuration | Code + initialized data |
|---|---:|
| Full Korean core, lexicon, 1 ms targets, stride 2 | 29,357 B |
| Compact core, no `libm`, 2 ms targets, stride 4 | 29,329 B |
| Full core without built-in lexicon | 27,277 B |
| Raw PCM16LE adapter | 349 B |
| Timer-PWM adapter | 445 B |

The full core is approximately 2.2 KB larger than 1.1 in the same host
measurement family. The increase implements coefficient de-zippering,
boundary-aware envelopes, pause-masked state reset, and the de-click test path.
Changing the internal biquad state to transposed direct form II reduced the
runtime context by about 120 bytes.

The compact no-`libm` object is slightly larger on this host because local math
approximations replace symbols supplied by the system math library. On an MCU,
the result depends on what the toolchain's math library would otherwise pull
in.

These figures exclude the CLI, tests, examples, C runtime, startup code, and
platform audio driver. Use the target linker map as the source of truth for MCU
flash and RAM.

## Relative host CPU diagnostic

Rendering the 3.37-second demonstration phrase 200 times at 24 kHz took a median
of about 1.45 seconds in 1.1 and 1.77 seconds in the default 1.2 profile on the
validation host. This approximately 22% increase is a comparative diagnostic,
not an MCU real-time guarantee. The coefficient stride can be raised to 3 or 4
when target measurements show insufficient headroom.
