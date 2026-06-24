/* SPDX-License-Identifier: MIT */
#include "nanotts/nanotts.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#define SAMPLE_RATE 16000u
#define SAMPLE_CAPACITY 60000u

static int16_t samples[SAMPLE_CAPACITY];
static size_t sample_count;

static int collect(void *user, const int16_t *input, size_t count)
{
    size_t i;
    (void)user;
    if (sample_count + count > SAMPLE_CAPACITY) return 1;
    for (i = 0u; i < count; ++i) samples[sample_count + i] = input[i];
    sample_count += count;
    return 0;
}

static uint32_t magnitude32(int32_t x)
{
    return x < 0 ? (uint32_t)(-x) : (uint32_t)x;
}

static unsigned base_duration_ms(nanotts_phone_t phone)
{
    switch (phone) {
    case NANOTTS_PH_A: return 104u;
    case NANOTTS_PH_I: return 90u;
    case NANOTTS_PH_K_LAX: return 78u;
    case NANOTTS_PH_T_LAX: return 74u;
    case NANOTTS_PH_S_LAX: return 112u;
    case NANOTTS_PH_H: return 76u;
    case NANOTTS_PH_M: return 80u;
    case NANOTTS_PH_N: return 78u;
    default: assert(!"unexpected phone in transition probe"); return 1u;
    }
}

static size_t event_samples(const nanotts_event_t *event)
{
    uint64_t n = (uint64_t)base_duration_ms((nanotts_phone_t)event->phone) *
                 (uint64_t)event->duration_percent *
                 (uint64_t)SAMPLE_RATE;
    n /= 100u * 1000u;
    return (size_t)(n ? n : 1u);
}

static uint64_t energy(size_t first, size_t last)
{
    uint64_t total = 0u;
    size_t i;
    assert(last <= sample_count);
    for (i = first; i < last; ++i) {
        int32_t v = samples[i];
        total += (uint64_t)(v * v);
    }
    return total;
}

int main(void)
{
    nanotts_t tts;
    nanotts_options_t options;
    const nanotts_event_t *events;
    size_t count, i, boundary;
    unsigned checked_ms = 0u, checked_mn = 0u;

    assert(nanotts_init(&tts, SAMPLE_RATE) == NANOTTS_OK);
    nanotts_default_options(&options);
    assert(nanotts_parse_text(
        &tts, "감사합니다", NANOTTS_NPOS, options.flags, NULL) == NANOTTS_OK);

    events = nanotts_events(&tts);
    count = nanotts_event_count(&tts);
    assert(count == 12u);

    /* Adjacent /m.n/ is one overlapped nasal cluster rather than two full
     * independent phones. */
    for (i = 1u; i < count; ++i) {
        if (events[i - 1u].phone == NANOTTS_PH_M &&
            events[i].phone == NANOTTS_PH_N) {
            assert(events[i - 1u].duration_percent >= 60u);
            assert(events[i - 1u].duration_percent <= 75u);
            assert(events[i].duration_percent >= 68u);
            assert(events[i].duration_percent <= 82u);
        }
    }

    sample_count = 0u;
    assert(nanotts_render_events(
        &tts, &options, collect, NULL) == NANOTTS_OK);

    /* The renderer emits 20 ms of leading silence. */
    boundary = SAMPLE_RATE / 50u;
    for (i = 1u; i < count; ++i) {
        nanotts_phone_t left = (nanotts_phone_t)events[i - 1u].phone;
        nanotts_phone_t right = (nanotts_phone_t)events[i].phone;
        size_t k, first, last;
        uint32_t exact_jump, local_jump = 0u;
        uint64_t before, after;

        boundary += event_samples(&events[i - 1u]);
        if (!((left == NANOTTS_PH_M && right == NANOTTS_PH_S_LAX) ||
              (left == NANOTTS_PH_M && right == NANOTTS_PH_N)))
            continue;

        assert(boundary > 80u && boundary + 80u < sample_count);
        exact_jump = magnitude32(
            (int32_t)samples[boundary] - (int32_t)samples[boundary - 1u]);
        first = boundary - 32u; /* +/- 2 ms */
        last = boundary + 32u;
        for (k = first + 1u; k < last; ++k) {
            uint32_t jump = magnitude32(
                (int32_t)samples[k] - (int32_t)samples[k - 1u]);
            if (jump > local_jump) local_jump = jump;
        }

        before = energy(boundary - 80u, boundary);
        after = energy(boundary, boundary + 80u);
        assert(before > 0u && after > 0u);
        assert(exact_jump < 512u);
        assert(local_jump < 1200u);
        assert(after * 4u > before && before * 4u > after);

        if (right == NANOTTS_PH_S_LAX) ++checked_ms;
        else ++checked_mn;
    }

    assert(checked_ms == 1u);
    assert(checked_mn == 1u);
    return 0;
}
