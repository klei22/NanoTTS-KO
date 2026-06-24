/* SPDX-License-Identifier: MIT */
#include "nanotts/nanotts.h"
#include <assert.h>
#include <string.h>

static int discard(void *u,const int16_t *s,size_t n)
{ size_t *count=(size_t*)u; (void)s; *count+=n; return 0; }
static int abort_now(void *u,const int16_t *s,size_t n)
{ (void)u;(void)s;(void)n;return 1; }
static int lexicon(void *u,const char *w,size_t n,const char **p,size_t *pn)
{
    (void)u;
    if(n==strlen("서울역")&&memcmp(w,"서울역",n)==0){*p="서울력";*pn=NANOTTS_NPOS;return 1;}
    return 0;
}
int main(void)
{
    nanotts_t t; nanotts_options_t o; nanotts_parse_info_t info;
    nanotts_event_t bad={255u,0u,100u,0,0,0,0,0}; size_t samples=0u;
    assert(NANOTTS_VERSION_MAJOR==2);
    assert(nanotts_context_bytes()==NANOTTS_CONTEXT_BYTES);
    assert(nanotts_event_capacity()==NANOTTS_MAX_EVENTS);
    assert(nanotts_syllable_capacity()==NANOTTS_MAX_SYLLABLES);
    assert(nanotts_token_count(&t)==0u);
    assert(nanotts_morpheme_count(&t)==0u);
    assert(nanotts_init(NULL,16000u)==NANOTTS_ERR_ARGUMENT);
    assert(nanotts_init(&t,7999u)==NANOTTS_ERR_SAMPLE_RATE);
    assert(nanotts_init(&t,16000u)==NANOTTS_OK);
    assert(nanotts_sample_rate(&t)==16000u);
    nanotts_default_options(&o);
    assert(o.rate_q8==256u&&o.volume==220u&&o.final_tone==NANOTTS_FINAL_AUTO);
    assert(o.style==NANOTTS_KO_STYLE_MODERN_SEOUL&&o.expression_q8==128u);
    assert((o.flags&NANOTTS_OPT_LINK_EOJEOL)!=0u);
    assert(nanotts_parse_text(&t,"안녕하세요",NANOTTS_NPOS,o.flags,&info)==NANOTTS_OK);
    assert(info.syllable_count==5u&&info.event_count>5u);
    assert(info.token_count>=1u&&info.morpheme_count>=1u);
    assert(nanotts_token_count(&t)==info.token_count);
    assert(nanotts_morpheme_count(&t)==info.morpheme_count);
    assert(nanotts_morphemes(&t)!=NULL);
    assert(nanotts_event_count(&t)==info.event_count&&nanotts_events(&t)!=NULL);
    assert(nanotts_render_events(&t,&o,discard,&samples)==NANOTTS_OK&&samples>1000u);
    assert(nanotts_render_events(&t,&o,abort_now,NULL)==NANOTTS_ERR_CALLBACK_ABORTED);
    assert(nanotts_set_events(&t,&bad,1u)==NANOTTS_ERR_ARGUMENT);
    assert(nanotts_parse_text(&t,"\xf0\x28\x8c\x28",4u,o.flags,&info)==NANOTTS_ERR_UTF8);
    assert(nanotts_parse_text(&t,"🙂",NANOTTS_NPOS,o.flags,&info)==NANOTTS_ERR_UNSUPPORTED_TEXT);
    assert(nanotts_parse_text(&t,"   ",NANOTTS_NPOS,o.flags,&info)==NANOTTS_ERR_EMPTY_INPUT);
    nanotts_set_lexicon(&t,lexicon,NULL);
    assert(nanotts_parse_text(&t,"서울역",NANOTTS_NPOS,o.flags,&info)==NANOTTS_OK);
    assert(info.lexicon_hits==1u);
    nanotts_reset(&t); assert(nanotts_sample_rate(&t)==16000u);
    assert(strcmp(nanotts_version_string(),"2.0.0-ko")==0);
    assert(strcmp(nanotts_ko_style_name(NANOTTS_KO_STYLE_CLEAR_DEVICE),"clear-device")==0);
    assert(strcmp(nanotts_ko_pos_name(NANOTTS_KO_POS_PARTICLE),"particle")==0);
    assert(strcmp(nanotts_strerror(NANOTTS_OK),"success")==0);
    return 0;
}
