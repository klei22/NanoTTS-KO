/* SPDX-License-Identifier: MIT */
#include "nanotts/nanotts_pcm.h"
#include <assert.h>
#include <string.h>
typedef struct sink{uint8_t b[32];size_t n;int calls;}sink_t;
static int writeb(void *u,const uint8_t *b,size_t n){sink_t*s=(sink_t*)u;if(s->n+n>sizeof(s->b))return 1;memcpy(s->b+s->n,b,n);s->n+=n;++s->calls;return 0;}
int main(void){nanotts_pcm16le_output_t o;uint8_t scratch[4];sink_t s={{0},0,0};int16_t x[]={-32768,-1,0,1,32767};
 assert(nanotts_pcm16le_output_init(&o,scratch,sizeof(scratch),writeb,&s)==NANOTTS_OK);
 assert(nanotts_pcm16le_write_pcm(&o,x,5u)==0);assert(s.n==10u&&s.calls==3);
 assert(s.b[0]==0&&s.b[1]==0x80&&s.b[2]==0xff&&s.b[3]==0xff&&s.b[4]==0&&s.b[5]==0&&s.b[8]==0xff&&s.b[9]==0x7f);return 0;}
