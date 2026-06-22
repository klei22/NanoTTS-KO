/* SPDX-License-Identifier: MIT */
#include "nanotts/nanotts.h"
#include <assert.h>
#include <stdint.h>

typedef struct stats { uint64_t n,nonzero,zeros; int16_t min,max; uint32_t hash; } stats_t;
static int collect(void *u,const int16_t *s,size_t n)
{
    stats_t *x=(stats_t*)u;size_t i;
    for(i=0u;i<n;++i){uint16_t v=(uint16_t)s[i];
      if(s[i])++x->nonzero;else ++x->zeros;if(s[i]<x->min)x->min=s[i];if(s[i]>x->max)x->max=s[i];
      x->hash^=(uint8_t)(v&0xffu);x->hash*=16777619u;x->hash^=(uint8_t)(v>>8);x->hash*=16777619u;}
    x->n+=n;return 0;
}
static stats_t render(const char *text)
{
    nanotts_t t;nanotts_options_t o;stats_t s={0u,0u,0u,32767,-32768,2166136261u};
    assert(nanotts_init(&t,16000u)==NANOTTS_OK);nanotts_default_options(&o);
    assert(nanotts_speak_text(&t,text,NANOTTS_NPOS,&o,collect,&s,NULL)==NANOTTS_OK);return s;
}
int main(void)
{
    stats_t a=render("가다, 까다, 카다. 바다, 빠다, 파다."),b=render("안녕하세요?"),c=render("안녕하세요."),a2=render("가다, 까다, 카다. 바다, 빠다, 파다.");
    assert(a.n>16000u&&a.nonzero>a.n/4u&&a.zeros>500u);
    assert(a.min<0&&a.max>0&&a.min>-32768&&a.max<32767);
    assert(a.hash==a2.hash&&a.n==a2.n);
    assert(b.hash!=c.hash);
    return 0;
}
