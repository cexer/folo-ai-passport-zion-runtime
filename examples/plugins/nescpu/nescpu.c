#include "zion_sdk.h"
#include "workload.h"
const uint32_t zion_plugin_abi=ZION_ABI_VERSION;
const uint32_t zion_plugin_api_size=sizeof(zion_api_t);
static const zion_api_t *host;
static char *number(char *out,uint32_t value) {
    char digits[10];unsigned n=0;do {digits[n++]=(char)('0'+value%10);value/=10;} while(value);
    while(n) *out++=digits[--n];*out=0;return out;
}
static char *text(char *out,const char *s) {while(*s) *out++=*s++;*out=0;return out;}
static void report(const char *name,cpu_result_t r) {
    char line[160];char *p=text(line,name);p=text(p," cycles=");p=number(p,r.cycles);p=text(p," hash=");p=number(p,r.hash);
    p=text(p," irq=");p=number(p,r.irq);p=text(p," nmi=");p=number(p,r.nmi);p=text(p," pc=");number(p,r.pc);host->log(line);
}
static void show(unsigned speed,bool running) {
    char body[96];char *p=text(body,"6502 / IRQ / NMI\nCPU ");p=number(p,4096u<<speed);text(p," cycles\n上下选择");
    host->text_set(ZION_TEXT_BODY,body);host->text_set(ZION_TEXT_STATUS,running?"CPU":"准备");
    host->text_set(ZION_TEXT_FOOTER,running?"短按OK关闭CPU":"短按OK开始");
}
void zion_plugin_entry(const zion_api_t *api) {
    host=api;host->text_set(ZION_TEXT_TITLE,"NES / 6502 CPU");host->text_set(ZION_TEXT_STATUS,"准备");
    workload_init();
    for(unsigned i=0;i<200;i++) {workload_slice(i,4096);if(i%10==0) {zion_event_t e;host->event_wait(&e,1);}}
    cpu_result_t r=workload_result();report("NESCPU_GOLDEN",r);
    if(r.hash!=1738864307u || r.cycles!=819539u || r.pc!=32800 || r.irq!=19 || r.nmi!=29 || r.jammed) {
        host->log("NESCPU_FAIL stage=golden");return;
    }
    host->log("NESCPU_GOLDEN_PASS source=nofrendo workload=200-slices");
    int32_t saved=0;(void)host->setting_get("speed",&saved);unsigned speed=saved>=0 && saved<=2?(unsigned)saved:0;
    workload_init();bool running=false;unsigned index=0;show(speed,running);host->log("NESCPU_READY");
    for(;;) {
        zion_event_t e;
        if(host->event_wait(&e,20)==1 && e.type==ZION_EVENT_KEY && e.value==ZION_KEY_CLICK) {
            if(e.code==ZION_KEY_OK) {
                running=!running;show(speed,running);report(running?"NESCPU_RUNNING":"NESCPU_PAUSED",workload_result());
            } else if(e.code==ZION_KEY_UP || e.code==ZION_KEY_DOWN) {
                speed=(speed+3+(e.code==ZION_KEY_UP?-1:1))%3;int result=host->setting_set("speed",(int32_t)speed);
                show(speed,running);host->log(result==0?"NESCPU_SETTING_SAVED":"NESCPU_FAIL stage=setting");
            }
        }
        if(!running) continue;
        workload_slice(index++,4096u<<speed);
        if(index%10==0) {r=workload_result();if(r.jammed) {host->log("NESCPU_FAIL stage=jammed");return;}report("NESCPU_PROGRESS",r);}
    }
}
