/* SPDX-License-Identifier: MIT */
#include "nanotts/nanotts.h"
#if NANOTTS_CLI_HAVE_PCM
#include "nanotts/nanotts_pcm.h"
#endif
#if NANOTTS_CLI_HAVE_PWM
#include "nanotts/nanotts_pwm.h"
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef NANOTTS_CLI_HAVE_PCM
#define NANOTTS_CLI_HAVE_PCM 0
#endif
#ifndef NANOTTS_CLI_HAVE_PWM
#define NANOTTS_CLI_HAVE_PWM 0
#endif

typedef enum output_format { FORMAT_WAV, FORMAT_PCM, FORMAT_PWM } output_format_t;
#if NANOTTS_CLI_HAVE_PCM || NANOTTS_CLI_HAVE_PWM
typedef struct file_sink { FILE *file; uint64_t bytes; } file_sink_t;
#endif
typedef struct wav_sink { FILE *file; uint64_t samples; } wav_sink_t;

static void usage(FILE *f)
{
    fprintf(f,
      "NanoTTS-KO %s - compact Korean text-to-speech\n"
      "Usage: nanotts --text TEXT [options]\n"
      "       nanotts --text-file FILE [options]\n\n"
      "Options:\n"
      "  -t, --text TEXT          Korean text (Hangul, digits, ASCII letters)\n"
      "  --text-file FILE         Read text from FILE; '-' reads stdin\n"
      "  -o, --output FILE        Output path (default: out.wav)\n"
      "  --format wav|pcm|pwm     Output format (default: wav)\n"
      "  --sample-rate HZ         8000..24000 (default: 16000)\n"
      "  --rate PERCENT           Speaking rate, 38..250 (default: 100)\n"
      "  --pitch CENTS            Global pitch, -1200..1200\n"
      "  --volume 0..255          Output volume (default: 220)\n"
      "  --final auto|fall|rise|continue|level\n"
      "  --pwm-top N              Timer TOP/ARR for PWM output (default: 1023)\n"
      "  --pwm-inverted           Invert PWM compare values\n"
      "  --no-link                Disable sound changes across spaces\n"
      "  --no-autopause           Suppress automatic accent-phrase pauses\n"
      "  --no-lexicon             Disable built-in exception lexicon\n"
      "  --dump-phones            Print generated phone events\n"
      "  --version                Print version\n"
      "  -h, --help               Show this help\n",
      nanotts_version_string());
}

static int parse_long(const char *s,long lo,long hi,long *out)
{
    char *end=NULL; long v;
    errno=0;v=strtol(s,&end,10);
    if(errno||end==s||*end!='\0'||v<lo||v>hi)return 0;
    *out=v;return 1;
}

static char *read_all(FILE *f,size_t *length)
{
    size_t cap=4096u,n=0u;char *buf=(char*)malloc(cap+1u);
    if(!buf)return NULL;
    for(;;){size_t got;
        if(n==cap){size_t next=cap*2u;char *p;
            if(next<cap){free(buf);return NULL;}p=(char*)realloc(buf,next+1u);
            if(!p){free(buf);return NULL;}buf=p;cap=next;}
        got=fread(buf+n,1u,cap-n,f);n+=got;
        if(got==0u){if(ferror(f)){free(buf);return NULL;}break;}
    }
    buf[n]='\0';*length=n;return buf;
}

#if NANOTTS_CLI_HAVE_PCM
static int bytes_write(void *user,const uint8_t *bytes,size_t count)
{
    file_sink_t *s=(file_sink_t*)user;
    if(fwrite(bytes,1u,count,s->file)!=count)return 1;
    s->bytes+=(uint64_t)count;return 0;
}
#endif
#if NANOTTS_CLI_HAVE_PWM
static int pwm_write(void *user,const uint16_t *values,size_t count)
{
    file_sink_t *s=(file_sink_t*)user;size_t i;uint8_t b[2];
    for(i=0u;i<count;++i){b[0]=(uint8_t)(values[i]&0xffu);b[1]=(uint8_t)(values[i]>>8);
        if(fwrite(b,1u,2u,s->file)!=2u) return 1;
        s->bytes+=2u;
    }
    return 0;
}
#endif
static int wav_write(void *user,const int16_t *samples,size_t count)
{
    wav_sink_t *s=(wav_sink_t*)user;size_t i;uint8_t b[2];
    for(i=0u;i<count;++i){uint16_t v=(uint16_t)samples[i];b[0]=(uint8_t)(v&0xffu);b[1]=(uint8_t)(v>>8);
        if(fwrite(b,1u,2u,s->file)!=2u)return 1;}
    s->samples+=(uint64_t)count;return 0;
}
static void put16(FILE *f,uint16_t v)
{ fputc((int)(v&0xffu),f);fputc((int)(v>>8),f); }
static void put32(FILE *f,uint32_t v)
{ put16(f,(uint16_t)(v&0xffffu));put16(f,(uint16_t)(v>>16)); }
static int wav_header(FILE *f,uint32_t rate,uint32_t data_bytes)
{
    if(fwrite("RIFF",1u,4u,f)!=4u) return 0;
    put32(f,36u+data_bytes);
    if(fwrite("WAVEfmt ",1u,8u,f)!=8u) return 0;
    put32(f,16u);put16(f,1u);put16(f,1u);
    put32(f,rate);put32(f,rate*2u);put16(f,2u);put16(f,16u);
    if(fwrite("data",1u,4u,f)!=4u) return 0;
    put32(f,data_bytes);
    return !ferror(f);
}

