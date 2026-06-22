/* SPDX-License-Identifier: MIT */
#include "nanotts/nanotts_pwm.h"
#include <assert.h>
typedef struct sink{uint16_t v[16];size_t n;int calls;}sink_t;
static int writev(void *u,const uint16_t*v,size_t n){sink_t*s=(sink_t*)u;size_t i;for(i=0;i<n;++i)s->v[s->n++]=v[i];++s->calls;return 0;}
int main(void){nanotts_pwm_output_t o;uint16_t scratch[2];sink_t s={{0},0,0};int16_t x[]={-32768,0,32767};
 assert(nanotts_pcm16_to_pwm(-32768,1023u,0)==0u);assert(nanotts_pcm16_to_pwm(32767,1023u,0)==1023u);
 assert(nanotts_pcm16_to_pwm(-32768,1023u,1)==1023u);
 assert(nanotts_pwm_output_init(&o,1023u,0,scratch,2u,writev,&s)==NANOTTS_OK);
 assert(nanotts_pwm_write_pcm(&o,x,3u)==0);assert(s.n==3u&&s.calls==2);assert(s.v[0]==0u&&s.v[1]>=511u&&s.v[2]==1023u);return 0;}
