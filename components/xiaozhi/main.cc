#include "application.h"
#include "zion_sdk.h"
#include <esp_log.h>
#include <cstdio>
const zion_api_t *xiaozhi_host_api=nullptr;
extern "C" void xiaozhi_runtime_entry(const zion_api_t *api) {
    xiaozhi_host_api=api;
    ESP_LOGI("zion-xiaozhi","HOST_API abi=%lu display=shared keys=shared pcm=shared wifi=shared",
        (unsigned long)api->abi_version);
#if ZION_RUNTIME_SIM
    // Explicit simulator scope: actual SDK adapter/display/keys; no fake voice success.
    api->text_set(ZION_TEXT_STATUS,"模拟验证：音频与联网未运行");
    api->text_set(ZION_TEXT_BODY,"小智已直接进入\n\n复用ZION界面与按键\n\n上下调节测试值\n短按OK验证输入\n关机后按上键开机回列表");
    int value=0;
    for(;;) {
        zion_event_t e;
        if(api->event_wait(&e,500)==1 && e.type==ZION_EVENT_KEY) {
            if(e.code==ZION_KEY_OK && e.value==ZION_KEY_LONG)
                ESP_LOGI("zion-xiaozhi","SIM_LONG_OK restart=0");
            if(e.value!=ZION_KEY_CLICK) continue;
            if(e.code==ZION_KEY_UP) value++;
            if(e.code==ZION_KEY_DOWN) value--;
            char text[80];snprintf(text,sizeof(text),"KEY %lu / VALUE %d",(unsigned long)e.code,value);
            api->text_set(ZION_TEXT_STATUS,text);
            ESP_LOGI("zion-xiaozhi","SIM_KEY code=%lu value=%d",(unsigned long)e.code,value);
        }
    }
#else
    int audio_result=api->pcm_open(16000);
    if(audio_result) {
        ESP_LOGE("zion-xiaozhi","audio:open result=%d",audio_result);
        api->text_set(ZION_TEXT_STATUS,"音频初始化失败");
        api->text_set(ZION_TEXT_BODY,"请关机后重新开机");
        for(;;) {zion_event_t event;(void)api->event_wait(&event,1000);}
    }
    auto &app=Application::GetInstance();app.Initialize();app.Run();
#endif
}
