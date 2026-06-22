/* SPDX-License-Identifier: MIT */
#include "nanotts_internal.h"

#if NANOTTS_USE_LIBM
#include <math.h>
#endif

#define KO_BASE_PITCH_HZ 118.0f

static float absf_local(float x) { return x < 0.0f ? -x : x; }
static float floor_positive(float x)
{
#if NANOTTS_USE_LIBM
    return floorf(x);
#else
    return (float)(uint32_t)x;
#endif
}

static void sincos_local(float angle, float *s, float *c)
{
#if NANOTTS_USE_LIBM
    *s = sinf(angle); *c = cosf(angle);
#else
    float x = angle, sign = 1.0f, x2;
    if (x > 0.5f * NANOTTS_PI_F) { x = NANOTTS_PI_F - x; sign = -1.0f; }
    x2 = x * x;
    *s = x * (1.0f + x2 * (-0.1666666667f +
         x2 * (0.0083333333f + x2 * -0.0001984127f)));
    *c = sign * (1.0f + x2 * (-0.5f +
         x2 * (0.0416666667f + x2 * (-0.0013888889f +
         x2 * 0.0000248016f))));
#endif
}

static float exp2_local(float exponent)
{
#if NANOTTS_USE_LIBM
    return powf(2.0f, exponent);
#else
    int whole = 0;
    float y, y2, result;
    while (exponent > 0.5f) { exponent -= 1.0f; ++whole; }
    while (exponent < -0.5f) { exponent += 1.0f; --whole; }
    y = exponent * 0.6931471806f; y2 = y * y;
    result = 1.0f + y + y2 * (0.5f + y * (0.1666666667f +
             y * (0.0416666667f + y * (0.0083333333f + y * 0.0013888889f))));
    while (whole > 0) { result *= 2.0f; --whole; }
    while (whole < 0) { result *= 0.5f; ++whole; }
    return result;
#endif
}

static void biquad_clear(nanotts_biquad_t *f)
{
    f->z1=f->z2=0.0f;
}

static void biquad_bandpass(nanotts_biquad_coeffs_t *f, float center,
                            float bandwidth, float sample_rate)
{
    float w0, q, alpha, a0, s, c;
    center = nanotts_clampf(center, 45.0f, sample_rate * 0.455f);
    bandwidth = nanotts_clampf(bandwidth, 50.0f, sample_rate * 0.40f);
    q = nanotts_clampf(center / bandwidth, 0.30f, 24.0f);
    w0 = 2.0f * NANOTTS_PI_F * center / sample_rate;
    sincos_local(w0,&s,&c); alpha=s/(2.0f*q); a0=1.0f+alpha;
    f->b0=alpha/a0; f->b1=0.0f; f->b2=-alpha/a0;
    f->a1=(-2.0f*c)/a0; f->a2=(1.0f-alpha)/a0;
}

static void biquad_notch(nanotts_biquad_coeffs_t *f, float center,
                         float bandwidth, float sample_rate)
{
    float w0, q, alpha, a0, s, c;
    center = nanotts_clampf(center, 80.0f, sample_rate * 0.445f);
    bandwidth = nanotts_clampf(bandwidth, 70.0f, sample_rate * 0.35f);
    q = nanotts_clampf(center / bandwidth, 0.35f, 20.0f);
    w0 = 2.0f * NANOTTS_PI_F * center / sample_rate;
    sincos_local(w0,&s,&c); alpha=s/(2.0f*q); a0=1.0f+alpha;
    f->b0=1.0f/a0; f->b1=(-2.0f*c)/a0; f->b2=1.0f/a0;
    f->a1=(-2.0f*c)/a0; f->a2=(1.0f-alpha)/a0;
}

static void biquad_resonator(nanotts_biquad_coeffs_t *f, float center,
                             float bandwidth, float sample_rate)
{
    float radius, angle, s, c, a1, a2;
    center=nanotts_clampf(center,45.0f,sample_rate*0.455f);
    bandwidth=nanotts_clampf(bandwidth,45.0f,sample_rate*0.40f);
    radius=exp2_local((-NANOTTS_PI_F*bandwidth/sample_rate)/0.6931471806f);
    angle=2.0f*NANOTTS_PI_F*center/sample_rate;
    sincos_local(angle,&s,&c); (void)s;
    a1=2.0f*radius*c; a2=-radius*radius;
    f->b0=1.0f-a1-a2; f->b1=f->b2=0.0f;
    f->a1=-a1; f->a2=-a2;
}

static float biquad_process(nanotts_biquad_t *f,float x)
{
    float y=f->b0*x+f->z1;
    f->z1=f->b1*x-f->a1*y+f->z2;
    f->z2=f->b2*x-f->a2*y;
    return y;
}

static void biquad_set_coeffs(nanotts_biquad_t *f,
                              const nanotts_biquad_coeffs_t *c)
{
    f->b0=c->b0; f->b1=c->b1; f->b2=c->b2;
    f->a1=c->a1; f->a2=c->a2;
}

static void biquad_approach_coeffs(nanotts_biquad_t *f,
                                   const nanotts_biquad_coeffs_t *c,
                                   float amount)
{
    f->b0+=amount*(c->b0-f->b0);
    f->b1+=amount*(c->b1-f->b1);
    f->b2+=amount*(c->b2-f->b2);
    f->a1+=amount*(c->a1-f->a1);
    f->a2+=amount*(c->a2-f->a2);
}

static float q8(uint8_t x) { return (float)x/255.0f; }
static float formant(const nanotts_phone_def_t *d,unsigned i,bool end)
{
    const uint16_t *a=&d->f1,*b=&d->e1; return (float)(end?b[i]:a[i]);
}

static bool adjacent(const nanotts_impl_t *impl,size_t at,int dir,
                     nanotts_phone_t *phone)
{
    size_t i;
    if(dir<0){ if(!at)return false; i=at-1u; }
    else { i=at+1u; if(i>=impl->event_count)return false; }
    *phone=(nanotts_phone_t)impl->events[i].phone;
    return !nanotts_phone_is_pause(*phone);
}

/* Return the obstruent that begins the current Korean syllable.  This lets
 * post-onset source and F0 cues survive an intervening /w/ or /j/ glide. */
