#include "zion_sdk.h"
const uint32_t zion_plugin_abi=ZION_ABI_VERSION;
const uint32_t zion_plugin_api_size=sizeof(zion_api_t);
static char *number(char *p,int32_t n) {
    uint32_t v=n<0?0u-(uint32_t)n:(uint32_t)n;
    char digits[11];unsigned count=0;
    if(n<0) *p++='-';
    do {digits[count++]=(char)('0'+v%10);v/=10;} while(v);
    while(count) *p++=digits[--count];
    *p=0;return p;
}
static char *copy(char *p,const char *s) {
    while(*s) *p++=*s++;
    *p=0;return p;
}
void zion_plugin_entry(const zion_api_t *a) {
    int32_t count=0;(void)a->setting_get("count",&count);
    a->text_set(ZION_TEXT_TITLE,"ELF 公共能力测试");
    a->text_set(ZION_TEXT_FOOTER,"UP +1  DOWN -1  OK 保存");
    for(;;) {
        char body[160];char *end=copy(body,"独立 ELF / ABI 1\n\n");end=number(end,count);
        end=copy(end,"\n\nWi-Fi: ");copy(end,a->network_online()?"ON":"OFF");
        a->text_set(ZION_TEXT_BODY,body);
        a->text_set(ZION_TEXT_STATUS,"关机后按上键开机回列表");
        zion_event_t event;
        if(a->event_wait(&event,500)!=1 || event.type!=ZION_EVENT_KEY) continue;
        if(event.code==ZION_KEY_OK && event.value==ZION_KEY_LONG) a->log("DEMO_LONG_OK restart=0");
        if(event.value!=ZION_KEY_CLICK) continue;
        if(event.code==ZION_KEY_UP && count<INT32_MAX) count++;
        if(event.code==ZION_KEY_DOWN && count>INT32_MIN) count--;
        if(event.code==ZION_KEY_OK) {
            int32_t result=a->setting_set("count",count);
            char line[80];end=copy(line,"DEMO_SAVE value=");end=number(end,count);
            end=copy(end," result=");number(end,result);a->log(line);
        }
    }
}
