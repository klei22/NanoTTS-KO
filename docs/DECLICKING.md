# Click-free synthesis and MCU playback

> NanoTTS-KO 2.0 retains this 1.3 renderer behavior beneath the new linguistic frontend.

NanoTTS-KO 1.3 smooths both silence edges and connected-phone joins inside the renderer, but an MCU audio path
can reintroduce clicks after the PCM callback. Use the following checks to
separate the two cases.

## First isolate the source

1. Render a host WAV from the same text.
2. Capture the exact PCM or PWM compare stream used by the MCU.
3. Convert the capture back to WAV with `tools/pcm_to_wav.py` or
   `tools/pwm_to_wav.py`.

If the host WAV already clicks, inspect the phone sequence and renderer. If the
host WAV is clean but the captured stream clicks, inspect buffer continuity. If
both digital files are clean but the loudspeaker clicks, inspect the timer,
DAC, amplifier, bias, and reconstruction filter.

## DMA and callback continuity

- Use ping-pong or circular DMA; do not stop and restart the peripheral between
  NanoTTS callback blocks.
- Queue the next block before the current block completes.
- Never insert an uninitialized block or repeat the previous block after an
  underrun.
- Keep the sample-update clock continuous across phrase boundaries.
- Do not reset I2S, DAC, PWM counter, or DMA phase for every utterance.

A block boundary is not an audio boundary. Consecutive callback blocks must be
played as one continuous sample stream.

## PWM-specific idle behavior

NanoTTS's PWM file contains timer compare values, not a carrier waveform. PCM
zero maps to approximately half duty:

```c
uint16_t idle = nanotts_pcm16_to_pwm(0, top, inverted);
```

Keep that compare value loaded before playback, during idle, and after the final
DMA sample. Setting the compare register to zero, disabling the carrier, or
changing GPIO mode at a phrase boundary creates a large DC step and an audible
pop.

Recommended starting points:

- carrier frequency comfortably above the audio band;
- at least 10-bit duty resolution when the timer clock permits it;
- DMA updates at the NanoTTS sample rate;
- a reconstruction low-pass filter;
- an amplifier input biased consistently during idle;
- several decibels of analog headroom.

## DAC and I2S behavior

For a unipolar DAC, add the midpoint bias continuously and remove it with the
analog output stage if required. Do not switch between biased audio and zero
volts at utterance boundaries. For I2S, continue transmitting digital zero
samples rather than stopping the clock when the codec or amplifier is sensitive
to clock loss.

## What version 1.2 changes internally

- source and vocal-tract parameters move continuously between control targets;
- biquad coefficients are de-zippered at a configurable high-rate stride;
- phones touching real silence receive short attack/release edges;
- filter and source history is cleared only while a pause masks the output;
- the tap closure no longer uses an abrupt binary gate.

These changes target discontinuities without applying a blanket low-pass filter
to the final speech signal.


## Connected-phone joins

For the shared acoustic midpoint, pair-specific overlap windows, nasal-cluster timing, and residual boundary bridge used in 1.3, see [`TRANSITIONS.md`](TRANSITIONS.md).