static bool syllable_onset(const nanotts_impl_t *impl,size_t at,
                           nanotts_phone_t *phone,size_t *distance)
{
    size_t i=at;
    for(;;){
        const nanotts_event_t *e=&impl->events[i];
        nanotts_phone_t p=(nanotts_phone_t)e->phone;
        if(nanotts_phone_is_pause(p))return false;
        if((e->flags&NANOTTS_EVENT_SYLLABLE_START)!=0u){
            nanotts_phone_kind_t k=(nanotts_phone_kind_t)nanotts_phone_def(p)->kind;
            if(k==NANOTTS_KIND_STOP_LAX||k==NANOTTS_KIND_STOP_TENSE||
               k==NANOTTS_KIND_STOP_ASP||k==NANOTTS_KIND_AFFRICATE_LAX||
               k==NANOTTS_KIND_AFFRICATE_TENSE||k==NANOTTS_KIND_AFFRICATE_ASP||
               k==NANOTTS_KIND_FRICATIVE_LAX||k==NANOTTS_KIND_FRICATIVE_TENSE||
               k==NANOTTS_KIND_FRICATIVE_H){
                *phone=p;if(distance)*distance=at-i;return true;
            }
            return false;
        }
        if(i==0u)return false;
        --i;
    }
}

static bool is_coronal(nanotts_phone_t p)
{
    return p==NANOTTS_PH_T_LAX||p==NANOTTS_PH_T_TENSE||p==NANOTTS_PH_T_ASP||
           p==NANOTTS_PH_S_LAX||p==NANOTTS_PH_S_TENSE||p==NANOTTS_PH_N||
           p==NANOTTS_PH_L||p==NANOTTS_PH_R||p==NANOTTS_PH_T_CODA;
}
static bool is_velar(nanotts_phone_t p)
{
    return p==NANOTTS_PH_K_LAX||p==NANOTTS_PH_K_TENSE||p==NANOTTS_PH_K_ASP||
           p==NANOTTS_PH_NG||p==NANOTTS_PH_K_CODA;
}
static bool is_labial(nanotts_phone_t p)
{
    return p==NANOTTS_PH_P_LAX||p==NANOTTS_PH_P_TENSE||p==NANOTTS_PH_P_ASP||
           p==NANOTTS_PH_M||p==NANOTTS_PH_P_CODA||p==NANOTTS_PH_W;
}
static bool is_palatal(nanotts_phone_t p)
{
    return p==NANOTTS_PH_C_LAX||p==NANOTTS_PH_C_TENSE||p==NANOTTS_PH_C_ASP||
           p==NANOTTS_PH_Y;
}

static bool consonant_locus(nanotts_phone_t p,const nanotts_params_t *v,float out[4])
{
    out[0]=350.0f; out[2]=2580.0f; out[3]=3500.0f;
    if(is_labial(p)){ out[1]=780.0f; out[0]=310.0f; return true; }
    if(is_coronal(p)){ out[1]=1800.0f; out[0]=360.0f; return true; }
    if(is_palatal(p)){ out[1]=2200.0f; out[0]=390.0f; return true; }
    if(is_velar(p)){ out[1]=nanotts_clampf(0.55f*v->f[1]+930.0f,1200.0f,2150.0f); return true; }
    if(p==NANOTTS_PH_H){ out[1]=v->f[1]; out[0]=v->f[0]; return true; }
    return false;
}

static void set_nasal_profile(nanotts_params_t *p,nanotts_phone_t phone)
{
    p->nasal_pole[0]=270.0f; p->nasal_bw[0]=155.0f;
    p->nasal_pole[1]=1050.0f; p->nasal_bw[1]=330.0f;
    p->nasal_zero[0]=1000.0f; p->nasal_zero_bw[0]=260.0f;
    p->nasal_zero[1]=2200.0f; p->nasal_zero_bw[1]=420.0f;
    if(phone==NANOTTS_PH_M){
        p->nasal_pole[1]=920.0f;
        p->nasal_zero[0]=760.0f; p->nasal_zero[1]=1650.0f;
    }else if(phone==NANOTTS_PH_N){
        p->nasal_pole[1]=1450.0f;
        p->nasal_zero[0]=1250.0f; p->nasal_zero[1]=2450.0f;
    }else if(phone==NANOTTS_PH_NG){
        p->nasal_pole[1]=1120.0f;
        p->nasal_zero[0]=1050.0f; p->nasal_zero[1]=2750.0f;
    }
}

static void acoustic_shape(nanotts_phone_t phone,const nanotts_phone_def_t *d,
                           nanotts_params_t *p)
{
    nanotts_phone_kind_t k=(nanotts_phone_kind_t)d->kind;
    p->bw[0]=92.0f; p->bw[1]=108.0f; p->bw[2]=155.0f; p->bw[3]=230.0f;
    p->detail_gain[0]=0.0f; p->detail_gain[1]=0.0f; p->detail_gain[2]=0.0f; p->detail_gain[3]=0.0f;
    if(k==NANOTTS_KIND_VOWEL){
        p->bw[0]=phone==NANOTTS_PH_I||phone==NANOTTS_PH_U||
                 phone==NANOTTS_PH_EU?70.0f:82.0f;
        p->bw[1]=95.0f; p->bw[2]=135.0f; p->bw[3]=205.0f;
        p->detail_gain[0]=8.0f; p->detail_gain[1]=15.5f; p->detail_gain[2]=5.5f; p->detail_gain[3]=2.2f;
    }else if(k==NANOTTS_KIND_GLIDE){
        p->bw[0]=88.0f; p->bw[1]=112.0f; p->bw[2]=160.0f; p->bw[3]=230.0f;
        p->detail_gain[0]=6.0f; p->detail_gain[1]=12.0f; p->detail_gain[2]=4.2f; p->detail_gain[3]=1.7f;
    }else if(k==NANOTTS_KIND_NASAL){
        p->bw[0]=125.0f; p->bw[1]=210.0f; p->bw[2]=310.0f; p->bw[3]=430.0f;
        p->detail_gain[0]=2.5f; p->detail_gain[1]=4.5f; p->detail_gain[2]=1.8f; p->detail_gain[3]=0.7f;
    }else if(k==NANOTTS_KIND_LATERAL){
        p->bw[0]=105.0f; p->bw[1]=175.0f; p->bw[2]=235.0f; p->bw[3]=330.0f;
        p->detail_gain[0]=4.2f; p->detail_gain[1]=7.5f; p->detail_gain[2]=3.0f; p->detail_gain[3]=1.1f;
    }else if(k==NANOTTS_KIND_TAP){
        p->bw[0]=115.0f; p->bw[1]=190.0f; p->bw[2]=250.0f; p->bw[3]=350.0f;
        p->detail_gain[0]=3.6f; p->detail_gain[1]=6.2f; p->detail_gain[2]=2.5f; p->detail_gain[3]=0.9f;
    }
    set_nasal_profile(p,phone);
}

