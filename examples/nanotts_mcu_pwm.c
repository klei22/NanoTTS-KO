/* SPDX-License-Identifier: MIT */
#include "nanotts/nanotts_pwm.h"
static int pwm_dma(void *timer,const uint16_t *duty,size_t count)
{ (void)timer;(void)duty;(void)count;return 0; }
int main(void){static nanotts_t t;static uint16_t scratch[64];nanotts_options_t o;nanotts_pwm_output_t pwm;
 if(nanotts_init(&t,16000u)!=NANOTTS_OK) return 1;
 nanotts_default_options(&o);
 if(nanotts_pwm_output_init(&pwm,1023u,0,scratch,64u,pwm_dma,0)!=NANOTTS_OK)return 1;
 return nanotts_speak_text(&t,"문이 열렸습니다.",NANOTTS_NPOS,&o,nanotts_pwm_write_pcm,&pwm,0)!=NANOTTS_OK;}
