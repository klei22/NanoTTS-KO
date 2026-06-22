/* SPDX-License-Identifier: MIT */
#include "nanotts_internal.h"
#include <limits.h>

#define KO_S_BASE 0xAC00u
#define KO_L_BASE 0x1100u
#define KO_V_BASE 0x1161u
#define KO_T_BASE 0x11A7u
#define KO_L_COUNT 19u
#define KO_V_COUNT 21u
#define KO_T_COUNT 28u
#define KO_N_COUNT (KO_V_COUNT * KO_T_COUNT)
#define KO_S_COUNT (KO_L_COUNT * KO_N_COUNT)
#define KO_L_LATERAL 19u
#define KO_BOUNDARY_STRENGTH_MASK 0x03u
#define KO_BOUNDARY_QUESTION 0x80u

enum {
    KO_CODA_NONE = 0,
    KO_CODA_K,
    KO_CODA_N,
    KO_CODA_T,
    KO_CODA_L,
    KO_CODA_M,
    KO_CODA_P,
    KO_CODA_NG
};

typedef struct ko_exception { const char *written; const char *spoken; } ko_exception_t;

#if NANOTTS_ENABLE_BUILTIN_LEXICON
static const ko_exception_t KO_EXCEPTIONS[] = {
    {"같이","가치"},{"굳이","구지"},{"맏이","마지"},{"밭이","바치"},
    {"해돋이","해도지"},{"붙이다","부치다"},{"닫히다","다치다"},
    {"읽다","익따"},{"읽고","일꼬"},{"밟다","밥따"},{"밟고","밥꼬"},
    {"넓다","널따"},{"핥다","할따"},{"맛있다","마싣따"},
    {"멋있다","머싣따"},{"꽃잎","꼰닙"},{"깻잎","깬닙"},
    {"색연필","생년필"},{"담요","담뇨"},{"솜이불","솜니불"},
    {"한여름","한녀름"},{"막일","망닐"},{"맨입","맨닙"},
    {"식용유","시굥뉴"},{"홑이불","혼니불"},{"삯일","상닐"},
    {"내복약","내봉냑"},{"남존여비","남존녀비"},{"신여성","신녀성"},
    {"직행열차","지캥녈차"},{"늑막염","능망념"},{"콩엿","콩녇"},
    {"눈요기","눈뇨기"},{"영업용","영엄뇽"},{"백분율","백뿐뉼"},
    {"밤윷","밤뉻"},{"들일","들릴"},{"솔잎","솔립"},
    {"설익다","설릭따"},{"물약","물략"},{"불여우","불려우"},
    {"서울역","서울력"},{"물엿","물렫"},{"휘발유","휘발류"},
    {"유들유들","유들류들"},
    /* Lexical tensification after sonorant codas in frequent compounds. */
    {"갈등","갈뜽"},{"발동","발똥"},{"절도","절또"},
    {"말살","말쌀"},{"불소","불쏘"},{"일시","일씨"},
    {"갈증","갈쯩"},{"물질","물찔"},{"발전","발쩐"},
    {"몰상식","몰쌍식"},{"불세출","불쎄출"},
    /* Frequent complex-coda forms whose surviving consonant is lexical. */
    {"넓게","널께"},{"짧게","짤께"},{"훑소","훌쏘"},{"떫지","떨찌"}
};
#endif

static uint8_t boundary_strength(uint8_t b)
{ return (uint8_t)(b & KO_BOUNDARY_STRENGTH_MASK); }

static bool utf8_decode(const char *text, size_t length, size_t *offset, uint32_t *cp)
{
    const unsigned char *s = (const unsigned char *)text;
    size_t i = *offset;
    uint32_t value;
    unsigned need, j;
    if (i >= length) return false;
    if (s[i] < 0x80u) { *cp = s[i]; *offset = i + 1u; return true; }
    if ((s[i] & 0xe0u) == 0xc0u) {
        value = (uint32_t)(s[i] & 0x1fu); need = 1u; if (value < 2u) return false;
    } else if ((s[i] & 0xf0u) == 0xe0u) {
        value = (uint32_t)(s[i] & 0x0fu); need = 2u;
    } else if ((s[i] & 0xf8u) == 0xf0u) {
        value = (uint32_t)(s[i] & 0x07u); need = 3u; if (value > 4u) return false;
    } else return false;
    if (i + need >= length) return false;
    for (j = 1u; j <= need; ++j) {
        if ((s[i+j] & 0xc0u) != 0x80u) return false;
        value = (value << 6) | (uint32_t)(s[i+j] & 0x3fu);
    }
    if ((need == 1u && value < 0x80u) || (need == 2u && value < 0x800u) ||
        (need == 3u && value < 0x10000u) || value > 0x10ffffu ||
        (value >= 0xd800u && value <= 0xdfffu)) return false;
    *cp = value; *offset = i + (size_t)need + 1u; return true;
}