static void base_params(nanotts_phone_t phone,const nanotts_phone_def_t *d,
                        float progress,nanotts_params_t *p)
{
    unsigned i; bool moving=false; float t;
    for(i=0u;i<4u;++i)if(formant(d,i,false)!=formant(d,i,true))moving=true;
    t=moving?nanotts_smoothstep(progress):0.0f;
    for(i=0u;i<4u;++i)p->f[i]=nanotts_lerp(formant(d,i,false),formant(d,i,true),t);
    acoustic_shape(phone,d,p);
    p->voiced=q8(d->voiced_q8); p->noise=q8(d->noise_q8);
    p->aspiration=q8(d->aspiration_q8); p->amplitude=q8(d->amp_q8);
    p->noise_center=(float)d->noise_center; p->noise_bw=(float)d->noise_bandwidth;
    p->nasal=0.0f; p->source_open=q8(d->open_q8);
    p->source_return=nanotts_clampf(1.34f-p->source_open,0.18f,0.88f);
    p->source_tilt=q8(d->tilt_q8);
}

static void apply_coarticulation(const nanotts_impl_t *impl,size_t index,
                                 float progress,const nanotts_phone_def_t *d,
                                 nanotts_params_t *p)
{
    nanotts_phone_t cur=(nanotts_phone_t)impl->events[index].phone;
    nanotts_phone_t prev=NANOTTS_PH_SILENCE,next=NANOTTS_PH_SILENCE;
    bool hp=adjacent(impl,index,-1,&prev),hn=adjacent(impl,index,1,&next);
    unsigned i;
    if(d->kind==NANOTTS_KIND_GLIDE && hn && nanotts_phone_is_vowel(next)){
        const nanotts_phone_def_t *n=nanotts_phone_def(next);
        float t=nanotts_smoothstep(progress);
        for(i=0u;i<4u;++i)
            p->f[i]=nanotts_lerp(formant(d,i,false),formant(n,i,false),t);
    }
    if(d->kind==NANOTTS_KIND_GLIDE){
        nanotts_phone_t onset; size_t onset_distance=0u;
        if(syllable_onset(impl,index,&onset,&onset_distance) && onset_distance>0u){
            nanotts_phone_kind_t k=(nanotts_phone_kind_t)nanotts_phone_def(onset)->kind;
            float fade=1.0f-0.25f*nanotts_smoothstep(progress);
            if(k==NANOTTS_KIND_STOP_ASP||k==NANOTTS_KIND_AFFRICATE_ASP||k==NANOTTS_KIND_FRICATIVE_H){
                p->aspiration+=0.28f*fade;p->source_open=nanotts_clampf(p->source_open+0.10f*fade,0.30f,0.92f);
            }else if(k==NANOTTS_KIND_STOP_LAX||k==NANOTTS_KIND_AFFRICATE_LAX){
                p->aspiration+=0.12f*fade;p->source_open=nanotts_clampf(p->source_open+0.08f*fade,0.30f,0.92f);
            }else if(k==NANOTTS_KIND_STOP_TENSE||k==NANOTTS_KIND_AFFRICATE_TENSE||k==NANOTTS_KIND_FRICATIVE_TENSE){
                p->source_open=nanotts_clampf(p->source_open-0.08f*fade,0.22f,0.90f);
                p->source_tilt=nanotts_clampf(p->source_tilt-0.11f*fade,0.04f,0.95f);
            }
        }
    }
    if(d->kind==NANOTTS_KIND_FRICATIVE_H && hn && nanotts_phone_is_vowel(next)){
        const nanotts_phone_def_t *n=nanotts_phone_def(next);
        for(i=0u;i<4u;++i)p->f[i]=formant(n,i,false);
        p->noise_center=p->f[1]; p->noise_bw=1750.0f; return;
    }
    if(nanotts_phone_is_vowel(cur)){
        float locus[4];
        if(hp&&consonant_locus(prev,p,locus)&&progress<0.19f){
            float t=nanotts_smoothstep(progress/0.19f);
            for(i=0u;i<4u;++i)p->f[i]=nanotts_lerp(locus[i],p->f[i],t);
        }
        if(hn&&consonant_locus(next,p,locus)&&progress>0.81f){
            float t=nanotts_smoothstep((progress-0.81f)/0.19f);
            for(i=0u;i<4u;++i)p->f[i]=nanotts_lerp(p->f[i],locus[i],t);
        }
        {
            nanotts_phone_t onset; size_t onset_distance=0u;
            if(syllable_onset(impl,index,&onset,&onset_distance)){
                nanotts_phone_kind_t k=(nanotts_phone_kind_t)nanotts_phone_def(onset)->kind;
                float carry=onset_distance>1u?0.74f:1.0f;
                float fade=carry*(1.0f-nanotts_smoothstep(progress/0.42f));
                if(k==NANOTTS_KIND_STOP_ASP||k==NANOTTS_KIND_AFFRICATE_ASP||k==NANOTTS_KIND_FRICATIVE_H){
                    p->aspiration+=0.40f*fade; p->voiced*=1.0f-0.25f*fade;
                    p->source_open=nanotts_clampf(p->source_open+0.13f*fade,0.30f,0.92f);
                    p->source_tilt=nanotts_clampf(p->source_tilt+0.13f*fade,0.1f,0.96f);
                }else if(k==NANOTTS_KIND_STOP_LAX||k==NANOTTS_KIND_AFFRICATE_LAX){
                    p->aspiration+=0.19f*fade; p->voiced*=1.0f-0.12f*fade;
                    p->source_open=nanotts_clampf(p->source_open+0.14f*fade,0.30f,0.92f);
                    p->source_tilt=nanotts_clampf(p->source_tilt+0.16f*fade,0.08f,0.96f);
                }else if(k==NANOTTS_KIND_STOP_TENSE||k==NANOTTS_KIND_AFFRICATE_TENSE||k==NANOTTS_KIND_FRICATIVE_TENSE){
                    p->source_open=nanotts_clampf(p->source_open-0.13f*fade,0.22f,0.90f);
                    p->source_tilt=nanotts_clampf(p->source_tilt-0.18f*fade,0.04f,0.95f);
                    p->amplitude*=1.0f+0.055f*fade;
                }
            }
        }
        if((hp&&(prev==NANOTTS_PH_N||prev==NANOTTS_PH_M||prev==NANOTTS_PH_NG))||
           (hn&&(next==NANOTTS_PH_N||next==NANOTTS_PH_M||next==NANOTTS_PH_NG))){
            nanotts_phone_t nasal_phone=hp?prev:next;
            float edge=hp?(1.0f-nanotts_smoothstep(progress/0.24f)):
                          nanotts_smoothstep((progress-0.76f)/0.24f);
            p->nasal=nanotts_clampf(p->nasal+0.38f*edge,0.0f,1.0f);
            set_nasal_profile(p,nasal_phone);
        }
    }else if(nanotts_phone_is_sonorant(cur)){
        if(hp&&nanotts_phone_is_vowel(prev)&&progress<0.20f){
            const nanotts_phone_def_t *q=nanotts_phone_def(prev);
            float t=nanotts_smoothstep(progress/0.20f);
            for(i=0u;i<4u;++i)p->f[i]=nanotts_lerp(formant(q,i,true),p->f[i],t);
        }
        if(hn&&nanotts_phone_is_vowel(next)&&progress>0.70f){
            const nanotts_phone_def_t *q=nanotts_phone_def(next);
            float t=nanotts_smoothstep((progress-0.70f)/0.30f);
            for(i=0u;i<4u;++i)p->f[i]=nanotts_lerp(p->f[i],formant(q,i,false),t);
        }
    }
    if((cur==NANOTTS_PH_S_LAX||cur==NANOTTS_PH_S_TENSE)&&hn&&
       (next==NANOTTS_PH_I||next==NANOTTS_PH_Y||next==NANOTTS_PH_UI)){
        p->noise_center*=0.76f; p->noise_bw*=0.84f;
    }
}

