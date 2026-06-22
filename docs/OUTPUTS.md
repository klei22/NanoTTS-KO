# Output formats

## Native PCM callback

The core always produces mono signed 16-bit PCM in native memory representation.
This is the preferred MCU interface for I2S and DAC peripherals.

## WAV

The CLI's WAV writer is host tooling only:

```bash
nanotts --text "안녕하세요." -o hello.wav
```

## Raw PCM16LE

Build the `NanoTTS::PCM` adapter and select:

```bash
nanotts --text "안녕하세요." --format pcm -o hello.pcm16le
```

Convert to WAV:

```bash
python3 tools/pcm_to_wav.py hello.pcm16le hello.wav --rate 16000
```

## PWM compare values

`NanoTTS::PWM` maps every PCM sample into a `uint16_t` timer compare value:

```bash
nanotts --text "안녕하세요." --format pwm \
  --pwm-top 1023 -o hello.pwm16le
```

Convert the diagnostic stream back to approximate PCM/WAV:

```bash
python3 tools/pwm_to_wav.py hello.pwm16le hello.wav \
  --top 1023 --rate 16000
```

Quantization depends on `TOP`. A value of 255 provides approximately 8-bit
amplitude resolution; 1023 provides approximately 10-bit resolution.

## Avoiding MCU playback clicks

Keep output peripherals and DMA streams continuous across callback blocks. PWM
silence is the midpoint compare value, not compare zero:

```c
uint16_t idle = nanotts_pcm16_to_pwm(0, top, inverted);
```

Load that value before, between, and after utterances without disabling the PWM
carrier. See `docs/DECLICKING.md` for DMA, DAC, I2S, PWM, and amplifier checks.
