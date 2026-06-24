#!/usr/bin/env bash
set -euo pipefail

jobs=""
clean=0
run_tests=1
compact=0
use_libm=1
builtin_lexicon=1
morphology=1
normalization=1
outputs=all
control_ms=1
control_ms_explicit=0
coeff_stride=2
coeff_stride_explicit=0

usage() {
  cat <<'TXT'
Usage: ./setup.sh [options]

Build NanoTTS-KO 2 with the portable Makefile and optionally run CTest.

Options:
  --clean             Remove this profile's build directories first
  --no-test           Build without running tests
  --jobs N            Parallel build jobs
  --compact           320 events, 128 syllables, 64 tokens, 128 morphemes,
                      8192-byte context and 64-sample audio blocks
  --no-libm           Use built-in compact math approximations
  --no-lexicon        Exclude the built-in Korean exception lexicon
  --no-morphology     Disable the compact morphology analyzer
  --no-normalization  Disable semantic number/date/time normalization
  --outputs VALUE     all, pwm, pcm, or none (default: all)
  --control-ms N      Acoustic target period, 1..4 ms (default: 1)
  --coeff-stride N    Samples between coefficient smoothing steps, 1..4
  -h, --help          Show this help

Examples:
  ./setup.sh
  ./setup.sh --compact --no-libm --outputs pwm
  ./setup.sh --no-morphology --no-normalization --outputs none
TXT
}

while (($#)); do
  case "$1" in
    --clean) clean=1 ;;
    --no-test) run_tests=0 ;;
    --compact) compact=1 ;;
    --no-libm) use_libm=0 ;;
    --no-lexicon) builtin_lexicon=0 ;;
    --no-morphology) morphology=0 ;;
    --no-normalization) normalization=0 ;;
    --jobs)
      shift; [[ $# -gt 0 && "$1" =~ ^[1-9][0-9]*$ ]] || { echo "--jobs needs a positive integer" >&2; exit 2; }
      jobs="$1" ;;
    --outputs)
      shift; [[ $# -gt 0 ]] || { echo "--outputs needs a value" >&2; exit 2; }
      outputs="$1" ;;
    --control-ms)
      shift; [[ $# -gt 0 && "$1" =~ ^[1-4]$ ]] || { echo "--control-ms needs 1..4" >&2; exit 2; }
      control_ms="$1"; control_ms_explicit=1 ;;
    --coeff-stride)
      shift; [[ $# -gt 0 && "$1" =~ ^[1-4]$ ]] || { echo "--coeff-stride needs 1..4" >&2; exit 2; }
      coeff_stride="$1"; coeff_stride_explicit=1 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "unknown option: $1" >&2; usage >&2; exit 2 ;;
  esac
  shift
done

case "$outputs" in
  all) build_pwm=1; build_pcm=1 ;;
  pwm) build_pwm=1; build_pcm=0 ;;
  pcm) build_pwm=0; build_pcm=1 ;;
  none) build_pwm=0; build_pcm=0 ;;
  *) echo "--outputs must be all, pwm, pcm, or none" >&2; exit 2 ;;
esac

if [[ -z "$jobs" ]]; then
  if command -v nproc >/dev/null 2>&1; then jobs="$(nproc)"; else jobs=2; fi
fi

max_events=640
max_syllables=192
max_tokens=96
max_morphemes=192
context_bytes=16384
audio_block=128
profile="ko2"
if ((compact)); then
  max_events=320; max_syllables=128; max_tokens=64; max_morphemes=128
  context_bytes=8192; audio_block=64
  if ((!control_ms_explicit)); then control_ms=2; fi
  if ((!coeff_stride_explicit)); then coeff_stride=4; fi
  profile+="-compact"
fi
((use_libm)) || profile+="-nolibm"
((builtin_lexicon)) || profile+="-nolex"
((morphology)) || profile+="-nomorph"
((normalization)) || profile+="-nonorm"
profile+="-${outputs}"
build_dir="build-make-${profile}"

make_args=(
  "BUILD_DIR=$build_dir"
  "USE_LIBM=$use_libm"
  "BUILTIN_LEXICON=$builtin_lexicon"
  "MORPHOLOGY=$morphology"
  "NORMALIZATION=$normalization"
  "BUILD_PWM=$build_pwm"
  "BUILD_PCM=$build_pcm"
  "MAX_EVENTS=$max_events"
  "MAX_SYLLABLES=$max_syllables"
  "MAX_TOKENS=$max_tokens"
  "MAX_MORPHEMES=$max_morphemes"
  "CONTEXT_BYTES=$context_bytes"
  "AUDIO_BLOCK=$audio_block"
  "CONTROL_MS=$control_ms"
  "COEFF_STRIDE=$coeff_stride"
  "TEST_JOBS=$jobs"
)

if ((clean)); then make "${make_args[@]}" clean; fi
make -j"$jobs" "${make_args[@]}" all
if ((run_tests)); then make "${make_args[@]}" test; fi

cat <<TXT
NanoTTS-KO 2 build complete.
  CLI:          $build_dir/nanotts
  Library:      $build_dir/libnanotts.a
  Profile:      $profile
  Context:      $context_bytes bytes
  Events:       $max_events
  Syllables:    $max_syllables
  Tokens:       $max_tokens
  Morphemes:    $max_morphemes
  Morphology:   $morphology
  Normalization:$normalization
  Control:      ${control_ms} ms
  Coeff step:   every ${coeff_stride} sample(s)
TXT