static float minf_local(float a,float b){return a<b?a:b;}

static void kind_envelope(const nanotts_impl_t *impl,size_t index,float x,
                          float elapsed_ms,float total_ms,
                          const nanotts_phone_def_t *d,nanotts_params_t *p)
{
    nanotts_phone_kind_t k=(nanotts_phone_kind_t)d->kind;
    nanotts_phone_t phone=(nanotts_phone_t)impl->events[index].phone;
    nanotts_phone_t prev=NANOTTS_PH_SILENCE,next=NANOTTS_PH_SILENCE;
    bool has_prev=adjacent(impl,index,-1,&prev),has_next=adjacent(impl,index,1,&next);
    bool intervocalic=has_prev&&has_next&&nanotts_phone_is_sonorant(prev)&&
                      nanotts_phone_is_vowel(next);
    bool coda_sonorant=(k==NANOTTS_KIND_NASAL||k==NANOTTS_KIND_LATERAL)&&
        has_prev&&nanotts_phone_is_vowel(prev)&&
        (index+1u>=impl->event_count||
         nanotts_phone_is_pause((nanotts_phone_t)impl->events[index+1u].phone)||
         (impl->events[index+1u].flags&NANOTTS_EVENT_SYLLABLE_START)!=0u);
    bool ap_initial=(impl->events[index].flags&NANOTTS_EVENT_AP_START)!=0u;
    float attack=nanotts_smoothstep(elapsed_ms/6.0f);
    float release=nanotts_smoothstep((total_ms-elapsed_ms)/8.0f);
    float env=attack<release?attack:release;
    switch(k){
    case NANOTTS_KIND_PAUSE:
        p->voiced=p->noise=p->aspiration=p->amplitude=0.0f; break;
    case NANOTTS_KIND_VOWEL:
        p->amplitude*=0.76f+0.24f*env; p->aspiration+=0.008f; break;
    case NANOTTS_KIND_GLIDE:
        p->amplitude*=0.61f+0.39f*env; p->voiced*=0.94f; break;
    case NANOTTS_KIND_NASAL:
        p->amplitude*=0.70f+0.30f*env; p->nasal=0.96f;
        set_nasal_profile(p,phone);
        if(coda_sonorant){
            float tail=1.0f-0.16f*nanotts_smoothstep(x);
            p->amplitude*=0.88f*tail;p->voiced*=0.96f;
        }
        break;
    case NANOTTS_KIND_LATERAL:
        p->amplitude*=0.66f+0.34f*env;
        if(coda_sonorant){
            p->f[0]=320.0f;p->f[1]=1080.0f;p->f[2]=2480.0f;p->f[3]=3480.0f;
            p->bw[1]=205.0f;p->amplitude*=0.84f*(1.0f-0.12f*nanotts_smoothstep(x));
        }
        break;
    case NANOTTS_KIND_TAP:{
        float center=0.50f*total_ms;
        float distance=absf_local(elapsed_ms-center);
        float gate;
        if(distance<=3.2f)gate=0.10f;
        else if(distance>=6.4f)gate=1.0f;
        else gate=nanotts_lerp(0.10f,1.0f,
             nanotts_smoothstep((distance-3.2f)/3.2f));
        p->voiced*=gate;p->amplitude*=0.60f+0.40f*env;break;}
    case NANOTTS_KIND_CODA_STOP:{
        float tail=1.0f-nanotts_smoothstep(elapsed_ms/6.0f);
        p->voiced=0.08f*tail; p->noise=p->aspiration=0.0f;
        p->amplitude*=0.24f*tail; break;}
    case NANOTTS_KIND_STOP_LAX:{
        float closure=intervocalic?minf_local(34.0f,total_ms*0.52f):
                                  minf_local(ap_initial?28.0f:31.0f,total_ms*0.48f);
        float burst=4.0f;
        if(elapsed_ms<closure){
            float closure_voice=intervocalic?0.19f:0.0f;
            p->noise=p->aspiration=0.0f;p->voiced=closure_voice;p->amplitude*=0.22f;
        }else if(elapsed_ms<closure+burst){
            p->voiced=0.0f;p->noise=1.10f;p->aspiration=0.14f;p->amplitude*=1.04f;
        }else{
            float den=total_ms-closure-burst;
            float t=den>1.0f?nanotts_smoothstep((elapsed_ms-closure-burst)/den):1.0f;
            p->noise*=0.72f*(1.0f-t);p->aspiration=(ap_initial?0.34f:0.20f)*(1.0f-t);
            p->voiced=intervocalic?0.30f*t:0.0f;p->amplitude*=0.68f;
        } break;}
    case NANOTTS_KIND_STOP_TENSE:{
        float tail=16.0f,burst=3.5f,closure=total_ms-tail;
        if(closure<18.0f)closure=total_ms*0.62f;
        if(elapsed_ms<closure){p->voiced=p->noise=p->aspiration=0.0f;p->amplitude=0.0f;}
        else if(elapsed_ms<closure+burst){p->voiced=0.0f;p->noise=1.15f;p->aspiration=0.01f;p->amplitude*=1.16f;}
        else {float den=total_ms-closure-burst;
            float t=den>1.0f?nanotts_smoothstep((elapsed_ms-closure-burst)/den):1.0f;
            p->noise*=0.52f*(1.0f-t);p->aspiration=0.008f*(1.0f-t);
            p->voiced=0.0f;p->amplitude*=0.54f;} break;}
    case NANOTTS_KIND_STOP_ASP:{
        float closure=minf_local(25.0f,total_ms*0.30f),burst=5.0f;
        if(elapsed_ms<closure){p->voiced=p->noise=p->aspiration=0.0f;p->amplitude=0.0f;}
        else if(elapsed_ms<closure+burst){p->voiced=0.0f;p->noise=1.12f;p->aspiration=0.92f;p->amplitude*=1.10f;}
        else {float den=total_ms-closure-burst;
            float t=den>1.0f?nanotts_smoothstep((elapsed_ms-closure-burst)/den):1.0f;
            p->voiced=0.0f;p->noise*=0.44f*(1.0f-t);
            p->aspiration=1.03f*(1.0f-0.94f*t);p->amplitude*=0.80f;} break;}
    case NANOTTS_KIND_AFFRICATE_LAX:
    case NANOTTS_KIND_AFFRICATE_TENSE:
    case NANOTTS_KIND_AFFRICATE_ASP:{
        float closure=k==NANOTTS_KIND_AFFRICATE_TENSE?minf_local(39.0f,total_ms*0.48f):
                      minf_local(29.0f,total_ms*0.34f);
        if(elapsed_ms<closure){
            p->voiced=(k==NANOTTS_KIND_AFFRICATE_LAX&&intervocalic)?0.10f:0.0f;
            p->noise=p->aspiration=0.0f;
            p->amplitude=(k==NANOTTS_KIND_AFFRICATE_LAX&&intervocalic)?0.18f:0.0f;
        }else{
            float den=total_ms-closure;
            float t=den>1.0f?nanotts_smoothstep((elapsed_ms-closure)/den):1.0f;
            p->voiced=0.0f;p->noise*=1.0f-0.38f*t;
            p->aspiration=(k==NANOTTS_KIND_AFFRICATE_ASP?0.82f:
                           (k==NANOTTS_KIND_AFFRICATE_LAX?0.20f:0.015f))*(1.0f-0.62f*t);
            p->amplitude*=0.88f;
        } break;}
    case NANOTTS_KIND_FRICATIVE_LAX:
        p->voiced=0.0f;p->noise*=0.80f+0.20f*env;p->aspiration=0.14f;
        p->amplitude*=0.88f;break;
    case NANOTTS_KIND_FRICATIVE_TENSE:
        p->voiced=0.0f;p->noise*=0.90f+0.10f*env;p->aspiration=0.008f;
        p->amplitude*=0.92f;break;
    case NANOTTS_KIND_FRICATIVE_H:
        p->voiced=0.0f;p->noise*=0.32f;p->aspiration*=0.72f+0.28f*env;
        p->amplitude*=0.74f;
        if(intervocalic){p->noise*=0.62f;p->aspiration*=0.70f;p->amplitude*=0.82f;}
        break;
    default: break;
    }
    /* A digital cut from an arbitrary waveform value to exact zero is heard
     * as a click.  Phones adjacent to real silence therefore receive a short
     * raised-cosine edge.  Connected phones keep full overlap and are handled
     * by the parameter smoother below. */
    if(k!=NANOTTS_KIND_PAUSE){
        float edge=1.0f;
        if(!has_prev)edge*=nanotts_smoothstep(elapsed_ms/5.0f);
        if(!has_next)edge*=nanotts_smoothstep((total_ms-elapsed_ms)/7.0f);
        p->amplitude*=edge;
    }
    if(phone==NANOTTS_PH_NG){p->f[1]=1120.0f;p->f[2]=2320.0f;}
}

