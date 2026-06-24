/* SPDX-License-Identifier: MIT */
#include "nanotts/nanotts.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct case_ { const char *text; const nanotts_phone_t *phones; size_t count; } case_t;
static void check_case(nanotts_t *t,const case_t *c)
{
    nanotts_parse_info_t info; const nanotts_event_t *e; size_t i,n=0u;
    assert(nanotts_parse_text(t,c->text,NANOTTS_NPOS,NANOTTS_OPT_LINK_EOJEOL,&info)==NANOTTS_OK);
    e=nanotts_events(t);
    for(i=0u;i<nanotts_event_count(t);++i){
        nanotts_phone_t p=(nanotts_phone_t)e[i].phone;
        if(p!=NANOTTS_PH_SILENCE&&p!=NANOTTS_PH_AP_PAUSE&&
           p!=NANOTTS_PH_SHORT_PAUSE&&p!=NANOTTS_PH_PHRASE_PAUSE){
            assert(n<c->count);
            if(p!=c->phones[n]){
                fprintf(stderr,"%s phone %lu: got %s expected %s\n",c->text,
                    (unsigned long)n,nanotts_phone_name(p),nanotts_phone_name(c->phones[n]));
            }
            assert(p==c->phones[n]); ++n;
        }
    }
    assert(n==c->count);
}
static const nanotts_phone_t p0[]={NANOTTS_PH_H,NANOTTS_PH_A,NANOTTS_PH_N,NANOTTS_PH_K_LAX,NANOTTS_PH_U,NANOTTS_PH_K_LAX,NANOTTS_PH_EO};
static const nanotts_phone_t p1[]={NANOTTS_PH_K_LAX,NANOTTS_PH_U,NANOTTS_PH_NG,NANOTTS_PH_M,NANOTTS_PH_I,NANOTTS_PH_N};
static const nanotts_phone_t p2[]={NANOTTS_PH_S_LAX_PAL,NANOTTS_PH_I,NANOTTS_PH_L,NANOTTS_PH_L,NANOTTS_PH_A};
static const nanotts_phone_t p3[]={NANOTTS_PH_M,NANOTTS_PH_EO,NANOTTS_PH_K_CODA,NANOTTS_PH_T_TENSE,NANOTTS_PH_A};
static const nanotts_phone_t p4[]={NANOTTS_PH_H,NANOTTS_PH_A,NANOTTS_PH_K_CODA,NANOTTS_PH_K_TENSE,NANOTTS_PH_Y,NANOTTS_PH_O};
static const nanotts_phone_t p5[]={NANOTTS_PH_M,NANOTTS_PH_A,NANOTTS_PH_N,NANOTTS_PH_A};
static const nanotts_phone_t p6[]={NANOTTS_PH_S_LAX_PAL,NANOTTS_PH_I,NANOTTS_PH_R,NANOTTS_PH_EO};
static const nanotts_phone_t p7[]={NANOTTS_PH_K_TENSE,NANOTTS_PH_O,NANOTTS_PH_C_ASP,NANOTTS_PH_I};
static const nanotts_phone_t p8[]={NANOTTS_PH_K_LAX,NANOTTS_PH_A,NANOTTS_PH_P_CODA,NANOTTS_PH_S_TENSE_PAL,NANOTTS_PH_I};
static const nanotts_phone_t p9[]={NANOTTS_PH_M,NANOTTS_PH_A,NANOTTS_PH_N,NANOTTS_PH_T_ASP,NANOTTS_PH_A};
static const nanotts_phone_t p10[]={NANOTTS_PH_M,NANOTTS_PH_A,NANOTTS_PH_C_ASP,NANOTTS_PH_I,NANOTTS_PH_T_LAX,NANOTTS_PH_A};
static const nanotts_phone_t p11[]={NANOTTS_PH_K_LAX,NANOTTS_PH_U,NANOTTS_PH_NG,NANOTTS_PH_N,NANOTTS_PH_I,NANOTTS_PH_P_CODA};
static const nanotts_phone_t p12[]={NANOTTS_PH_S_LAX,NANOTTS_PH_EO,NANOTTS_PH_L,NANOTTS_PH_L,NANOTTS_PH_A,NANOTTS_PH_L};
static const nanotts_phone_t p13[]={NANOTTS_PH_I,NANOTTS_PH_L,NANOTTS_PH_K_LAX,NANOTTS_PH_EO};
#if NANOTTS_ENABLE_MORPHOLOGY
static const nanotts_phone_t p14[]={NANOTTS_PH_A,NANOTTS_PH_N,NANOTTS_PH_T_TENSE,NANOTTS_PH_A};
static const nanotts_phone_t p15[]={NANOTTS_PH_C_LAX,NANOTTS_PH_EO,NANOTTS_PH_M,NANOTTS_PH_T_TENSE,NANOTTS_PH_A};
#endif
static const nanotts_phone_t p16[]={NANOTTS_PH_I,NANOTTS_PH_S_LAX,NANOTTS_PH_A};
static const nanotts_phone_t p17[]={NANOTTS_PH_H,NANOTTS_PH_A,NANOTTS_PH_N,NANOTTS_PH_N,NANOTTS_PH_I,NANOTTS_PH_L};
static const case_t cases[]={
 {"한국어",p0,sizeof(p0)/sizeof(p0[0])},{"국민",p1,sizeof(p1)/sizeof(p1[0])},
 {"신라",p2,sizeof(p2)/sizeof(p2[0])},{"먹다",p3,sizeof(p3)/sizeof(p3[0])},
 {"학교",p4,sizeof(p4)/sizeof(p4[0])},{"많아",p5,sizeof(p5)/sizeof(p5[0])},
 {"싫어",p6,sizeof(p6)/sizeof(p6[0])},{"꽃이",p7,sizeof(p7)/sizeof(p7[0])},
 {"값이",p8,sizeof(p8)/sizeof(p8[0])},{"많다",p9,sizeof(p9)/sizeof(p9[0])},
 {"맞히다",p10,sizeof(p10)/sizeof(p10[0])},{"국립",p11,sizeof(p11)/sizeof(p11[0])},
 {"설날",p12,sizeof(p12)/sizeof(p12[0])},{"읽어",p13,sizeof(p13)/sizeof(p13[0])},
#if NANOTTS_ENABLE_MORPHOLOGY
 {"앉다",p14,sizeof(p14)/sizeof(p14[0])},{"젊다",p15,sizeof(p15)/sizeof(p15[0])},
#endif
 {"의사",p16,sizeof(p16)/sizeof(p16[0])},{"한 일",p17,sizeof(p17)/sizeof(p17[0])}
};
int main(void)
{
    nanotts_t t; nanotts_parse_info_t info; const nanotts_event_t *e; size_t i;
    assert(nanotts_init(&t,16000u)==NANOTTS_OK);
    for(i=0u;i<sizeof(cases)/sizeof(cases[0]);++i)check_case(&t,&cases[i]);

    assert(nanotts_parse_text(&t,"괜찮아요? 네, 좋아요.",NANOTTS_NPOS,NANOTTS_OPT_LINK_EOJEOL,&info)==NANOTTS_OK);
    e=nanotts_events(&t);{int q=0,cont=0,phrase=0;
      for(i=0u;i<nanotts_event_count(&t);++i){q+=(e[i].flags&NANOTTS_EVENT_QUESTION)!=0u;cont+=(e[i].flags&NANOTTS_EVENT_CONTINUATION)!=0u;phrase+=(e[i].flags&NANOTTS_EVENT_PHRASE_END)!=0u;}
      assert(q==1&&cont==1&&phrase>=3);
    }
    assert(nanotts_parse_text(&t,"나노 음성 합성기",NANOTTS_NPOS,NANOTTS_OPT_LINK_EOJEOL,&info)==NANOTTS_OK);
    e=nanotts_events(&t);{int starts=0,pauses=0;
      for(i=0u;i<nanotts_event_count(&t);++i){starts+=(e[i].flags&NANOTTS_EVENT_AP_START)!=0u;pauses+=e[i].phone==NANOTTS_PH_AP_PAUSE;}
      assert(starts==1&&pauses==0);
    }
    assert(nanotts_parse_text(&t,"한 일",NANOTTS_NPOS,0u,&info)==NANOTTS_OK);
    e=nanotts_events(&t);{int consecutive_n=0;
      for(i=1u;i<nanotts_event_count(&t);++i)
        if(e[i-1u].phone==NANOTTS_PH_N&&e[i].phone==NANOTTS_PH_N)consecutive_n=1;
      assert(!consecutive_n);
    }
    assert(nanotts_parse_text(&t,"10000 007 AI",NANOTTS_NPOS,NANOTTS_OPT_LINK_EOJEOL,&info)==NANOTTS_OK);
    assert(info.syllable_count>5u);
#if NANOTTS_ENABLE_BUILTIN_LEXICON
    {
      static const char *const lexical[]={"갈등","발전","물질","넓게","떫지"};
      size_t j;
      for(j=0u;j<sizeof(lexical)/sizeof(lexical[0]);++j){
        assert(nanotts_parse_text(&t,lexical[j],NANOTTS_NPOS,NANOTTS_OPT_LINK_EOJEOL,&info)==NANOTTS_OK);
        assert(info.lexicon_hits==1u);
      }
    }
#endif
    puts("Korean G2P tests passed");return 0;
}
