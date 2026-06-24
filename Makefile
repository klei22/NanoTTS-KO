CC ?= cc
AR ?= ar
CFLAGS ?= -Os -std=c99 -Wall -Wextra -Wpedantic
CPPFLAGS ?=
LDFLAGS ?=
USE_LIBM ?= 1
BUILTIN_LEXICON ?= 1
MORPHOLOGY ?= 1
NORMALIZATION ?= 1
BUILD_PWM ?= 1
BUILD_PCM ?= 1
MAX_EVENTS ?= 640
MAX_SYLLABLES ?= 192
MAX_TOKENS ?= 96
MAX_MORPHEMES ?= 192
CONTEXT_BYTES ?= 16384
AUDIO_BLOCK ?= 128
CONTROL_MS ?= 1
COEFF_STRIDE ?= 2
TEST_JOBS ?= 2
BUILD_DIR ?= build-make

DEFS = -DNANOTTS_USE_LIBM=$(USE_LIBM) -DNANOTTS_ENABLE_BUILTIN_LEXICON=$(BUILTIN_LEXICON) \
       -DNANOTTS_ENABLE_MORPHOLOGY=$(MORPHOLOGY) -DNANOTTS_ENABLE_NORMALIZATION=$(NORMALIZATION) \
       -DNANOTTS_MAX_EVENTS=$(MAX_EVENTS) -DNANOTTS_MAX_SYLLABLES=$(MAX_SYLLABLES) \
       -DNANOTTS_MAX_TOKENS=$(MAX_TOKENS) -DNANOTTS_MAX_MORPHEMES=$(MAX_MORPHEMES) \
       -DNANOTTS_CONTEXT_BYTES=$(CONTEXT_BYTES) -DNANOTTS_AUDIO_BLOCK=$(AUDIO_BLOCK) \
       -DNANOTTS_CONTROL_MS=$(CONTROL_MS) -DNANOTTS_COEFF_STRIDE=$(COEFF_STRIDE)
INCS = -Iinclude -Isrc
CORE_SRC = src/nanotts.c src/nanotts_ko.c src/nanotts_ko_unicode.c src/nanotts_ko_normalize.c \
           src/nanotts_ko_lexicon.c src/nanotts_ko_morph.c src/nanotts_ko_phonology.c \
           src/nanotts_ko_prosody.c src/nanotts_voice.c src/nanotts_synth.c
CORE_OBJ = $(CORE_SRC:%.c=$(BUILD_DIR)/%.o)
ADAPTER_OBJ =
CLI_DEFS = -DNANOTTS_CLI_HAVE_PWM=$(BUILD_PWM) -DNANOTTS_CLI_HAVE_PCM=$(BUILD_PCM)
LIBS =
ifeq ($(USE_LIBM),1)
LIBS += -lm
endif
ifeq ($(BUILD_PWM),1)
ADAPTER_OBJ += $(BUILD_DIR)/src/output/nanotts_pwm.o
endif
ifeq ($(BUILD_PCM),1)
ADAPTER_OBJ += $(BUILD_DIR)/src/output/nanotts_pcm.o
endif

.PHONY: all clean test
all: $(BUILD_DIR)/libnanotts.a $(BUILD_DIR)/nanotts

$(BUILD_DIR)/libnanotts.a: $(CORE_OBJ) $(ADAPTER_OBJ)
	@mkdir -p $(@D)
	$(AR) rcs $@ $^

$(BUILD_DIR)/nanotts: $(BUILD_DIR)/tools/nanotts_cli.o $(BUILD_DIR)/libnanotts.a
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(DEFS) $(INCS) $(CFLAGS) $(if $(findstring tools/nanotts_cli.c,$<),$(CLI_DEFS),) -c $< -o $@

test: all
	cmake -S . -B $(BUILD_DIR)-tests -DNANOTTS_BUILD_CLI=OFF -DNANOTTS_BUILD_EXAMPLES=OFF \
	  -DNANOTTS_USE_LIBM=$(if $(filter 1,$(USE_LIBM)),ON,OFF) \
	  -DNANOTTS_ENABLE_BUILTIN_LEXICON=$(if $(filter 1,$(BUILTIN_LEXICON)),ON,OFF) \
	  -DNANOTTS_ENABLE_MORPHOLOGY=$(if $(filter 1,$(MORPHOLOGY)),ON,OFF) \
	  -DNANOTTS_ENABLE_NORMALIZATION=$(if $(filter 1,$(NORMALIZATION)),ON,OFF) \
	  -DNANOTTS_BUILD_PWM=$(if $(filter 1,$(BUILD_PWM)),ON,OFF) \
	  -DNANOTTS_BUILD_PCM=$(if $(filter 1,$(BUILD_PCM)),ON,OFF) \
	  -DNANOTTS_MAX_EVENTS=$(MAX_EVENTS) -DNANOTTS_MAX_SYLLABLES=$(MAX_SYLLABLES) \
	  -DNANOTTS_MAX_TOKENS=$(MAX_TOKENS) -DNANOTTS_MAX_MORPHEMES=$(MAX_MORPHEMES) \
	  -DNANOTTS_CONTEXT_BYTES=$(CONTEXT_BYTES) -DNANOTTS_AUDIO_BLOCK=$(AUDIO_BLOCK) \
       -DNANOTTS_CONTROL_MS=$(CONTROL_MS) -DNANOTTS_COEFF_STRIDE=$(COEFF_STRIDE)
	cmake --build $(BUILD_DIR)-tests --parallel $(TEST_JOBS)
	ctest --test-dir $(BUILD_DIR)-tests --output-on-failure

clean:
	rm -rf $(BUILD_DIR) $(BUILD_DIR)-tests
