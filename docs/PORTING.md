# MCU porting guide

## Core requirements

NanoTTS-KO requires:

```text
C99 compiler
32-bit integer arithmetic
single-precision float support or software float
caller-provided static context
an int16 PCM sink
```

It does not require heap allocation, files, POSIX, threads, SDL, or an audio
framework.

## Recommended profiles

### Cortex-M4F/M7, ESP32, RP2350-class FPU targets

```bash
./setup.sh --outputs pwm
```

Use the default 1 ms control period and two-sample coefficient stride when the
CPU budget permits.

### Smaller/no-FPU targets

```bash
./setup.sh --compact --no-libm --outputs pwm
```

The compact profile uses smaller working arrays, 2 ms controls, and coefficient
updates every four samples.

### Fixed application vocabulary

Run normalization and morphology on a host, cache the resulting eight-byte
events, and use `nanotts_set_events()` on the MCU. This removes linguistic work
from latency-sensitive paths while preserving the high-quality renderer plan.
The event format should be wrapped in an application version header because it
is not a stable cross-major-version wire format.

## Audio callback

For I2S or DAC:

```c
static int write_pcm(void *user, const int16_t *samples, size_t count)
{
    return queue_i2s_dma(user, samples, count) ? 0 : 1;
}
```

The callback must consume or copy the samples before returning.

## PWM

NanoTTS's PWM adapter outputs one timer compare value per audio sample. It is
not a sampled carrier waveform. Configure:

```text
free-running carrier well above the audio sample rate
DMA or a deterministic ISR that updates compare at 16 or 24 kHz
mid-scale duty while idle
continuous timer operation across blocks
analog reconstruction filtering
```

Do not stop the timer, force the pin low, or reset duty between blocks; those
operations create physical pops even when the synthesized PCM is continuous.

## Memory placement

Put immutable voice, lexicon, and normalization tables in flash/ROM. Keep the
`nanotts_t` context and output adapter scratch buffers in RAM. The context is
large because it contains all tokens, morphemes, syllables, events, DSP state,
and the PCM callback block.

## Stack

The core uses bounded local variables and does not allocate large variable-length
arrays. Application callbacks should also avoid deep stacks and blocking I/O.

## Compile-time module removal

```bash
./setup.sh --no-normalization
./setup.sh --no-morphology
./setup.sh --no-lexicon
./setup.sh --outputs none
```

These remove code and tables at compile time; there is no runtime modularity
penalty.

## Throughput

Benchmark the exact target, sample rate, compiler, flash wait-state setup, and
output method. Lower `NANOTTS_CONTROL_MS` and `NANOTTS_COEFF_STRIDE` values make
transitions smoother but increase coefficient design/update work.

## Critical prompts

For alarms, medical prompts, industrial controls, or public announcements:

1. Use `clear-device` style.
2. Override product and proper-name pronunciations explicitly.
3. Review normalized numbers and units.
4. Validate through the real speaker/amplifier/PWM filter path.
5. Conduct native-Korean blind transcription in expected background noise.