static void params_for_event(const nanotts_impl_t *impl,size_t index,float progress,
                             float elapsed_ms,float total_ms,nanotts_params_t *p)
{
    nanotts_phone_t ph=(nanotts_phone_t)impl->events[index].phone;
    const nanotts_phone_def_t *d=nanotts_phone_def(ph);
    base_params(ph,d,progress,p); apply_coarticulation(impl,index,progress,d,p);
    kind_envelope(impl,index,progress,elapsed_ms,total_ms,d,p);
    p->voiced=nanotts_clampf(p->voiced,0.0f,1.0f);
    p->noise=nanotts_clampf(p->noise,0.0f,1.25f);
    p->aspiration=nanotts_clampf(p->aspiration,0.0f,1.25f);
    p->amplitude=nanotts_clampf(p->amplitude,0.0f,1.20f);
}

static void params_silence(nanotts_params_t *p)
{
    p->voiced=0.0f; p->noise=0.0f; p->aspiration=0.0f;
    p->amplitude=0.0f; p->nasal=0.0f;
}

static void params_make_step(nanotts_params_t *step,
                             const nanotts_params_t *current,
                             const nanotts_params_t *target,float reciprocal)
{
    unsigned i;
    memset(step,0,sizeof(*step));
#define STEP(field) step->field=(target->field-current->field)*reciprocal
    for(i=0u;i<4u;++i)
        step->detail_gain[i]=(target->detail_gain[i]-current->detail_gain[i])*reciprocal;
    STEP(voiced); STEP(noise); STEP(aspiration); STEP(amplitude); STEP(nasal);
    STEP(source_open); STEP(source_return); STEP(source_tilt);
#undef STEP
}

static void params_advance(nanotts_params_t *p,const nanotts_params_t *step)
{
    unsigned i;
#define ADVANCE(field) p->field+=step->field
    for(i=0u;i<4u;++i)p->detail_gain[i]+=step->detail_gain[i];
    ADVANCE(voiced); ADVANCE(noise); ADVANCE(aspiration);
    ADVANCE(amplitude); ADVANCE(nasal);
    ADVANCE(source_open); ADVANCE(source_return); ADVANCE(source_tilt);
#undef ADVANCE
}

static void params_snap_sample_fields(nanotts_params_t *p,
                                      const nanotts_params_t *target)
{
    unsigned i;
    for(i=0u;i<4u;++i)p->detail_gain[i]=target->detail_gain[i];
    p->voiced=target->voiced;p->noise=target->noise;
    p->aspiration=target->aspiration;p->amplitude=target->amplitude;
    p->nasal=target->nasal;p->source_open=target->source_open;
    p->source_return=target->source_return;p->source_tilt=target->source_tilt;
}

static void design_filters(nanotts_filter_targets_t *t,
                           const nanotts_params_t *p,float sr)
{
    unsigned i;
    for(i=0u;i<4u;++i)
        biquad_resonator(&t->voiced[i],p->f[i],p->bw[i],sr);
    for(i=0u;i<4u;++i)
        biquad_bandpass(&t->voiced_detail[i],p->f[i],
                        p->bw[i]*(i>=2u?2.7f:2.1f),sr);
    biquad_bandpass(&t->noise[0],p->noise_center*0.66f,p->noise_bw*0.92f,sr);
    biquad_bandpass(&t->noise[1],p->noise_center,p->noise_bw,sr);
    biquad_bandpass(&t->noise[2],p->noise_center*1.34f,p->noise_bw*1.10f,sr);
    for(i=0u;i<2u;++i){
        biquad_bandpass(&t->nasal[i],p->nasal_pole[i],p->nasal_bw[i],sr);
        biquad_notch(&t->nasal_zero[i],p->nasal_zero[i],
                     p->nasal_zero_bw[i],sr);
    }
}

