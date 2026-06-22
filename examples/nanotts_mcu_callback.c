/* SPDX-License-Identifier: MIT */
#include "nanotts/nanotts.h"
/* Replace with an I2S/DAC DMA queue. Return nonzero to abort. */
static int audio_write(void *device,const int16_t *samples,size_t count)
{ (void)device;(void)samples;(void)count;return 0; }
int main(void){static nanotts_t t;nanotts_options_t o;
 if(nanotts_init(&t,16000u)!=NANOTTS_OK) return 1;
 nanotts_default_options(&o);
 return nanotts_speak_text(&t,"배터리가 부족합니다.",NANOTTS_NPOS,&o,audio_write,0,0)!=NANOTTS_OK;}
