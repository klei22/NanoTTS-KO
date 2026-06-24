/* SPDX-License-Identifier: MIT */
#include "nanotts_internal.h"

typedef struct ko_exception {
    const char *written;
    const char *prescriptive;
    const char *modern;
    uint16_t morph_flags;
} ko_exception_t;

#if NANOTTS_ENABLE_BUILTIN_LEXICON
/*
 * Independently authored compact pronunciation list.  It contains words whose
 * standard or common surface form cannot be recovered reliably from spelling
 * alone.  The table deliberately stores Hangul pronunciations rather than
 * acoustic data so it remains renderer-independent.
 */
static const ko_exception_t KO_EXCEPTIONS[] = {
    {"같이","가치",NULL,0},{"굳이","구지",NULL,0},
    {"맏이","마지",NULL,0},{"밭이","바치",NULL,0},
    {"해돋이","해도지",NULL,0},{"붙이다","부치다",NULL,0},
    {"닫히다","다치다",NULL,0},{"읽다","익따",NULL,0},
    {"읽고","일꼬",NULL,0},{"밟다","밥따",NULL,0},
    {"밟고","밥꼬",NULL,0},{"넓다","널따",NULL,0},
    {"핥다","할따",NULL,0},
    {"맛있다","마딛따","마싣따",0},
    {"멋있다","머딛따","머싣따",0},
    {"꽃잎","꼰닙",NULL,0},{"깻잎","깬닙",NULL,0},
    {"색연필","생년필",NULL,0},{"담요","담뇨",NULL,0},
    {"솜이불","솜니불",NULL,0},{"한여름","한녀름",NULL,0},
    {"막일","망닐",NULL,0},{"맨입","맨닙",NULL,0},
    {"식용유","시굥뉴",NULL,0},{"홑이불","혼니불",NULL,0},
    {"삯일","상닐",NULL,0},{"내복약","내봉냑",NULL,0},
    {"남존여비","남존녀비",NULL,0},{"신여성","신녀성",NULL,0},
    {"직행열차","지캥녈차",NULL,0},{"늑막염","능망념",NULL,0},
    {"콩엿","콩녇",NULL,0},{"눈요기","눈뇨기",NULL,0},
    {"영업용","영엄뇽",NULL,0},{"백분율","백뿐뉼",NULL,0},
    {"밤윷","밤뉻",NULL,0},{"들일","들릴",NULL,0},
    {"솔잎","솔립",NULL,0},{"설익다","설릭따",NULL,0},
    {"물약","물략",NULL,0},{"불여우","불려우",NULL,0},
    {"서울역","서울력",NULL,0},{"물엿","물렫",NULL,0},
    {"휘발유","휘발류",NULL,0},{"유들유들","유들류들",NULL,0},

    /* Lexical/Hanja-derived tensification. */
    {"갈등","갈뜽",NULL,0},{"발동","발똥",NULL,0},
    {"절도","절또",NULL,0},{"말살","말쌀",NULL,0},
    {"불소","불쏘",NULL,0},{"일시","일씨",NULL,0},
    {"갈증","갈쯩",NULL,0},{"물질","물찔",NULL,0},
    {"발전","발쩐",NULL,0},{"몰상식","몰쌍식",NULL,0},
    {"불세출","불쎄출",NULL,0},

    /* Complex-coda lexical survivors. */
    {"넓게","널께",NULL,0},{"짧게","짤께",NULL,0},
    {"넓죽하다","넙쭈카다",NULL,0},
    {"훑소","훌쏘",NULL,0},{"떫지","떨찌",NULL,0},

    /* Frequent compounds and standard lexical forms. */
    {"공권력","공꿘녁",NULL,0},{"입원료","이붠뇨",NULL,0},
    {"생산량","생산냥",NULL,0},{"결단력","결딴녁",NULL,0},
    {"상견례","상견녜",NULL,0},{"횡단로","횡단노",NULL,0},
    {"대관령","대괄령",NULL,0},{"의견란","의견난",NULL,0},
    {"임진란","임진난",NULL,0},{"동원령","동원녕",NULL,0},
    {"검열","검녈",NULL,0},{"금융","금늉",NULL,0},
    {"협력","혐녁",NULL,0},{"법률","범뉼",NULL,0},
    {"독립","동닙",NULL,0},{"심리","심니",NULL,0},
    {"합리","함니",NULL,0},{"십리","심니",NULL,0},
    {"몇리","면니",NULL,0},{"못이기다","몬니기다",NULL,0},

    /* Lexical compound fortition (Standard Pronunciation Rule 28). */
    {"문고리","문꼬리",NULL,0},{"눈동자","눈똥자",NULL,0},
    {"신바람","신빠람",NULL,0},{"산새","산쌔",NULL,0},
    {"손재주","손째주",NULL,0},{"길가","길까",NULL,0},
    {"물동이","물똥이",NULL,0},{"발바닥","발빠닥",NULL,0},
    {"굴속","굴쏙",NULL,0},{"술잔","술짠",NULL,0},
    {"바람결","바람껼",NULL,0},{"그믐달","그믐딸",NULL,0},
    {"아침밥","아침빱",NULL,0},{"잠자리","잠짜리",NULL,0},
    {"강가","강까",NULL,0},{"초승달","초승딸",NULL,0},
    {"등불","등뿔",NULL,0},{"창살","창쌀",NULL,0},
    {"강줄기","강쭐기",NULL,0},

    /* Common modern contractions. */
    {"되어","되어","돼어",0},{"보이어","보이어","뵈어",0}
};
#endif