static void set_filter_coefficients(nanotts_impl_t *impl,
                                    const nanotts_filter_targets_t *t)
{
    unsigned i;
    for(i=0u;i<4u;++i)biquad_set_coeffs(&impl->voiced_filters[i],&t->voiced[i]);
    for(i=0u;i<4u;++i)biquad_set_coeffs(&impl->voiced_detail_filters[i],&t->voiced_detail[i]);
    for(i=0u;i<3u;++i)biquad_set_coeffs(&impl->noise_filters[i],&t->noise[i]);
    for(i=0u;i<2u;++i){
        biquad_set_coeffs(&impl->nasal_filters[i],&t->nasal[i]);
        biquad_set_coeffs(&impl->nasal_zero_filters[i],&t->nasal_zero[i]);
    }
}

static void approach_filter_coefficients(nanotts_impl_t *impl,
                                         const nanotts_filter_targets_t *t,
                                         float amount)
{
    unsigned i;
    for(i=0u;i<4u;++i)biquad_approach_coeffs(&impl->voiced_filters[i],&t->voiced[i],amount);
    for(i=0u;i<4u;++i)biquad_approach_coeffs(&impl->voiced_detail_filters[i],&t->voiced_detail[i],amount);
    for(i=0u;i<3u;++i)biquad_approach_coeffs(&impl->noise_filters[i],&t->noise[i],amount);
    for(i=0u;i<2u;++i){
        biquad_approach_coeffs(&impl->nasal_filters[i],&t->nasal[i],amount);
        biquad_approach_coeffs(&impl->nasal_zero_filters[i],&t->nasal_zero[i],amount);
    }
}

static float white_noise(nanotts_impl_t *impl)
{
    uint32_t x=impl->rng; x^=x<<13; x^=x>>17; x^=x<<5; impl->rng=x;
    return (float)((int32_t)(x>>8)-8388608)/8388608.0f;
}

static float glottal_source(nanotts_impl_t *impl,float f0,const nanotts_params_t *p)
{
    float open=nanotts_clampf(0.38f+0.30f*p->source_open,0.36f,0.72f);
    float return_len=nanotts_clampf(0.10f+0.25f*p->source_return,0.10f,0.34f);
    float phase=impl->phase,flow,derivative,tilt;
    float inc=f0*(1.0f+0.0028f*impl->cycle_jitter)/(float)impl->sample_rate;
    impl->phase+=inc;
    if(impl->phase>=1.0f){
        impl->phase-=floor_positive(impl->phase);
        impl->cycle_jitter=0.72f*impl->cycle_jitter+0.28f*white_noise(impl);
        impl->cycle_shimmer=0.76f*impl->cycle_shimmer+0.24f*white_noise(impl);
    }
    if(phase<open){float t=phase/open;flow=nanotts_smoothstep(t);}
    else if(phase<open+return_len){float t=(phase-open)/return_len;flow=1.0f-nanotts_smoothstep(t);}
    else flow=0.0f;
    derivative=flow-impl->previous_glottal_flow; impl->previous_glottal_flow=flow;
    /* Two tiny one-pole sections give controllable source tilt. */
    tilt=nanotts_clampf(0.40f+0.50f*p->source_tilt,0.35f,0.92f);
    impl->source_tilt_1+= (derivative-impl->source_tilt_1)*(1.0f-0.76f*tilt);
    impl->source_tilt_2+= (impl->source_tilt_1-impl->source_tilt_2)*(1.0f-0.64f*tilt);
    return impl->source_tilt_2*(5.1f*(1.0f+0.010f*impl->cycle_shimmer));
}

static float synth_sample(nanotts_impl_t *impl,const nanotts_params_t *p,
                          float f0,float volume)
{
    static const float nw[3]={0.38f,1.0f,0.48f};
    float src=glottal_source(impl,f0,p),noise=white_noise(impl);
    float high=noise-0.67f*impl->previous_noise,cascade=src,nout=0.0f;
    float detail[4],oral,nasal_low,nasal_high,notched,voiced,mix,dc,a;
    float aspiration; unsigned i;
    impl->previous_noise=noise;
    for(i=0u;i<4u;++i)cascade=biquad_process(&impl->voiced_filters[i],cascade);
    for(i=0u;i<4u;++i)detail[i]=biquad_process(&impl->voiced_detail_filters[i],src);
    for(i=0u;i<3u;++i)nout+=nw[i]*biquad_process(&impl->noise_filters[i],high);
    oral=0.60f*cascade+p->detail_gain[0]*detail[0]+
         p->detail_gain[1]*detail[1]+p->detail_gain[2]*detail[2]+
         p->detail_gain[3]*detail[3];
    nasal_low=biquad_process(&impl->nasal_filters[0],src);
    nasal_high=biquad_process(&impl->nasal_filters[1],src);
    notched=biquad_process(&impl->nasal_zero_filters[0],oral);
    notched=biquad_process(&impl->nasal_zero_filters[1],notched);
    voiced=(1.0f-p->nasal)*oral+p->nasal*(0.43f*notched+7.8f*nasal_low+3.1f*nasal_high);
    impl->aspiration_lowpass+=(noise-impl->aspiration_lowpass)*0.115f;
    aspiration=0.68f*impl->aspiration_lowpass+0.32f*noise;
    mix=p->voiced*voiced+p->noise*0.29f*nout+
        p->aspiration*(0.105f*aspiration+0.115f*nout);
    mix*=p->amplitude;
    dc=mix-impl->dc_prev_x+0.992f*impl->dc_prev_y;
    impl->dc_prev_x=mix;impl->dc_prev_y=dc;
    {
        float bright=dc-impl->radiation_previous;
        float radiation=0.045f+0.075f*p->voiced;
        impl->radiation_previous=dc;
        dc+=radiation*bright;
    }
    a=absf_local(dc);dc=dc/(1.0f+0.24f*a);
    return nanotts_clampf(dc*1.18f*volume,-0.86f,0.86f);
}

static void decay_state(nanotts_impl_t *impl)
{
    unsigned i; float dc;
    for(i=0u;i<4u;++i)(void)biquad_process(&impl->voiced_filters[i],0.0f);
    for(i=0u;i<4u;++i)(void)biquad_process(&impl->voiced_detail_filters[i],0.0f);
    for(i=0u;i<3u;++i)(void)biquad_process(&impl->noise_filters[i],0.0f);
    for(i=0u;i<2u;++i){
        (void)biquad_process(&impl->nasal_filters[i],0.0f);
        (void)biquad_process(&impl->nasal_zero_filters[i],0.0f);
    }
    impl->source_tilt_1*=0.985f;impl->source_tilt_2*=0.985f;
    impl->previous_noise*=0.92f;impl->aspiration_lowpass*=0.94f;
    impl->radiation_previous*=0.96f;impl->cycle_jitter*=0.98f;impl->cycle_shimmer*=0.98f;
    dc=-impl->dc_prev_x+0.992f*impl->dc_prev_y;impl->dc_prev_x=0.0f;impl->dc_prev_y=dc;
}