static bool is_hangul(uint32_t cp)
{
    return (cp >= KO_S_BASE && cp < KO_S_BASE + KO_S_COUNT) ||
           (cp >= 0x1100u && cp <= 0x11ffu);
}
static bool is_digit(uint32_t cp) { return cp >= '0' && cp <= '9'; }
static bool is_letter(uint32_t cp)
{ return (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z'); }
static bool is_space(uint32_t cp)
{ return cp==' ' || cp=='\t' || cp=='\n' || cp=='\r' || cp==0x3000u; }
static bool is_comma(uint32_t cp)
{ return cp==',' || cp==';' || cp==':' || cp==0x3001u || cp==0xff0cu; }
static bool is_question(uint32_t cp) { return cp=='?' || cp==0xff1fu; }
static bool is_sentence_end(uint32_t cp)
{ return cp=='.' || cp=='!' || cp==0x3002u || cp==0xff01u; }

static bool append_syllable(nanotts_impl_t *impl, uint8_t l, uint8_t v,
                            uint8_t t, uint8_t boundary)
{
    nanotts_ko_syllable_t *s;
    if (impl->syllable_count >= NANOTTS_MAX_SYLLABLES) return false;
    s = &impl->syllables[impl->syllable_count++];
    s->onset=l; s->vowel=v; s->coda=t; s->boundary_before=boundary;
    return true;
}

static nanotts_result_t append_hangul(nanotts_impl_t *impl, const char *text,
    size_t length, uint8_t boundary, nanotts_parse_info_t *info)
{
    size_t off=0u; bool first=true;
    while (off < length) {
        size_t here=off; uint32_t cp; uint8_t l,v,t;
        if (!utf8_decode(text,length,&off,&cp)) {
            if (info) info->error_byte=here;
            return NANOTTS_ERR_UTF8;
        }
        if (cp >= KO_S_BASE && cp < KO_S_BASE + KO_S_COUNT) {
            uint32_t x=cp-KO_S_BASE;
            l=(uint8_t)(x/KO_N_COUNT); v=(uint8_t)((x%KO_N_COUNT)/KO_T_COUNT);
            t=(uint8_t)(x%KO_T_COUNT);
        } else if (cp >= KO_L_BASE && cp < KO_L_BASE+KO_L_COUNT) {
            size_t vat=off; uint32_t vcp;
            if (!utf8_decode(text,length,&off,&vcp) ||
                vcp<KO_V_BASE || vcp>=KO_V_BASE+KO_V_COUNT) {
                if (info) { info->error_byte=vat; info->error_codepoint=vcp; }
                return NANOTTS_ERR_UNSUPPORTED_TEXT;
            }
            l=(uint8_t)(cp-KO_L_BASE); v=(uint8_t)(vcp-KO_V_BASE); t=0u;
            if (off < length) {
                size_t tat=off; uint32_t tcp;
                if (!utf8_decode(text,length,&off,&tcp)) {
                    if (info) info->error_byte=tat;
                    return NANOTTS_ERR_UTF8;
                }
                if (tcp>KO_T_BASE && tcp<KO_T_BASE+KO_T_COUNT)
                    t=(uint8_t)(tcp-KO_T_BASE);
                else off=tat;
            }
        } else {
            if (info) { info->error_byte=here; info->error_codepoint=cp; }
            return NANOTTS_ERR_UNSUPPORTED_TEXT;
        }
        if (!append_syllable(impl,l,v,t,first?boundary:NANOTTS_BOUNDARY_NONE))
            return NANOTTS_ERR_TOO_MANY_SYLLABLES;
        first=false;
    }
    return NANOTTS_OK;
}

static const char *builtin_pronunciation(const char *word,size_t len,size_t *outlen)
{
#if NANOTTS_ENABLE_BUILTIN_LEXICON
    size_t i;
    for (i=0u;i<sizeof(KO_EXCEPTIONS)/sizeof(KO_EXCEPTIONS[0]);++i) {
        size_t n=strlen(KO_EXCEPTIONS[i].written);
        if (n==len && memcmp(word,KO_EXCEPTIONS[i].written,len)==0) {
            *outlen=strlen(KO_EXCEPTIONS[i].spoken); return KO_EXCEPTIONS[i].spoken;
        }
    }
#else
    (void)word; (void)len;
#endif
    *outlen=0u; return NULL;
}

static nanotts_result_t append_word(nanotts_impl_t *impl,const char *word,size_t len,
    uint8_t boundary,uint8_t flags,nanotts_parse_info_t *info)
{
    const char *spoken=NULL; size_t slen=0u;
    if (impl->lexicon && impl->lexicon(impl->lexicon_user,word,len,&spoken,&slen)) {
        if (!spoken) return NANOTTS_ERR_ARGUMENT;
        if (slen==NANOTTS_NPOS) slen=strlen(spoken);
        if (info) ++info->lexicon_hits;
    } else if (!(flags & NANOTTS_OPT_NO_BUILTIN_LEXICON)) {
        spoken=builtin_pronunciation(word,len,&slen);
        if (spoken && info) ++info->lexicon_hits;
    }
    if (!spoken) { spoken=word; slen=len; }
    return append_hangul(impl,spoken,slen,boundary,info);
}

static const char *const DIGITS[10] =
    {"영","일","이","삼","사","오","육","칠","팔","구"};
static nanotts_result_t append_literal(nanotts_impl_t *impl,const char *s,
    uint8_t *boundary,nanotts_parse_info_t *info)
{
    nanotts_result_t r=append_hangul(impl,s,strlen(s),*boundary,info);
    if (r==NANOTTS_OK) *boundary=NANOTTS_BOUNDARY_NONE;
    return r;
}
static nanotts_result_t append_four(nanotts_impl_t *impl,unsigned value,
    uint8_t *boundary,nanotts_parse_info_t *info)
{
    static const unsigned divs[4]={1000u,100u,10u,1u};
    static const char *const units[4]={"천","백","십",""};
    unsigned i;
    for (i=0u;i<4u;++i) {
        unsigned d=value/divs[i]; value%=divs[i];
        if (!d) continue;
        if (d!=1u || i==3u) {
            nanotts_result_t r=append_literal(impl,DIGITS[d],boundary,info);
            if (r!=NANOTTS_OK) return r;
        }
        if (*units[i]) {
            nanotts_result_t r=append_literal(impl,units[i],boundary,info);
            if (r!=NANOTTS_OK) return r;
        }
    }
    return NANOTTS_OK;
}
static nanotts_result_t append_number(nanotts_impl_t *impl,const char *digits,
    size_t len,uint8_t boundary,nanotts_parse_info_t *info)
{
    static const char *const big[]={"","만","억","조"};
    uint64_t value=0u; unsigned groups[4]={0u,0u,0u,0u}; unsigned gc=0u,i;
    if (!len) return NANOTTS_OK;
    if (len>13u || (len>1u && digits[0]=='0')) {
        for (i=0u;i<len;++i) {
            nanotts_result_t r=append_literal(impl,DIGITS[(unsigned)(digits[i]-'0')],&boundary,info);
            if (r!=NANOTTS_OK) return r;
            boundary=NANOTTS_BOUNDARY_SPACE;
        }
        return NANOTTS_OK;
    }
    for (i=0u;i<len;++i) {
        unsigned d=(unsigned)(digits[i]-'0');
        if (value>(UINT64_MAX-d)/10u) return NANOTTS_ERR_UNSUPPORTED_TEXT;
        value=value*10u+d;
    }
    if (!value) return append_literal(impl,DIGITS[0],&boundary,info);
    while (value && gc<4u) { groups[gc++]=(unsigned)(value%10000u); value/=10000u; }
    if (value) return NANOTTS_ERR_UNSUPPORTED_TEXT;
    for (i=gc;i>0u;--i) {
        unsigned gi=i-1u;
        if (groups[gi]) {
            nanotts_result_t r=NANOTTS_OK;
            if (!(gi==1u && groups[gi]==1u)) r=append_four(impl,groups[gi],&boundary,info);
            if (r!=NANOTTS_OK) return r;
            if (gi) { r=append_literal(impl,big[gi],&boundary,info); if (r!=NANOTTS_OK) return r; }
        }
    }
    return NANOTTS_OK;
}

static const char *const LATIN[26]={
    "에이","비","씨","디","이","에프","지","에이치","아이","제이",
    "케이","엘","엠","엔","오","피","큐","알","에스","티","유",
    "브이","더블유","엑스","와이","제트"};
static nanotts_result_t append_latin(nanotts_impl_t *impl,const char *s,size_t len,
    uint8_t boundary,nanotts_parse_info_t *info)
{
    size_t i;
    for (i=0u;i<len;++i) {
        unsigned char c=(unsigned char)s[i]; unsigned index; nanotts_result_t r;
        if (c>='a'&&c<='z') c=(unsigned char)(c-'a'+'A');
        index=(unsigned)(c-'A');
        r=append_literal(impl,LATIN[index],&boundary,info);
        if (r!=NANOTTS_OK) return r;
        boundary=NANOTTS_BOUNDARY_SPACE;
    }
    return NANOTTS_OK;
}

static bool boundary_allows(const nanotts_ko_syllable_t *b,uint8_t flags,bool space)
{
    uint8_t strength=boundary_strength(b->boundary_before);
    if (strength==NANOTTS_BOUNDARY_NONE) return true;
    return space && strength==NANOTTS_BOUNDARY_SPACE &&
           (flags & NANOTTS_OPT_LINK_EOJEOL)!=0u;
}
static bool vowel_i(uint8_t v) { return v==20u; }
static bool vowel_i_or_y(uint8_t v)
{ return v==20u || v==2u || v==3u || v==6u || v==7u || v==12u || v==17u; }
static uint8_t coda_to_onset(uint8_t c)
{
    switch(c) {
    case 1:return 0; case 2:return 1; case 4:return 2; case 7:return 3;
    case 8:return 5; case 16:return 6; case 17:return 7; case 19:return 9;
    case 20:return 10; case 22:return 12; case 23:return 14; case 24:return 15;
    case 25:return 16; case 26:return 17; case 27:return 18; default:return 0xffu;
    }
}
static uint8_t tense(uint8_t o)
{
    switch(o) { case 0:return 1; case 3:return 4; case 7:return 8;
    case 9:return 10; case 12:return 13; default:return o; }
}
static uint8_t aspirate(uint8_t o)
{
    switch(o) { case 0:return 15; case 3:return 16; case 7:return 17;
    case 12:return 14; default:return o; }
}

static void palatalize_and_link(nanotts_impl_t *impl,uint8_t flags)
{
    size_t i;
    for (i=0u;i+1u<impl->syllable_count;++i) {
        nanotts_ko_syllable_t *a=&impl->syllables[i], *b=&impl->syllables[i+1u];
        uint8_t moved;
        if (!boundary_allows(b,flags,true)) continue;
        /* Connected words before /i/ or a y-glide use productive n-insertion.
         * Keep the first coda; later nasal/lateral rules determine its surface. */
        if (boundary_strength(b->boundary_before)==NANOTTS_BOUNDARY_SPACE &&
            (flags&NANOTTS_OPT_LINK_EOJEOL)!=0u && a->coda!=0u &&
            b->onset==11u && vowel_i_or_y(b->vowel)) {
            b->onset=2u;
            continue;
        }
        if (vowel_i(b->vowel) && b->onset==11u) {
            if (a->coda==7u) { a->coda=0; b->onset=12; continue; }
            if (a->coda==25u) { a->coda=0; b->onset=14; continue; }
            if (a->coda==13u) { a->coda=8; b->onset=14; continue; }
        }
        if (vowel_i(b->vowel) && b->onset==18u &&
            (a->coda==7u || a->coda==25u)) {
            a->coda=0; b->onset=14; continue;
        }
        if (b->onset!=11u || !a->coda || a->coda==21u) continue;
        switch(a->coda) {
        case 3:a->coda=1;b->onset=9;continue;
        case 5:a->coda=4;b->onset=12;continue;
        case 6:a->coda=0;b->onset=2;continue;
        case 9:a->coda=8;b->onset=0;continue;
        case 10:a->coda=8;b->onset=6;continue;
        case 11:a->coda=8;b->onset=7;continue;
        case 12:a->coda=8;b->onset=10;continue;
        case 13:a->coda=8;b->onset=16;continue;
        case 14:a->coda=8;b->onset=17;continue;
        case 15:a->coda=0;b->onset=5;continue;
        case 18:a->coda=17;b->onset=9;continue;
        default:break;
        }
        if (a->coda==27u) { a->coda=0u; continue; }
        moved=coda_to_onset(a->coda);
        if (moved!=0xffu) { a->coda=0u; b->onset=moved; }
    }
}

static void resolve_clusters(nanotts_impl_t *impl,uint8_t flags)
{
    size_t i;
    for (i=0u;i<impl->syllable_count;++i) {
        nanotts_ko_syllable_t *a=&impl->syllables[i];
        nanotts_ko_syllable_t *b=i+1u<impl->syllable_count?&impl->syllables[i+1u]:NULL;
        bool linked=b && boundary_allows(b,flags,true);
        switch(a->coda) {
        case 3:a->coda=1;break;
        case 5:a->coda=4;if(linked)b->onset=tense(b->onset);break;
        case 6:a->coda=4;if(linked)b->onset=aspirate(b->onset);break;
        case 9:
            if(linked && b->onset==0u){a->coda=8;b->onset=1;}
            else a->coda=1;
            break;
        case 10:a->coda=16;if(linked)b->onset=tense(b->onset);break;
        case 11:a->coda=8;if(linked)b->onset=tense(b->onset);break;
        case 12:a->coda=8;if(linked)b->onset=tense(b->onset);break;
        case 13:a->coda=8;if(linked)b->onset=tense(b->onset);break;
        case 14:a->coda=26;break;
        case 15:
            a->coda=8;
            if(linked){uint8_t x=aspirate(b->onset);b->onset=x!=b->onset?x:tense(b->onset);} break;
        case 18:a->coda=17;break;
        default:break;
        }
    }
}

static void h_rules(nanotts_impl_t *impl,uint8_t flags)
{
    size_t i;
    for (i=0u;i+1u<impl->syllable_count;++i) {
        nanotts_ko_syllable_t *a=&impl->syllables[i], *b=&impl->syllables[i+1u];
        if (!boundary_allows(b,flags,true)) continue;
        if (a->coda==27u) {
            uint8_t x=aspirate(b->onset);
            if (x!=b->onset){a->coda=0;b->onset=x;}
            else if (b->onset==9u){a->coda=0;b->onset=10;}
            else if (b->onset==2u||b->onset==6u)a->coda=4;
        } else if (b->onset==18u) {
            switch(a->coda) {
            case 1:case 2:case 24:a->coda=0;b->onset=15;break;
            case 7:case 19:case 20:case 25:a->coda=0;b->onset=16;break;
            case 22:case 23:a->coda=0;b->onset=14;break;
            case 17:case 26:a->coda=0;b->onset=17;break;
            default:break;
            }
        }
    }
}

static uint8_t neutral(uint8_t c)
{
    switch(c) {
    case 0:return KO_CODA_NONE;
    case 1:case 2:case 3:case 9:case 24:return KO_CODA_K;
    case 4:case 5:case 6:return KO_CODA_N;
    case 7:case 19:case 20:case 22:case 23:case 25:case 27:return KO_CODA_T;
    case 8:case 11:case 12:case 13:case 15:return KO_CODA_L;
    case 10:case 16:return KO_CODA_M;
    case 14:case 17:case 18:case 26:return KO_CODA_P;
    case 21:return KO_CODA_NG;
    default:return KO_CODA_NONE;
    }
}

static void lateralize(nanotts_impl_t *impl,uint8_t flags)
{
    size_t i;
    for(i=0u;i+1u<impl->syllable_count;++i){
        nanotts_ko_syllable_t *a=&impl->syllables[i],*b=&impl->syllables[i+1u];
        if(!boundary_allows(b,flags,true))continue;
        if(a->coda==4u&&b->onset==5u){a->coda=8;b->onset=KO_L_LATERAL;}
        else if(a->coda==8u&&b->onset==2u)b->onset=KO_L_LATERAL;
        else if((a->coda==16u||a->coda==21u)&&b->onset==5u)b->onset=2u;
    }
}
static void surface_rules(nanotts_impl_t *impl,uint8_t flags)
{
    size_t i;
    for(i=0u;i<impl->syllable_count;++i)impl->syllables[i].coda=neutral(impl->syllables[i].coda);
    for(i=0u;i+1u<impl->syllable_count;++i){
        nanotts_ko_syllable_t *a=&impl->syllables[i],*b=&impl->syllables[i+1u];
        if(!boundary_allows(b,flags,true))continue;
        if(b->onset==5u){
            if(a->coda==KO_CODA_N||a->coda==KO_CODA_L){a->coda=KO_CODA_L;b->onset=KO_L_LATERAL;}
            else if(a->coda!=KO_CODA_NONE)b->onset=2u;
        }
        if(b->onset==2u||b->onset==6u){
            if(a->coda==KO_CODA_K)a->coda=KO_CODA_NG;
            else if(a->coda==KO_CODA_T)a->coda=KO_CODA_N;
            else if(a->coda==KO_CODA_P)a->coda=KO_CODA_M;
        }
        if(a->coda==KO_CODA_K||a->coda==KO_CODA_T||a->coda==KO_CODA_P)
            b->onset=tense(b->onset);
    }
}

static nanotts_phone_t onset_phone(uint8_t o)
{
    switch(o){
    case 0:return NANOTTS_PH_K_LAX;case 1:return NANOTTS_PH_K_TENSE;
    case 2:return NANOTTS_PH_N;case 3:return NANOTTS_PH_T_LAX;
    case 4:return NANOTTS_PH_T_TENSE;case 5:return NANOTTS_PH_R;
    case 6:return NANOTTS_PH_M;case 7:return NANOTTS_PH_P_LAX;
    case 8:return NANOTTS_PH_P_TENSE;case 9:return NANOTTS_PH_S_LAX;
    case 10:return NANOTTS_PH_S_TENSE;case 12:return NANOTTS_PH_C_LAX;
    case 13:return NANOTTS_PH_C_TENSE;case 14:return NANOTTS_PH_C_ASP;
    case 15:return NANOTTS_PH_K_ASP;case 16:return NANOTTS_PH_T_ASP;
    case 17:return NANOTTS_PH_P_ASP;case 18:return NANOTTS_PH_H;
    case KO_L_LATERAL:return NANOTTS_PH_L;default:return NANOTTS_PH_SILENCE;
    }
}
static nanotts_phone_t coda_phone(uint8_t c)
{
    switch(c){case KO_CODA_K:return NANOTTS_PH_K_CODA;case KO_CODA_N:return NANOTTS_PH_N;
    case KO_CODA_T:return NANOTTS_PH_T_CODA;case KO_CODA_L:return NANOTTS_PH_L;
    case KO_CODA_M:return NANOTTS_PH_M;case KO_CODA_P:return NANOTTS_PH_P_CODA;
    case KO_CODA_NG:return NANOTTS_PH_NG;default:return NANOTTS_PH_SILENCE;}
}

static int8_t ap_pitch(size_t pos,size_t count,bool high_initial)
{
    /* q4 semitones: compact LHLH/HHLH targets with a restrained range. */
    int first=high_initial?17:-13, high=15, low=-11;
    if(count<=1u)return(int8_t)(high_initial?13:-7);
    if(count==2u)return(int8_t)(pos?high:first);
    if(count==3u)return(int8_t)(pos==0u?first:(pos==1u?low:high));
    if(pos==0u)return(int8_t)first;
    if(pos==1u)return(int8_t)high;
    if(pos+2u>=count)return(int8_t)(pos+1u==count?high:low);
    return(int8_t)(high-(int)((high-low)*(int)(pos-1u)/(int)(count-2u)));
}

static uint8_t onset_duration(nanotts_phone_t phone,bool ap_initial)
{
    nanotts_phone_kind_t k=(nanotts_phone_kind_t)nanotts_phone_def(phone)->kind;
    switch(k){
    case NANOTTS_KIND_STOP_LAX: return ap_initial?100u:84u;
    case NANOTTS_KIND_STOP_TENSE: return ap_initial?100u:91u;
    case NANOTTS_KIND_STOP_ASP: return ap_initial?100u:89u;
    case NANOTTS_KIND_AFFRICATE_LAX: return ap_initial?100u:88u;
    case NANOTTS_KIND_AFFRICATE_TENSE: return ap_initial?100u:92u;
    case NANOTTS_KIND_AFFRICATE_ASP: return ap_initial?100u:91u;
    case NANOTTS_KIND_FRICATIVE_LAX:
    case NANOTTS_KIND_FRICATIVE_TENSE: return ap_initial?100u:93u;
    case NANOTTS_KIND_FRICATIVE_H: return ap_initial?96u:82u;
    case NANOTTS_KIND_NASAL: return ap_initial?96u:90u;
    case NANOTTS_KIND_LATERAL: return 92u;
    case NANOTTS_KIND_TAP: return 88u;
    default:return 100u;
    }
}

static bool push_vowels(nanotts_impl_t *impl,const nanotts_ko_syllable_t *s,
    uint8_t first_flags,uint8_t end_flags,int8_t pitch,uint8_t duration)
{
    nanotts_phone_t seq[3]; size_t n=0u,i;
    switch(s->vowel){
    case 0:seq[n++]=NANOTTS_PH_A;break; case 1:seq[n++]=NANOTTS_PH_AE;break;
    case 2:seq[n++]=NANOTTS_PH_Y;seq[n++]=NANOTTS_PH_A;break;
    case 3:seq[n++]=NANOTTS_PH_Y;seq[n++]=NANOTTS_PH_AE;break;
    case 4:seq[n++]=NANOTTS_PH_EO;break; case 5:seq[n++]=NANOTTS_PH_E;break;
    case 6:seq[n++]=NANOTTS_PH_Y;seq[n++]=NANOTTS_PH_EO;break;
    case 7:seq[n++]=NANOTTS_PH_Y;seq[n++]=NANOTTS_PH_E;break;
    case 8:seq[n++]=NANOTTS_PH_O;break;
    case 9:seq[n++]=NANOTTS_PH_W;seq[n++]=NANOTTS_PH_A;break;
    case 10:seq[n++]=NANOTTS_PH_W;seq[n++]=NANOTTS_PH_AE;break;
    case 11:seq[n++]=NANOTTS_PH_W;seq[n++]=NANOTTS_PH_E;break;
    case 12:seq[n++]=NANOTTS_PH_Y;seq[n++]=NANOTTS_PH_O;break;
    case 13:seq[n++]=NANOTTS_PH_U;break;
    case 14:seq[n++]=NANOTTS_PH_W;seq[n++]=NANOTTS_PH_EO;break;
    case 15:seq[n++]=NANOTTS_PH_W;seq[n++]=NANOTTS_PH_E;break;
    case 16:seq[n++]=NANOTTS_PH_W;seq[n++]=NANOTTS_PH_I;break;
    case 17:seq[n++]=NANOTTS_PH_Y;seq[n++]=NANOTTS_PH_U;break;
    case 18:seq[n++]=NANOTTS_PH_EU;break;
    case 19:
        if(s->onset==11u)seq[n++]=NANOTTS_PH_UI;
        else seq[n++]=NANOTTS_PH_I;
        break;
    case 20:seq[n++]=NANOTTS_PH_I;break; default:return false;
    }
    for(i=0u;i<n;++i){
        uint8_t f=i?0u:first_flags;
        uint8_t d=nanotts_phone_is_vowel(seq[i])?duration:72u;
        if(i+1u==n)f|=end_flags;
        if(!nanotts_push_event(impl,seq[i],f,d,pitch))return false;
    }
    return true;
}

static bool push_pause(nanotts_impl_t *impl,uint8_t boundary,bool question,uint8_t flags)
{
    uint8_t strength=boundary_strength(boundary),ef=0u,dur=100u;
    nanotts_phone_t phone;
    question=question||((boundary&KO_BOUNDARY_QUESTION)!=0u);
    if(strength==NANOTTS_BOUNDARY_NONE)return true;
    if(strength==NANOTTS_BOUNDARY_SPACE){
        if(flags&NANOTTS_OPT_NO_AUTOPAUSE)return true;
        phone=NANOTTS_PH_AP_PAUSE;dur=50u;
    }else if(strength==NANOTTS_BOUNDARY_COMMA){
        phone=NANOTTS_PH_PHRASE_PAUSE;dur=68u;
        ef=NANOTTS_EVENT_PHRASE_END|NANOTTS_EVENT_CONTINUATION;
    }else{
        phone=NANOTTS_PH_PHRASE_PAUSE;ef=NANOTTS_EVENT_PHRASE_END;
        if(question)ef|=NANOTTS_EVENT_QUESTION;
    }
    return nanotts_push_event(impl,phone,ef,dur,0);
}

static size_t choose_ap_end(const nanotts_impl_t *impl,size_t start)
{
    const size_t max_syllables=7u,max_words=3u; size_t end=start,words=0u;
    while(end<impl->syllable_count){
        size_t word_end=end+1u;
        while(word_end<impl->syllable_count &&
              boundary_strength(impl->syllables[word_end].boundary_before)==NANOTTS_BOUNDARY_NONE)
            ++word_end;
        if(words && (words>=max_words || word_end-start>max_syllables))break;
        end=word_end;++words;
        if(end>=impl->syllable_count)break;
        if(boundary_strength(impl->syllables[end].boundary_before)>=NANOTTS_BOUNDARY_COMMA)break;
    }
    return end;
}

static nanotts_result_t emit_events(nanotts_impl_t *impl,uint8_t trailing,
    bool question,uint8_t flags)
{
    size_t start=0u; impl->event_count=0u;
    while(start<impl->syllable_count){
        size_t end=choose_ap_end(impl,start),i;
        nanotts_phone_t first=onset_phone(impl->syllables[start].onset);
        bool high=first!=NANOTTS_PH_SILENCE&&nanotts_phone_is_high_onset(first);
        for(i=start;i<end;++i){
            nanotts_ko_syllable_t *s=&impl->syllables[i];
            bool last=i+1u==end;
            bool word_end=i+1u==impl->syllable_count ||
                boundary_strength(impl->syllables[i+1u].boundary_before)!=NANOTTS_BOUNDARY_NONE;
            uint8_t first_flags=NANOTTS_EVENT_SYLLABLE_START,end_flags=0u;
            int8_t pitch=ap_pitch(i-start,end-start,high);
            nanotts_phone_t on=onset_phone(s->onset),co=coda_phone(s->coda);
            bool closed=co!=NANOTTS_PH_SILENCE;
            bool phrase_edge=last&&(end==impl->syllable_count ||
                boundary_strength(impl->syllables[end].boundary_before)>=NANOTTS_BOUNDARY_COMMA);
            uint8_t vdur=closed?91u:100u;
            if(i==start)first_flags|=NANOTTS_EVENT_AP_START;
            if(word_end){end_flags|=NANOTTS_EVENT_WORD_END;if(vdur<103u)vdur=(uint8_t)(vdur+3u);}
            if(last){
                end_flags|=NANOTTS_EVENT_AP_END;
                vdur=(uint8_t)(vdur+(closed?8u:10u));
                if(phrase_edge)vdur=(uint8_t)(vdur+(closed?8u:10u));
            }
            if(on!=NANOTTS_PH_SILENCE){
                uint8_t odur=onset_duration(on,i==start);
                if(!nanotts_push_event(impl,on,first_flags,odur,pitch))return NANOTTS_ERR_TOO_MANY_EVENTS;
                first_flags=0u;
            }
            if(!push_vowels(impl,s,first_flags,co==NANOTTS_PH_SILENCE?end_flags:0u,pitch,vdur))
                return NANOTTS_ERR_TOO_MANY_EVENTS;
            if(co!=NANOTTS_PH_SILENCE){
                bool sonorant=co==NANOTTS_PH_N||co==NANOTTS_PH_M||
                              co==NANOTTS_PH_NG||co==NANOTTS_PH_L;
                uint8_t cd=sonorant?78u:82u;
                if(phrase_edge)cd=(uint8_t)(cd+(sonorant?9u:5u));
                if(!nanotts_push_event(impl,co,end_flags,cd,pitch))return NANOTTS_ERR_TOO_MANY_EVENTS;
            }
        }
        if(end<impl->syllable_count && !push_pause(impl,impl->syllables[end].boundary_before,false,flags))
            return NANOTTS_ERR_TOO_MANY_EVENTS;
        start=end;
    }
    if(!push_pause(impl,trailing,question,flags))return NANOTTS_ERR_TOO_MANY_EVENTS;
    return impl->event_count?NANOTTS_OK:NANOTTS_ERR_EMPTY_INPUT;
}

nanotts_result_t nanotts_ko_parse_text_impl(nanotts_impl_t *impl,const char *text,
    size_t length,uint8_t flags,nanotts_parse_info_t *info)
{
    size_t off=0u; uint8_t pending=NANOTTS_BOUNDARY_PHRASE,trailing=NANOTTS_BOUNDARY_NONE;
    bool trailing_question=false; nanotts_result_t r;
    while(off<length){
        size_t start=off,end,after; uint32_t cp;
        if(!utf8_decode(text,length,&off,&cp)){if(info)info->error_byte=start;return NANOTTS_ERR_UTF8;}
        if(is_hangul(cp)){
            end=off;
            while(end<length){size_t probe=end;uint32_t next;
                if(!utf8_decode(text,length,&probe,&next)){if(info)info->error_byte=end;return NANOTTS_ERR_UTF8;}
                if(!is_hangul(next)) break;
                end=probe;
            }
            r=append_word(impl,text+start,end-start,pending,flags,info);
            if(r!=NANOTTS_OK)return r;
            pending=NANOTTS_BOUNDARY_NONE;trailing=NANOTTS_BOUNDARY_NONE;trailing_question=false;off=end;continue;
        }
        if(is_digit(cp)){
            end=off;while(end<length&&text[end]>='0'&&text[end]<='9')++end;
            r=append_number(impl,text+start,end-start,pending,info);if(r!=NANOTTS_OK)return r;
            pending=NANOTTS_BOUNDARY_NONE;trailing=NANOTTS_BOUNDARY_NONE;trailing_question=false;off=end;continue;
        }
        if(is_letter(cp)){
            end=off;while(end<length&&((text[end]>='A'&&text[end]<='Z')||(text[end]>='a'&&text[end]<='z')))++end;
            r=append_latin(impl,text+start,end-start,pending,info);if(r!=NANOTTS_OK)return r;
            pending=NANOTTS_BOUNDARY_NONE;trailing=NANOTTS_BOUNDARY_NONE;trailing_question=false;off=end;continue;
        }
        after=off;
        if(is_space(cp)){
            if(boundary_strength(pending)<NANOTTS_BOUNDARY_SPACE)pending=NANOTTS_BOUNDARY_SPACE;
            if(impl->syllable_count)trailing=NANOTTS_BOUNDARY_SPACE;
        }else if(is_comma(cp)||cp=='-'||cp==0x2014u){
            pending=NANOTTS_BOUNDARY_COMMA;trailing=NANOTTS_BOUNDARY_COMMA;trailing_question=false;
        }else if(is_question(cp)){
            pending=(uint8_t)(NANOTTS_BOUNDARY_PHRASE|KO_BOUNDARY_QUESTION);
            trailing=NANOTTS_BOUNDARY_PHRASE;trailing_question=true;
        }else if(is_sentence_end(cp)){
            pending=NANOTTS_BOUNDARY_PHRASE;trailing=NANOTTS_BOUNDARY_PHRASE;trailing_question=false;
        }else if(cp=='\''||cp==0x2019u||cp=='"'||cp==0x201cu||cp==0x201du||
                 cp=='('||cp==')'||cp=='['||cp==']'){
        }else{
            if(info){info->error_byte=start;info->error_codepoint=cp;}
            return NANOTTS_ERR_UNSUPPORTED_TEXT;
        }
        off=after;
    }
    if(!impl->syllable_count)return NANOTTS_ERR_EMPTY_INPUT;
    palatalize_and_link(impl,flags);
    resolve_clusters(impl,flags);
    h_rules(impl,flags);
    lateralize(impl,flags);
    surface_rules(impl,flags);
    return emit_events(impl,trailing,trailing_question,flags);
}
