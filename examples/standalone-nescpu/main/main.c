#include "zion_sdk.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_i2c.h"
#include "bsp_battery.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/usb_serial_jtag_vfs.h"
#include <stdio.h>
#include <string.h>
LV_FONT_DECLARE(zion_font_ui_14);
extern void zion_plugin_entry(const zion_api_t *api);
static QueueHandle_t events;
static lv_obj_t *labels[4];
static uint64_t uptime(void) {return (uint64_t)(esp_timer_get_time()/1000);}
static int32_t text_set(uint32_t field,const char *text) {
    if(field>=4 || !text || !bsp_lvgl_lock(1000)) return ZION_E_ARGUMENT;
    lv_label_set_text(labels[field],text);bsp_lvgl_unlock();return 0;
}
static int32_t wait_event(zion_event_t *event,uint32_t ms) {return xQueueReceive(events,event,pdMS_TO_TICKS(ms))==pdTRUE?1:0;}
static int32_t online(void) {return 0;}
static int32_t battery(void) {return bsp_battery_soc();}
static int32_t get(const char *key,int32_t *value) {
    nvs_handle_t h;if(nvs_open("nescpu",NVS_READONLY,&h)!=ESP_OK) return ZION_E_IO;
    int result=nvs_get_i32(h,key,value);nvs_close(h);return result==ESP_OK?0:ZION_E_IO;
}
static int32_t set(const char *key,int32_t value) {
    nvs_handle_t h;if(nvs_open("nescpu",NVS_READWRITE,&h)!=ESP_OK) return ZION_E_IO;
    int result=nvs_set_i32(h,key,value);if(result==ESP_OK) result=nvs_commit(h);nvs_close(h);return result==ESP_OK?0:ZION_E_IO;
}
static void log_line(const char *s) {ESP_LOGI("nescpu-standalone","%s",s);}
static void button(bsp_btn_t key,bsp_btn_ev_t phase,void *unused) {
    (void)unused;zion_event_t event={ZION_EVENT_KEY,(uint32_t)key,(uint32_t)phase};(void)xQueueSend(events,&event,0);
}
static void console(void *unused) {
    (void)unused;char line[64];unsigned length=0;usb_serial_jtag_vfs_set_rx_line_endings(ESP_LINE_ENDINGS_LF);
    for(;;) {
        int value=getchar();if(value==EOF) {clearerr(stdin);vTaskDelay(pdMS_TO_TICKS(10));continue;}
        if(value=='\n' || value=='\r') {
            line[length]=0;length=0;
            if(!strcmp(line,"ZION KEY UP")) button(BSP_BTN_UP,BSP_BTN_CLICK,NULL);
            if(!strcmp(line,"ZION KEY DOWN")) button(BSP_BTN_DOWN,BSP_BTN_CLICK,NULL);
            if(!strcmp(line,"ZION KEY OK")) button(BSP_BTN_OK,BSP_BTN_CLICK,NULL);
            if(!strcmp(line,"ZION KEY LONG")) button(BSP_BTN_OK,BSP_BTN_LONG,NULL);
        } else if(length<sizeof(line)-1) line[length++]=(char)value;
    }
}
void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());events=xQueueCreate(16,sizeof(zion_event_t));configASSERT(events);
    ESP_ERROR_CHECK(bsp_i2c_init());ESP_ERROR_CHECK(bsp_display_init());configASSERT(bsp_lvgl_init());
    (void)bsp_battery_init();bsp_display_backlight(100);
    configASSERT(bsp_lvgl_lock(2000));lv_obj_t *screen=lv_obj_create(NULL);
    lv_obj_set_style_bg_opa(screen,LV_OPA_COVER,0);lv_obj_set_style_bg_color(screen,lv_color_hex(0x101810),0);
    lv_obj_set_style_text_color(screen,lv_color_hex(0xe0f0c0),0);lv_obj_set_style_text_font(screen,&zion_font_ui_14,0);
    const int positions[]={10,42,80,292};
    for(unsigned i=0;i<4;i++) {labels[i]=lv_label_create(screen);lv_obj_set_pos(labels[i],10,positions[i]);lv_obj_set_width(labels[i],220);lv_label_set_text(labels[i],"");}
    lv_obj_set_height(labels[2],200);lv_screen_load(screen);bsp_lvgl_unlock();
    ESP_ERROR_CHECK(bsp_button_init(button,NULL));configASSERT(xTaskCreate(console,"nescpu-usb",4096,NULL,3,NULL)==pdPASS);
    const zion_api_t api={.abi_version=ZION_ABI_VERSION,.struct_size=sizeof(zion_api_t),
        .capabilities=ZION_CAP_TEXT|ZION_CAP_KEYS|ZION_CAP_STORAGE|ZION_CAP_NETWORK_STATE,
        .uptime_ms=uptime,.text_set=text_set,.event_wait=wait_event,.network_online=online,.battery_percent=battery,
        .setting_get=get,.setting_set=set,.log=log_line};
    ESP_LOGI("nescpu-standalone","STANDALONE_READY code=same-nescpu-source ZION-launcher=absent");
    zion_plugin_entry(&api);
}