static void clear_signal_state(nanotts_impl_t *impl)
{
    unsigned i;
    impl->phase=0.0f;
    impl->previous_glottal_flow=impl->source_tilt_1=impl->source_tilt_2=0.0f;
    impl->previous_noise=impl->dc_prev_x=impl->dc_prev_y=0.0f;
    impl->aspiration_lowpass=impl->radiation_previous=0.0f;
    impl->cycle_jitter=impl->cycle_shimmer=0.0f;
    for(i=0u;i<4u;++i)biquad_clear(&impl->voiced_filters[i]);
    for(i=0u;i<4u;++i)biquad_clear(&impl->voiced_detail_filters[i]);
    for(i=0u;i<3u;++i)biquad_clear(&impl->noise_filters[i]);
    for(i=0u;i<2u;++i){
        biquad_clear(&impl->nasal_filters[i]);
        biquad_clear(&impl->nasal_zero_filters[i]);
    }
}

static void clear_state(nanotts_impl_t *impl)
{
    impl->rng=0x6d2b79f5u;
    impl->audio_count=0u;
    clear_signal_state(impl);
}

static int flush_audio(nanotts_impl_t *impl,nanotts_write_fn write,void *user)
{
    int r=0;if(impl->audio_count){r=write(user,impl->audio_block,impl->audio_count);impl->audio_count=0u;}return r;
}
static int emit_sample(nanotts_impl_t *impl,int16_t sample,nanotts_write_fn write,void *user)
{
    impl->audio_block[impl->audio_count++]=sample;
    return impl->audio_count==NANOTTS_AUDIO_BLOCK?flush_audio(impl,write,user):0;
}
static nanotts_result_t emit_silence(nanotts_impl_t *impl,size_t n,nanotts_write_fn w,void *u)
{
    size_t i;for(i=0;i<n;++i)if(emit_sample(impl,0,w,u))return NANOTTS_ERR_CALLBACK_ABORTED;
    return NANOTTS_OK;
}

static size_t event_samples(const nanotts_event_t *e,const nanotts_phone_def_t *d,
                            uint32_t sample_rate,const nanotts_options_t *o)
{
    uint32_t rate=o->rate_q8?o->rate_q8:256u;
    uint64_t x=(uint64_t)d->duration_ms*(uint64_t)e->duration_percent*(uint64_t)sample_rate*256u;
    x/=(uint64_t)100u*1000u*(uint64_t)rate;
    return (size_t)(x?x:1u);
}

static size_t phrase_end(const nanotts_impl_t *impl,size_t start)
{
    size_t i;for(i=start;i<impl->event_count;++i){
        const nanotts_event_t *e=&impl->events[i];
        if((e->flags&NANOTTS_EVENT_PHRASE_END)||e->phone==(uint8_t)NANOTTS_PH_PHRASE_PAUSE)return i;
    }return impl->event_count;
}
static nanotts_final_tone_t phrase_tone(const nanotts_impl_t *impl,size_t end,uint8_t requested)
{
    if(requested!=(uint8_t)NANOTTS_FINAL_AUTO)return(nanotts_final_tone_t)requested;
    if(end<impl->event_count){uint8_t f=impl->events[end].flags;
        if(f&NANOTTS_EVENT_QUESTION)return NANOTTS_FINAL_RISE;
        if(f&NANOTTS_EVENT_CONTINUATION)return NANOTTS_FINAL_CONTINUE;
    }return NANOTTS_FINAL_FALL;
}
static size_t phrase_samples(const nanotts_impl_t *impl,size_t start,size_t end,
                             const nanotts_options_t *o)
{
    size_t i,total=0u;for(i=start;i<end&&i<impl->event_count;++i){
        nanotts_phone_t p=(nanotts_phone_t)impl->events[i].phone;
        total+=event_samples(&impl->events[i],nanotts_phone_def(p),impl->sample_rate,o);
    }return total?total:1u;
}

static float contour(nanotts_final_tone_t tone,float x)
{
    float semitones;
    x=nanotts_clampf(x,0.0f,1.0f);
    semitones=-0.70f*x; /* gentle declination across an intonational phrase */
    if(tone==NANOTTS_FINAL_RISE&&x>0.68f)semitones+=4.7f*nanotts_smoothstep((x-0.68f)/0.32f);
    else if(tone==NANOTTS_FINAL_FALL&&x>0.72f)semitones-=2.5f*nanotts_smoothstep((x-0.72f)/0.28f);
    else if(tone==NANOTTS_FINAL_CONTINUE&&x>0.70f)semitones+=2.0f*nanotts_smoothstep((x-0.70f)/0.30f);
    return exp2_local(semitones/12.0f);
}

static float event_pitch(const nanotts_impl_t *impl,size_t index,float x)
{
    float a=(float)impl->events[index].pitch_semitones_q4/16.0f,b=a;
    if(index+1u<impl->event_count&&!nanotts_phone_is_pause((nanotts_phone_t)impl->events[index+1u].phone))
        b=(float)impl->events[index+1u].pitch_semitones_q4/16.0f;
    return exp2_local(nanotts_lerp(a,b,nanotts_smoothstep(x))/12.0f);
}

static float render_f0(const nanotts_impl_t *impl,size_t event_index,
                       float event_x,float phrase_x,float global_pitch,
                       nanotts_final_tone_t tone,const nanotts_phone_def_t *d)
{
    float f0=KO_BASE_PITCH_HZ*global_pitch*event_pitch(impl,event_index,event_x)*
             contour(tone,phrase_x);
    int8_t cue=d->onset_pitch_q4; float envelope=1.0f-nanotts_smoothstep(event_x/0.44f);
    nanotts_phone_kind_t kind=(nanotts_phone_kind_t)d->kind;
    if(kind==NANOTTS_KIND_VOWEL||kind==NANOTTS_KIND_GLIDE){
        nanotts_phone_t onset;size_t distance=0u;
        if(syllable_onset(impl,event_index,&onset,&distance)&&distance>0u){
            const nanotts_phone_def_t *od=nanotts_phone_def(onset);
            nanotts_phone_kind_t ok=(nanotts_phone_kind_t)od->kind;
            float floor_strength=0.38f;
            cue=od->onset_pitch_q4;
            if(ok==NANOTTS_KIND_STOP_ASP||ok==NANOTTS_KIND_AFFRICATE_ASP||
               ok==NANOTTS_KIND_FRICATIVE_H)floor_strength=0.72f;
            else if(ok==NANOTTS_KIND_STOP_TENSE||ok==NANOTTS_KIND_AFFRICATE_TENSE||
                    ok==NANOTTS_KIND_FRICATIVE_TENSE)floor_strength=0.48f;
            else if(ok==NANOTTS_KIND_FRICATIVE_LAX)floor_strength=0.55f;
            if(kind==NANOTTS_KIND_GLIDE)
                envelope=1.0f-(1.0f-floor_strength)*0.35f*nanotts_smoothstep(event_x);
            else envelope=(distance>1u?0.80f:1.0f)*
                (1.0f-(1.0f-floor_strength)*nanotts_smoothstep(event_x));
        }else cue=0;
    }
    f0*=exp2_local((float)cue*envelope/192.0f);
    return nanotts_clampf(f0,62.0f,350.0f);
}

