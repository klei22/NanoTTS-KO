# MCU porting

## Minimum integration

The core needs:

- a C99 compiler;
- 32-bit integer arithmetic;
- `float` support, in hardware or software;
- a callback accepting signed 16-bit PCM;
- static storage for `nanotts_t` and the peripheral queue.

It does not need an OS, heap, filesystem, locale, Unicode library, or audio
codec.

## Quality and compact profiles

The default full-quality profile uses:

```text
512 phone events
160 Hangul syllables
4096-byte public context
128-sample callback blocks
1 ms acoustic target planning
2-sample coefficient smoothing stride
```

Build it with:

```bash
./setup.sh
```

The recommended small/no-FPU starting point is:

```bash
./setup.sh --compact --no-libm --outputs pwm
```

It uses:

```text
256 phone events
96 Hangul syllables
3072-byte public context
64-sample callback blocks
2 ms acoustic target planning
4-sample coefficient smoothing stride
compact internal math
PWM adapter only
```

The compact script chooses 2 ms and a four-sample coefficient stride unless
those values are explicitly supplied. Benchmark both dimensions on the target
before trading away transition smoothness:

```bash
./setup.sh --compact --control-ms 1 --coeff-stride 1 --no-libm --outputs pwm
./setup.sh --compact --control-ms 2 --coeff-stride 2 --no-libm --outputs pwm
./setup.sh --compact --control-ms 2 --coeff-stride 4 --no-libm --outputs pwm
```

Reduce event and syllable capacities only after measuring the longest actual
prompt set. A compile-time context-size assertion rejects unsafe settings.

## Sample rate

- 16 kHz is the normal MCU baseline.
- 24 kHz retains more frication/aspiration bandwidth when CPU and output
  hardware permit it.
- 8 kHz is supported but substantially limits high-frequency consonant detail.

The acoustic table is sample-rate independent; filters are recalculated for the
selected rate.

## I2S or DAC

Connect the native callback directly to a DMA queue:

```c
static int audio_write(void *user, const int16_t *samples, size_t count)
{
    return queue_i2s_dma(user, samples, count) ? 0 : 1;
}
```

The callback must consume or copy the block before returning; NanoTTS reuses the
buffer.

## PWM

Use `nanotts_pwm_output_t` to map each PCM sample into one timer compare value.
The timer generates a much faster carrier while DMA or an ISR updates its
compare register at the speech sample rate.

A practical starting point is:

```text
speech update rate: 16 kHz
PWM TOP:             1023 or greater
PWM carrier:         well above speech/audio bandwidth
output:              reconstruction low-pass -> amplifier -> speaker
```

The `.pwm16le` host format is a stream of compare values, not a sampled carrier
waveform. Keep the PWM carrier running at midpoint duty during idle; see
`DECLICKING.md` for click-free DMA and amplifier behavior.

## CPU considerations

The quality renderer uses:

```text
4 cascade vocal-tract resonators
4 parallel voiced-detail resonators
3 frication/noise filters
2 nasal poles
2 nasal antiresonances
1 aspiration path
```

Coefficient targets are calculated once per 1–4 ms control block. Stable
intermediate coefficients are then approached every 1–4 audio samples. The
inner sample loop contains only filter/source arithmetic and does not call
trigonometry or divide. The no-`libm` build replaces coefficient and pitch math with bounded
local approximations.

Use size optimization, sectioning, and linker garbage collection:

```text
-Os -ffunction-sections -fdata-sections
-Wl,--gc-sections
```

Measure both real-time headroom and flash with the target linker map; host object
sizes are comparative only.

## Concurrency

A context is single-renderer and not internally synchronized. Use one context
per simultaneous voice or serialize access. It is safe for DMA to consume a
previously submitted block while NanoTTS computes the next block, provided the
callback copies or transfers ownership correctly.
