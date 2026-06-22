/* SPDX-License-Identifier: MIT */
#include "nanotts/nanotts.h"
#include <string.h>
static int lookup(void *u,const char *word,size_t n,const char **spoken,size_t *spoken_n)
{(void)u;if(n==strlen("서울역")&&!memcmp(word,"서울역",n)){*spoken="서울력";*spoken_n=NANOTTS_NPOS;return 1;}return 0;}
static int discard(void*u,const int16_t*s,size_t n){(void)u;(void)s;(void)n;return 0;}
int main(void){nanotts_t t;nanotts_options_t o;if(nanotts_init(&t,16000u)!=NANOTTS_OK)return 1;
 nanotts_set_lexicon(&t,lookup,0);nanotts_default_options(&o);
 return nanotts_speak_text(&t,"서울역입니다.",NANOTTS_NPOS,&o,discard,0,0)!=NANOTTS_OK;}