static void dump_events(const nanotts_t *tts)
{
    const nanotts_event_t *e=nanotts_events(tts);size_t i,n=nanotts_event_count(tts);
    for(i=0u;i<n;++i){
        printf("%4lu  %-12s dur=%3u%% pitch=%+.2f st flags=0x%02x\n",
          (unsigned long)i,nanotts_phone_name((nanotts_phone_t)e[i].phone),
          (unsigned)e[i].duration_percent,(double)e[i].pitch_semitones_q4/16.0,
          (unsigned)e[i].flags);
    }
}

int main(int argc,char **argv)
{
    const char *text_arg=NULL,*text_file=NULL,*out_path="out.wav";
    char *owned_text=NULL;size_t text_len=NANOTTS_NPOS;output_format_t format=FORMAT_WAV;
    uint32_t sample_rate=16000u;uint16_t pwm_top=1023u;int pwm_inverted=0,dump=0;
    nanotts_options_t options;nanotts_parse_info_t info;nanotts_t tts;
    int i;FILE *out=NULL;nanotts_result_t r;
    nanotts_default_options(&options);
    for(i=1;i<argc;++i){const char *a=argv[i];
        if(!strcmp(a,"-h")||!strcmp(a,"--help")){usage(stdout);return 0;}
        if(!strcmp(a,"--version")){puts(nanotts_version_string());return 0;}
        if((!strcmp(a,"-t")||!strcmp(a,"--text"))&&i+1<argc){text_arg=argv[++i];continue;}
        if(!strcmp(a,"--text-file")&&i+1<argc){text_file=argv[++i];continue;}
        if((!strcmp(a,"-o")||!strcmp(a,"--output"))&&i+1<argc){out_path=argv[++i];continue;}
        if(!strcmp(a,"--format")&&i+1<argc){const char *v=argv[++i];
            if(!strcmp(v,"wav"))format=FORMAT_WAV;else if(!strcmp(v,"pcm"))format=FORMAT_PCM;
            else if(!strcmp(v,"pwm"))format=FORMAT_PWM;
            else{fprintf(stderr,"unknown format: %s\n",v);return 2;}
            continue;}
        if(!strcmp(a,"--sample-rate")&&i+1<argc){long v;if(!parse_long(argv[++i],8000,24000,&v)){fprintf(stderr,"invalid sample rate\n");return 2;}sample_rate=(uint32_t)v;continue;}
        if(!strcmp(a,"--rate")&&i+1<argc){long v;if(!parse_long(argv[++i],38,250,&v)){fprintf(stderr,"invalid rate\n");return 2;}options.rate_q8=(uint16_t)((v*256+50)/100);continue;}
        if(!strcmp(a,"--pitch")&&i+1<argc){long v;if(!parse_long(argv[++i],-1200,1200,&v)){fprintf(stderr,"invalid pitch\n");return 2;}options.pitch_cents=(int16_t)v;continue;}
        if(!strcmp(a,"--volume")&&i+1<argc){long v;if(!parse_long(argv[++i],0,255,&v)){fprintf(stderr,"invalid volume\n");return 2;}options.volume=(uint8_t)v;continue;}
        if(!strcmp(a,"--pwm-top")&&i+1<argc){long v;if(!parse_long(argv[++i],1,65535,&v)){fprintf(stderr,"invalid PWM TOP\n");return 2;}pwm_top=(uint16_t)v;continue;}
        if(!strcmp(a,"--pwm-inverted")){pwm_inverted=1;continue;}
        if(!strcmp(a,"--dump-phones")){dump=1;continue;}
        if(!strcmp(a,"--no-link")){options.flags=(uint8_t)(options.flags&~NANOTTS_OPT_LINK_EOJEOL);continue;}
        if(!strcmp(a,"--no-autopause")){options.flags|=NANOTTS_OPT_NO_AUTOPAUSE;continue;}
        if(!strcmp(a,"--no-lexicon")){options.flags|=NANOTTS_OPT_NO_BUILTIN_LEXICON;continue;}
        if(!strcmp(a,"--final")&&i+1<argc){const char *v=argv[++i];
            if(!strcmp(v,"auto"))options.final_tone=NANOTTS_FINAL_AUTO;
            else if(!strcmp(v,"fall"))options.final_tone=NANOTTS_FINAL_FALL;
            else if(!strcmp(v,"rise"))options.final_tone=NANOTTS_FINAL_RISE;
            else if(!strcmp(v,"continue"))options.final_tone=NANOTTS_FINAL_CONTINUE;
            else if(!strcmp(v,"level"))options.final_tone=NANOTTS_FINAL_LEVEL;
            else{fprintf(stderr,"invalid final tone\n");return 2;}continue;}
        fprintf(stderr,"unknown or incomplete option: %s\n",a);usage(stderr);return 2;
    }
#if !NANOTTS_CLI_HAVE_PWM
    (void)pwm_top;
    (void)pwm_inverted;
#endif
    if((text_arg!=NULL)+(text_file!=NULL)!=1){fprintf(stderr,"provide exactly one of --text or --text-file\n");return 2;}
    if(text_file){FILE *in=!strcmp(text_file,"-")?stdin:fopen(text_file,"rb");
        if(!in){perror(text_file);return 1;}owned_text=read_all(in,&text_len);if(in!=stdin)fclose(in);
        if(!owned_text){fprintf(stderr,"unable to read input\n");return 1;}text_arg=owned_text;}
    r=nanotts_init(&tts,sample_rate);if(r!=NANOTTS_OK){fprintf(stderr,"init: %s\n",nanotts_strerror(r));free(owned_text);return 1;}
    r=nanotts_parse_text(&tts,text_arg,text_len,options.flags,&info);
    if(r!=NANOTTS_OK){fprintf(stderr,"parse: %s",nanotts_strerror(r));
        if(info.error_byte!=NANOTTS_NPOS)
            fprintf(stderr," at byte %lu",(unsigned long)info.error_byte);
        fputc('\n',stderr);free(owned_text);return 1;}
    if(dump)dump_events(&tts);
    if(!strcmp(out_path,"-")&&format==FORMAT_WAV){fprintf(stderr,"WAV output requires a seekable file\n");free(owned_text);return 2;}
    out=!strcmp(out_path,"-")?stdout:fopen(out_path,"wb");if(!out){perror(out_path);free(owned_text);return 1;}
    if(format==FORMAT_WAV){wav_sink_t sink={out,0u};
        if(!wav_header(out,sample_rate,0u)){r=NANOTTS_ERR_CALLBACK_ABORTED;}
        else r=nanotts_render_events(&tts,&options,wav_write,&sink);
        if(r==NANOTTS_OK){uint64_t bytes=sink.samples*2u;if(bytes>0xffffffffu)r=NANOTTS_ERR_OUTPUT_TOO_SMALL;
            else if(fseek(out,0L,SEEK_SET)!=0||!wav_header(out,sample_rate,(uint32_t)bytes))r=NANOTTS_ERR_CALLBACK_ABORTED;}
    }else if(format==FORMAT_PCM){
#if NANOTTS_CLI_HAVE_PCM
        uint8_t scratch[256];file_sink_t sink={out,0u};nanotts_pcm16le_output_t pcm;
        r=nanotts_pcm16le_output_init(&pcm,scratch,sizeof(scratch),bytes_write,&sink);
        if(r==NANOTTS_OK)r=nanotts_render_events(&tts,&options,nanotts_pcm16le_write_pcm,&pcm);
#else
        r=NANOTTS_ERR_FEATURE_DISABLED;
#endif
    }else{
#if NANOTTS_CLI_HAVE_PWM
        uint16_t scratch[128];file_sink_t sink={out,0u};nanotts_pwm_output_t pwm;
        r=nanotts_pwm_output_init(&pwm,pwm_top,pwm_inverted,scratch,128u,pwm_write,&sink);
        if(r==NANOTTS_OK)r=nanotts_render_events(&tts,&options,nanotts_pwm_write_pcm,&pwm);
#else
        r=NANOTTS_ERR_FEATURE_DISABLED;
#endif
    }
    if(out!=stdout&&fclose(out)!=0&&r==NANOTTS_OK)r=NANOTTS_ERR_CALLBACK_ABORTED;
    if(r!=NANOTTS_OK){fprintf(stderr,"render: %s\n",nanotts_strerror(r));if(out_path&&strcmp(out_path,"-"))remove(out_path);free(owned_text);return 1;}
    free(owned_text);return 0;
}
