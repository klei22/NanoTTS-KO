/* SPDX-License-Identifier: MIT */
#include "nanotts_internal.h"

#define DEF(kind_, dur_, voice_, noise_, asp_, amp_, open_, tilt_, pitch_, \
            f1_, f2_, f3_, f4_, e1_, e2_, e3_, e4_, nc_, nb_) \
    { (uint8_t)(kind_), (uint8_t)(dur_), (uint8_t)(voice_), (uint8_t)(noise_), \
      (uint8_t)(asp_), (uint8_t)(amp_), (uint8_t)(open_), (uint8_t)(tilt_), \
      (int8_t)(pitch_), 0u, \
      (uint16_t)(f1_), (uint16_t)(f2_), (uint16_t)(f3_), (uint16_t)(f4_), \
      (uint16_t)(e1_), (uint16_t)(e2_), (uint16_t)(e3_), (uint16_t)(e4_), \
      (uint16_t)(nc_), (uint16_t)(nb_) }

static const nanotts_phone_def_t PHONE_DEFS[NANOTTS_PH_COUNT] = {
    DEF(NANOTTS_KIND_PAUSE, 60, 0,0,0,0,120,150,0, 500,1500,2500,3500,500,1500,2500,3500,0,0),
    DEF(NANOTTS_KIND_PAUSE, 22, 0,0,0,0,120,150,0, 500,1500,2500,3500,500,1500,2500,3500,0,0),
    DEF(NANOTTS_KIND_PAUSE, 42, 0,0,0,0,120,150,0, 500,1500,2500,3500,500,1500,2500,3500,0,0),
    DEF(NANOTTS_KIND_PAUSE,145, 0,0,0,0,120,150,0, 500,1500,2500,3500,500,1500,2500,3500,0,0),

    DEF(NANOTTS_KIND_VOWEL,104,255,3,2,232,116,115,0, 790,1190,2520,3550,790,1190,2520,3550,0,0),
    DEF(NANOTTS_KIND_VOWEL,100,255,4,3,230,118,118,0, 585,900,2440,3440,585,900,2440,3440,0,0),
    DEF(NANOTTS_KIND_VOWEL, 96,255,3,2,228,112,120,0, 370,690,2350,3350,370,690,2350,3350,0,0),
    DEF(NANOTTS_KIND_VOWEL, 92,255,3,2,226,110,122,0, 275,900,2250,3300,275,900,2250,3300,0,0),
    DEF(NANOTTS_KIND_VOWEL, 96,255,3,2,228,113,120,0, 300,1560,2480,3450,300,1560,2480,3450,0,0),
    DEF(NANOTTS_KIND_VOWEL, 90,255,2,1,224,108,126,0, 235,2320,3020,3700,235,2320,3020,3700,0,0),
    DEF(NANOTTS_KIND_VOWEL, 96,255,3,2,230,115,120,0, 500,2025,2700,3560,500,2025,2700,3560,0,0),
    DEF(NANOTTS_KIND_VOWEL, 94,255,3,2,229,113,122,0, 480,2070,2750,3600,480,2070,2750,3600,0,0),
    DEF(NANOTTS_KIND_GLIDE, 38,235,2,2,195,105,128,0, 250,2250,3000,3700,430,1750,2700,3550,0,0),
    DEF(NANOTTS_KIND_GLIDE, 42,235,2,2,195,106,126,0, 280,760,2200,3300,430,1250,2450,3480,0,0),

    DEF(NANOTTS_KIND_STOP_LAX, 78,34,170,105,205,132,112,-28, 420,1380,2500,3500,420,1380,2500,3500,2250,1800),
    DEF(NANOTTS_KIND_STOP_TENSE,63,0,180,8,215,78,92,30, 420,1380,2500,3500,420,1380,2500,3500,2450,1550),
    DEF(NANOTTS_KIND_STOP_ASP,108,0,225,245,222,145,132,44, 420,1380,2500,3500,420,1380,2500,3500,2350,2050),

    DEF(NANOTTS_KIND_STOP_LAX,74,34,175,100,203,132,112,-28, 360,1800,2780,3800,360,1800,2780,3800,4900,2700),
    DEF(NANOTTS_KIND_STOP_TENSE,60,0,190,7,213,76,90,28, 360,1800,2780,3800,360,1800,2780,3800,5100,2350),
    DEF(NANOTTS_KIND_STOP_ASP,104,0,230,250,222,145,132,44, 360,1800,2780,3800,360,1800,2780,3800,5000,3000),

    DEF(NANOTTS_KIND_STOP_LAX,76,34,150,90,201,132,112,-28, 330,850,2250,3350,330,850,2250,3350,1700,2200),
    DEF(NANOTTS_KIND_STOP_TENSE,61,0,165,6,211,76,90,28, 330,850,2250,3350,330,850,2250,3350,1850,1850),
    DEF(NANOTTS_KIND_STOP_ASP,105,0,215,245,220,145,132,44, 330,850,2250,3350,330,850,2250,3350,1800,2600),

    DEF(NANOTTS_KIND_AFFRICATE_LAX,104,25,220,85,212,128,108,-26, 400,2050,3050,4000,400,2050,3050,4000,3750,2200),
    DEF(NANOTTS_KIND_AFFRICATE_TENSE,90,0,245,6,222,74,88,32, 400,2050,3050,4000,400,2050,3050,4000,3950,1950),
    DEF(NANOTTS_KIND_AFFRICATE_ASP,126,0,245,235,226,145,130,46, 400,2050,3050,4000,400,2050,3050,4000,3850,2450),

    DEF(NANOTTS_KIND_FRICATIVE_LAX,112,0,245,90,215,132,118,30, 430,1900,3200,4500,430,1900,3200,4500,5700,2500),
    DEF(NANOTTS_KIND_FRICATIVE_TENSE,94,0,255,6,224,74,90,38, 430,1950,3250,4550,430,1950,3250,4550,6000,2200),
    /* Korean plain and tense sibilants before /i/ and y-glides are strongly
     * palatalized. Separate
     * spectral targets avoid treating it as only a frontend label. */
    DEF(NANOTTS_KIND_FRICATIVE_LAX,116,0,242,96,212,134,120,28, 390,2200,3150,4200,390,2200,3150,4200,4450,2350),
    DEF(NANOTTS_KIND_FRICATIVE_TENSE,98,0,255,7,223,76,90,38, 390,2250,3200,4300,390,2250,3200,4300,4780,2050),
    DEF(NANOTTS_KIND_FRICATIVE_H,76,0,145,225,182,150,145,18, 500,1500,2500,3500,500,1500,2500,3500,1450,2800),

    DEF(NANOTTS_KIND_NASAL,80,226,4,1,198,120,130,0, 260,850,2150,3200,260,850,2150,3200,0,0),
    DEF(NANOTTS_KIND_NASAL,78,226,4,1,198,120,130,0, 270,1550,2500,3450,270,1550,2500,3450,0,0),
    DEF(NANOTTS_KIND_NASAL,82,226,4,1,198,120,130,0, 270,1120,2320,3320,270,1120,2320,3320,0,0),
    DEF(NANOTTS_KIND_LATERAL,70,236,3,1,202,112,125,0, 350,1280,2580,3500,350,1280,2580,3500,0,0),
    DEF(NANOTTS_KIND_TAP,34,220,3,1,192,112,125,0, 420,1500,2580,3500,420,1500,2580,3500,0,0),

    DEF(NANOTTS_KIND_CODA_STOP,52,0,0,0,0,110,120,0, 430,1350,2450,3450,430,1350,2450,3450,0,0),
    DEF(NANOTTS_KIND_CODA_STOP,48,0,0,0,0,110,120,0, 360,1800,2750,3750,360,1800,2750,3750,0,0),
    DEF(NANOTTS_KIND_CODA_STOP,50,0,0,0,0,110,120,0, 330,850,2250,3350,330,850,2250,3350,0,0),

    /* ㅢ: moving /ɯi/ nucleus. It remains vowel-like so it carries phrase pitch. */
    DEF(NANOTTS_KIND_VOWEL,112,255,3,2,228,112,122,0,
        300,1560,2480,3450,235,2320,3020,3700,0,0)
};
#undef DEF

