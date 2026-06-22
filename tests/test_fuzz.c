/* SPDX-License-Identifier: MIT */
#include "nanotts/nanotts.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
static uint32_t rng=0x13579bdfu;
static uint32_t rnd(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static size_t utf8(uint32_t cp,char *p){
 if(cp<0x800u){p[0]=(char)(0xc0u|(cp>>6));p[1]=(char)(0x80u|(cp&63u));return 2u;}
 p[0]=(char)(0xe0u|(cp>>12));p[1]=(char)(0x80u|((cp>>6)&63u));p[2]=(char)(0x80u|(cp&63u));return 3u;}
static int discard(void*u,const int16_t*s,size_t n){(void)u;(void)s;(void)n;return 0;}
int main(void){nanotts_t t;nanotts_options_t o;unsigned c;
 assert(nanotts_init(&t,16000u)==NANOTTS_OK);nanotts_default_options(&o);
 for(c=0u;c<20000u;++c){char text[256];size_t n=0u;unsigned syllables=1u+rnd()%18u,i;nanotts_parse_info_t info;nanotts_result_t r;
  for(i=0u;i<syllables;++i){uint32_t cp=0xac00u+rnd()%11172u;n+=utf8(cp,text+n);
   if(i+1u<syllables&&rnd()%5u==0u)text[n++]=' ';}
  if(rnd()%3u==0u) text[n++]=(rnd()&1u)?'?':'.';
  text[n]='\0';
  r=nanotts_parse_text(&t,text,n,o.flags,&info);assert(r==NANOTTS_OK);assert(info.event_count>0u&&info.event_count<=nanotts_event_capacity());
  {const nanotts_event_t*e=nanotts_events(&t);size_t j;for(j=0u;j<info.event_count;++j){assert(e[j].phone<NANOTTS_PH_COUNT);assert(e[j].duration_percent>0u);}}
  if(c%2000u==0u)assert(nanotts_render_events(&t,&o,discard,0)==NANOTTS_OK);
 }
 return 0;}