const char *ko_builtin_pronunciation(const char *word, size_t length,
    nanotts_ko_style_t style, size_t *out_length, uint16_t *morph_flags,
    bool *variant_selected)
{
#if NANOTTS_ENABLE_BUILTIN_LEXICON
    size_t i;
    for (i = 0u; i < sizeof(KO_EXCEPTIONS) / sizeof(KO_EXCEPTIONS[0]); ++i) {
        size_t n = strlen(KO_EXCEPTIONS[i].written);
        const char *spoken;
        if (n != length || memcmp(word, KO_EXCEPTIONS[i].written, length) != 0)
            continue;
        spoken = KO_EXCEPTIONS[i].prescriptive;
        if ((style == NANOTTS_KO_STYLE_MODERN_SEOUL ||
             style == NANOTTS_KO_STYLE_CONVERSATIONAL) &&
            KO_EXCEPTIONS[i].modern)
            spoken = KO_EXCEPTIONS[i].modern;
        if (out_length) *out_length = strlen(spoken);
        if (morph_flags) *morph_flags = KO_EXCEPTIONS[i].morph_flags;
        if (variant_selected)
            *variant_selected = KO_EXCEPTIONS[i].modern &&
                spoken == KO_EXCEPTIONS[i].modern;
        return spoken;
    }
#else
    (void)word; (void)length; (void)style;
#endif
    if (out_length) *out_length = 0u;
    if (morph_flags) *morph_flags = 0u;
    if (variant_selected) *variant_selected = false;
    return NULL;
}

/* Modern Seoul speech does not preserve lexical length consistently.  The
 * table is intentionally conservative and currently provides only an API hook
 * for application dictionaries; no ambiguous spelling is lengthened here. */
bool ko_lexical_long_vowel(const char *word, size_t length, size_t *syllable_index)
{
    (void)word;
    (void)length;
    if (syllable_index) *syllable_index = 0u;
    return false;
}

static const char *const KO_VERB_STEMS[] = {
    "가","오","하","되","있","없","먹","마시","보","듣","걷",
    "읽","쓰","말하","살","알","모르","만들","주","받","찾","놓",
    "껴안","만나","얹",
    "좋","많","싫","맞","앉","신","안","삼","더듬","닮","젊",
    "밟","넓","핥","훑","떫","입","잡","닫","열","웃","울",
    "자","깨","일어나","서","앉아있","기다리","배우","가르치","공부하",
    "일하","운전하","사용하","준비하","시작하","끝나","끝내","들어가",
    "나오","돌아가","돌아오","만나","헤어지","사랑하","생각하","기억하",
    "잊","알리","남기","옮기","감기","굶기","안기","보내","꺼내",
    "넣","빼","켜","끄","누르","당기","밀","열리","닫히","붙이",
    "떨어지","올리","내리","바꾸","고르","선택하","확인하","취소하",
    "연결하","충전하","설정하","저장하","삭제하","실행하","멈추","도착하",
    "출발하","필요하","가능하","안전하","위험하","부족하","고장나","작동하",
    "신","신고하","신청하","말","묻","대답하","설명하","도와주","기다리"
};

bool ko_is_known_verb_stem(const char *word, size_t length)
{
    size_t i;
    for (i = 0u; i < sizeof(KO_VERB_STEMS) / sizeof(KO_VERB_STEMS[0]); ++i) {
        size_t n = strlen(KO_VERB_STEMS[i]);
        if (n == length && memcmp(word, KO_VERB_STEMS[i], length) == 0)
            return true;
    }
    return false;
}

static bool verb_word_matches_syllables(const char *word,
    const nanotts_ko_syllable_t *syllables, size_t count,
    bool clear_final_coda)
{
    size_t off = 0u, index = 0u, length = strlen(word);
    while (off < length && index < count) {
        uint32_t cp, x;
        uint8_t l, v, t, input_t;
        if (!ko_utf8_decode(word, length, &off, &cp) ||
            cp < KO_S_BASE || cp >= KO_S_BASE + KO_S_COUNT)
            return false;
        x = cp - KO_S_BASE;
        l = (uint8_t)(x / KO_N_COUNT);
        v = (uint8_t)((x % KO_N_COUNT) / KO_T_COUNT);
        t = (uint8_t)(x % KO_T_COUNT);
        input_t = syllables[index].lexical_coda;
        if (clear_final_coda && index + 1u == count) input_t = 0u;
        if (syllables[index].lexical_onset != l ||
            syllables[index].vowel != v || input_t != t)
            return false;
        ++index;
    }
    return off == length && index == count;
}

bool ko_is_known_verb_syllables(const nanotts_ko_syllable_t *syllables,
                                size_t count, bool clear_final_coda)
{
    size_t i;
    if (!syllables || !count) return false;
    for (i = 0u; i < sizeof(KO_VERB_STEMS) / sizeof(KO_VERB_STEMS[0]); ++i)
        if (verb_word_matches_syllables(KO_VERB_STEMS[i], syllables, count,
                                       clear_final_coda))
            return true;
    return false;
}

static const char *const KO_QUESTION_WORDS[] = {
    "뭐","무엇","누구","어디","언제","왜","어떻게","어느","어떤",
    "몇","얼마","얼마나","무슨","어째서"
};

bool ko_is_question_word(const char *word, size_t length)
{
    size_t i;
    for (i = 0u; i < sizeof(KO_QUESTION_WORDS) / sizeof(KO_QUESTION_WORDS[0]); ++i) {
        size_t n = strlen(KO_QUESTION_WORDS[i]);
        if (n == length && memcmp(word, KO_QUESTION_WORDS[i], length) == 0)
            return true;
    }
    return false;
}