static const char *const PHONE_NAMES[NANOTTS_PH_COUNT] = {
    "sil","pause","ap-pause","phrase-pause",
    "a","eo","o","u","eu","i","ae","e","y","w",
    "g/k-lax","kk-tense","kh-asp","d/t-lax","tt-tense","th-asp",
    "b/p-lax","pp-tense","ph-asp","j-lax","jj-tense","ch-asp",
    "s-lax","ss-tense","s-pal","ss-pal","h","m","n","ng","l","r-tap",
    "k-coda","t-coda","p-coda","ui"
};

const nanotts_phone_def_t *nanotts_phone_def(nanotts_phone_t phone)
{
    if ((unsigned)phone >= (unsigned)NANOTTS_PH_COUNT)
        return &PHONE_DEFS[NANOTTS_PH_SILENCE];
    return &PHONE_DEFS[phone];
}
bool nanotts_phone_is_vowel(nanotts_phone_t phone)
{ return nanotts_phone_def(phone)->kind == NANOTTS_KIND_VOWEL; }
bool nanotts_phone_is_pause(nanotts_phone_t phone)
{ return nanotts_phone_def(phone)->kind == NANOTTS_KIND_PAUSE; }
bool nanotts_phone_is_sonorant(nanotts_phone_t phone)
{
    nanotts_phone_kind_t kind = (nanotts_phone_kind_t)nanotts_phone_def(phone)->kind;
    return kind == NANOTTS_KIND_VOWEL || kind == NANOTTS_KIND_GLIDE ||
           kind == NANOTTS_KIND_NASAL || kind == NANOTTS_KIND_LATERAL ||
           kind == NANOTTS_KIND_TAP;
}
bool nanotts_phone_is_high_onset(nanotts_phone_t phone)
{
    nanotts_phone_kind_t kind = (nanotts_phone_kind_t)nanotts_phone_def(phone)->kind;
    return kind == NANOTTS_KIND_STOP_TENSE || kind == NANOTTS_KIND_STOP_ASP ||
           kind == NANOTTS_KIND_AFFRICATE_TENSE ||
           kind == NANOTTS_KIND_AFFRICATE_ASP ||
           kind == NANOTTS_KIND_FRICATIVE_LAX ||
           kind == NANOTTS_KIND_FRICATIVE_TENSE ||
           kind == NANOTTS_KIND_FRICATIVE_H;
}
const char *nanotts_phone_name(nanotts_phone_t phone)
{
    if ((unsigned)phone >= (unsigned)NANOTTS_PH_COUNT) return "unknown";
    return PHONE_NAMES[phone];
}