nanotts_result_t nanotts_synth_render(nanotts_impl_t *impl,
    const nanotts_options_t *o,nanotts_write_fn write,void *user)
{
    size_t ei,ps=0u,pe=phrase_end(impl,0u),ptotal,pelapsed=0u;
    nanotts_final_tone_t ptone;float global_pitch,volume;
    nanotts_params_t current_params,target_params,param_step;
    nanotts_filter_targets_t filter_targets;
    bool params_ready=false;
    unsigned coefficient_phase=0u;
    float coefficient_amount;
    nanotts_result_t r;
    if(!impl||!o||!write)return NANOTTS_ERR_ARGUMENT;
    if((o->rate_q8&&(o->rate_q8<96u||o->rate_q8>640u))||
       o->pitch_cents < -1200||o->pitch_cents>1200||
       o->final_tone>(uint8_t)NANOTTS_FINAL_AUTO)return NANOTTS_ERR_ARGUMENT;
    clear_state(impl);
    memset(&current_params,0,sizeof(current_params));
    memset(&target_params,0,sizeof(target_params));
    memset(&param_step,0,sizeof(param_step));
    global_pitch=exp2_local((float)o->pitch_cents/1200.0f);
    volume=(float)o->volume/255.0f;ptotal=phrase_samples(impl,ps,pe,o);
    ptone=phrase_tone(impl,pe,o->final_tone);
    /* Sample-rate-normalized de-zippering.  Parameters settle in roughly
     * half a millisecond; coefficients settle slightly faster. */
    {
        float per_sample=nanotts_clampf(5000.0f/(float)impl->sample_rate,0.14f,0.64f);
        float remaining=1.0f-per_sample;
        unsigned i;
        float stride_remaining=1.0f;
        for(i=0u;i<NANOTTS_COEFF_STRIDE;++i)stride_remaining*=remaining;
        coefficient_amount=nanotts_clampf(1.0f-stride_remaining,0.16f,0.72f);
    }
    r=emit_silence(impl,impl->sample_rate/50u,write,user);if(r!=NANOTTS_OK)return r;
    for(ei=0u;ei<impl->event_count;++ei){
        const nanotts_event_t *e=&impl->events[ei];
        nanotts_phone_t ph=(nanotts_phone_t)e->phone;
        const nanotts_phone_def_t *d=nanotts_phone_def(ph);
        size_t total=event_samples(e,d,impl->sample_rate,o);
        size_t frame=(size_t)impl->sample_rate*NANOTTS_CONTROL_MS/1000u;
        size_t pos=0u;float total_ms=1000.0f*(float)total/(float)impl->sample_rate;
        bool is_pause=d->kind==NANOTTS_KIND_PAUSE;
        if(!frame)frame=1u;
        if(ei>pe){ps=ei;pe=phrase_end(impl,ei);ptotal=phrase_samples(impl,ps,pe,o);
            pelapsed=0u;ptone=phrase_tone(impl,pe,o->final_tone);}
        while(pos<total){
            size_t count=total-pos,k;if(count>frame)count=frame;
            float x0=(float)pos/(float)total;
            float x1=(float)(pos+count)/(float)total;
            float pp0=ei<pe?((float)pelapsed+(float)pos)/(float)ptotal:1.0f;
            float pp1=ei<pe?((float)pelapsed+(float)(pos+count))/(float)ptotal:1.0f;
            float f0a=0.0f,f0b=0.0f,f0step=0.0f;
            if(!is_pause){
                params_for_event(impl,ei,x1,x1*total_ms,total_ms,&target_params);
                design_filters(&filter_targets,&target_params,(float)impl->sample_rate);
                if(!params_ready){
                    current_params=target_params;
                    params_silence(&current_params);
                    set_filter_coefficients(impl,&filter_targets);
                    params_ready=true;
                }
                params_make_step(&param_step,&current_params,&target_params,
                                 1.0f/(float)count);
                f0a=render_f0(impl,ei,x0,pp0,global_pitch,ptone,d);
                f0b=render_f0(impl,ei,x1,pp1,global_pitch,ptone,d);
                f0step=(f0b-f0a)/(float)count;
            }
            for(k=0u;k<count;++k){float out;int32_t pcm;
                if(is_pause){
                    decay_state(impl);out=0.0f;
                }else{
                    float f0=f0a+f0step*((float)k+0.5f);
                    params_advance(&current_params,&param_step);
                    if(coefficient_phase==0u)
                        approach_filter_coefficients(impl,&filter_targets,coefficient_amount);
                    ++coefficient_phase;
                    if(coefficient_phase>=NANOTTS_COEFF_STRIDE)coefficient_phase=0u;
                    out=synth_sample(impl,&current_params,f0,volume);
                }
                pcm=(int32_t)(out*32767.0f);if(pcm>32767)pcm=32767;if(pcm<-32768)pcm=-32768;
                if(emit_sample(impl,(int16_t)pcm,write,user))return NANOTTS_ERR_CALLBACK_ABORTED;
            }
            if(!is_pause)params_snap_sample_fields(&current_params,&target_params);
            pos+=count;
        }
        if(is_pause){
            /* The surrounding phone envelopes have reached zero.  Clear the
             * hidden resonator and source history while silence masks it so
             * the next accent phrase cannot inherit a stale transient. */
            clear_signal_state(impl);
            params_ready=false;
            coefficient_phase=0u;
        }
        if(ei<pe)pelapsed+=total;
    }
    r=emit_silence(impl,impl->sample_rate/20u,write,user);if(r!=NANOTTS_OK)return r;
    return flush_audio(impl,write,user)?NANOTTS_ERR_CALLBACK_ABORTED:NANOTTS_OK;
}
