#include "zion_sdk.h"
#include <stdbool.h>
#include "fixture.h"
#include "golden.h"
const uint32_t zion_plugin_abi=ZION_ABI_VERSION;
const uint32_t zion_plugin_api_size=sizeof(zion_api_t);
static nv_rec_t record;
static nv_player_t player;
static int16_t block[320];
static const zion_api_t *host;
static unsigned mode;
static char *decimal(char *out,uint32_t value) {
    char digits[10];unsigned n=0;do {digits[n++]=(char)('0'+value%10);value/=10;} while(value);
    while(n) *out++=digits[--n];*out=0;return out;
}
static char *append(char *out,const char *s) {while(*s) *out++=*s++;*out=0;return out;}
static void report(const char *stage,unsigned value,unsigned count,unsigned hash) {
    char line[128];char *p=append(line,stage);p=append(p," mode=");p=decimal(p,value);
    p=append(p," samples=");p=decimal(p,count);p=append(p," hash=");(void)decimal(p,hash);host->log(line);
}
static void show(void) {
    char text[80];char *p=append(text,"DSP ");p=decimal(p,mode);append(p,"/11\nPCM 16000 Hz\n短按OK开始");
    host->text_set(ZION_TEXT_BODY,text);
}
static bool golden(void) {
    seed_record(&record,block);
    for(unsigned voice=0;voice<NV_COUNT;voice++) {
        unsigned count=0,hash=2166136261u;nv_play_start(&player,&record,(int)voice);
        while(!player.done) {
            int n=nv_play_fill(&player,block,320);if(n<=0) break;
            count+=(unsigned)n;hash=pcm_hash(hash,block,(unsigned)n);
            if(count>32000) return false;
        }
        report("SOUNDLAB_GOLDEN",voice,count,hash);
        if(count!=expected_samples[voice] || hash!=expected_hash[voice]) return false;
        zion_event_t unused;(void)host->event_wait(&unused,1);
    }
    return true;
}
static bool capture_play(void) {
    host->text_set(ZION_TEXT_STATUS,"PCM / ADPCM");nv_rec_start(&record);
    for(unsigned i=0;i<50;i++) {
        if(host->pcm_read(block,320)!=320) return false;
        if(nv_rec_push(&record,block,320)!=320) return false;
    }
    nv_play_start(&player,&record,(int)mode);unsigned samples=0;
    while(!player.done) {
        int count=nv_play_fill(&player,block,320);if(count<=0) break;
        if(host->pcm_write(block,(uint32_t)count)!=count) return false;
        samples+=(unsigned)count;
    }
    report("SOUNDLAB_CAPTURE_PLAY",mode,samples,(unsigned)nv_rec_bytes(&record));
    host->text_set(ZION_TEXT_STATUS,"完成");return true;
}
void zion_plugin_entry(const zion_api_t *api) {
    host=api;host->text_set(ZION_TEXT_TITLE,"ADPCM / DSP");
    host->text_set(ZION_TEXT_FOOTER,"上下选择  短按OK开始");
    host->text_set(ZION_TEXT_STATUS,"准备");
    if(!golden()) {host->log("SOUNDLAB_FAIL stage=golden");return;}
    host->log("SOUNDLAB_GOLDEN_PASS voices=12 upstream=NABO");
    int32_t stored=0;(void)host->setting_get("mode",&stored);mode=stored>=0 && stored<NV_COUNT?(unsigned)stored:0;
    bool pcm=(api->capabilities&ZION_CAP_PCM) && api->pcm_open && api->pcm_open(16000)==0;
    if(pcm) {
        api->volume_set(0); /* Automated hardware checks are deliberately muted. */
        int read=api->pcm_read(block,320);for(unsigned i=0;i<320;i++) block[i]=0;
        int written=api->pcm_write(block,320);
        if(read!=320 || written!=320) {api->log("SOUNDLAB_FAIL stage=pcm-io");return;}
        api->log("SOUNDLAB_PCM_PASS read=320 write=320 rate=16000 muted=1");
    } else api->log("SOUNDLAB_PCM_UNAVAILABLE");
    show();report("SOUNDLAB_READY",mode,0,0);api->text_set(ZION_TEXT_STATUS,"准备完成");
    for(;;) {
        zion_event_t event;if(api->event_wait(&event,100)!=1 || event.type!=ZION_EVENT_KEY || event.value!=ZION_KEY_CLICK) continue;
        if(event.code==ZION_KEY_UP || event.code==ZION_KEY_DOWN) {
            mode=(mode+NV_COUNT+(event.code==ZION_KEY_UP?-1:1))%NV_COUNT;
            int saved=api->setting_set("mode",(int32_t)mode);show();report("SOUNDLAB_MODE",mode,(unsigned)(saved==0),0);
        } else if(event.code==ZION_KEY_OK && pcm && !capture_play()) api->log("SOUNDLAB_FAIL stage=capture-play");
    }
}
