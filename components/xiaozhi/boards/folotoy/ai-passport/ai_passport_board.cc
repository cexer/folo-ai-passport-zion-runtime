// Native XiaoZhi adapter: the ZION host owns every physical device.
#include "board.h"
#include "display/text_display.h"
#include "audio/audio_codec.h"
#include "application.h"
#include "zion_sdk.h"
#include <esp_network.h>
#include <esp_log.h>
#include <atomic>
extern const zion_api_t *xiaozhi_host_api;

class HostCodec final : public AudioCodec {
public:
    HostCodec() {duplex_=true;input_sample_rate_=output_sample_rate_=16000;}
    void Start() override {
        AudioCodec::Start();
        int result=xiaozhi_host_api->pcm_open(16000);
        if(result) {ESP_LOGE("zion-xiaozhi","audio:open result=%d",result);return;}
        xiaozhi_host_api->volume_set(output_volume_);
    }
    void SetOutputVolume(int volume) override {
        if(volume<0 || volume>100) return;
        AudioCodec::SetOutputVolume(volume);xiaozhi_host_api->volume_set(volume);
    }
protected:
    int Read(int16_t *data,int count) override {return xiaozhi_host_api->pcm_read(data,count);}
    int Write(const int16_t *data,int count) override {return xiaozhi_host_api->pcm_write(data,count);}
};
class HostBacklight final : public Backlight {
    void SetBrightnessImpl(uint8_t brightness) override {xiaozhi_host_api->backlight_set(brightness);}
};
class AiPassportBoard final : public Board {
    TextDisplay display_;
    NetworkEventCallback callback_;
    std::atomic_bool network_started_{false};
    static void InputTask(void *arg) {
        auto self=static_cast<AiPassportBoard*>(arg);int previous=-1;
        for(;;) {
            zion_event_t event;
            if(xiaozhi_host_api->event_wait(&event,50)==1 && event.type==ZION_EVENT_KEY) {
                if(event.value==ZION_KEY_LONG && event.code!=ZION_KEY_OK) {
                    Application::GetInstance().Schedule([event]() {
                        auto &board=Board::GetInstance();
                        board.GetAudioCodec()->SetOutputVolume(event.code==ZION_KEY_UP?100:0);
                    });
                }
                if(event.value!=ZION_KEY_CLICK) continue;
                Application::GetInstance().Schedule([event]() {
                    auto &app=Application::GetInstance();
                    if(event.code==ZION_KEY_OK) app.ToggleChatState();
                    else {
                        auto codec=Board::GetInstance().GetAudioCodec();
                        int value=codec->output_volume()+(event.code==ZION_KEY_UP?10:-10);
                        if(value<0) value=0;
                        if(value>100) value=100;
                        codec->SetOutputVolume(value);
                        Board::GetInstance().GetDisplay()->ShowNotification("音量 "+std::to_string(value));
                    }
                });
            }
            if(self->network_started_) {
                int online=xiaozhi_host_api->network_online();
                if(online!=previous) {
                    previous=online;
                    Application::GetInstance().Schedule([self,online]() {
                        if(self->callback_) self->callback_(online?NetworkEvent::Connected:NetworkEvent::Disconnected,"");
                        if(!online) self->display_.SetStatus("请关机后进入ZION配网");
                    });
                }
            }
        }
    }
public:
    AiPassportBoard() {
        // Only a lightweight event pump is new; no second ADC/SPI/LVGL/Wi-Fi init.
        configASSERT(xTaskCreate(InputTask,"plugin-input",4096,this,4,nullptr)==pdPASS);
    }
    std::string GetBoardType() override {return "wifi";}
    AudioCodec *GetAudioCodec() override {static HostCodec codec;return &codec;}
    Display *GetDisplay() override {return &display_;}
    Backlight *GetBacklight() override {static HostBacklight light;return &light;}
    NetworkInterface *GetNetwork() override {static EspNetwork network;return &network;}
    void StartNetwork() override {network_started_=true;}
    void SetNetworkEventCallback(NetworkEventCallback cb) override {callback_=std::move(cb);}
    const char *GetNetworkStateIcon() override {return "";}
    void SetPowerSaveLevel(PowerSaveLevel) override {} // The host owns radio policy.
    bool GetBatteryLevel(int &level,bool &charging,bool &discharging) override {
        level=xiaozhi_host_api->battery_percent();charging=false;discharging=true;return level>=0;
    }
    std::string GetBoardJson() override {return R"({"type":"folotoy-ai-passport","name":"ZION","manufacturer":"FoloToy"})";}
    std::string GetDeviceStatusJson() override {return xiaozhi_host_api->network_online()?R"({"network":{"type":"wifi","online":true}})":R"({"network":{"type":"wifi","online":false}})";}
};
DECLARE_BOARD(AiPassportBoard);
