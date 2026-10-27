/* Conventional ESP-IDF application: no ZION header, callback table or #ifdef.
 * This is a compatibility fixture, not a claim of a full NES application port.
 * The external Nofrendo CPU and workload sources are shared with standalone. */
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_i2c.h"
#include "bsp_battery.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lvgl.h"
#include "workload.h"
#include <stdio.h>
static QueueHandle_t keys;
static lv_obj_t *body;
static int32_t count;
static void button(bsp_btn_t key,bsp_btn_ev_t phase,void *user) {
    (void)user;if(phase==BSP_BTN_CLICK) (void)xQueueSend(keys,&key,0);
}
static void show(void) {
    char text[160];snprintf(text,sizeof(text),"6502 reference: PASS\nIRQ 19 / NMI 29\nCounter %ld\nUP +1 / DOWN -1\nOK saves to NVS",(long)count);
    if(bsp_lvgl_lock(1000)) {lv_label_set_text(body,text);bsp_lvgl_unlock();}
}
static void worker(void *unused) {
    (void)unused;
    ESP_LOGI("compat-app","COMPAT_WORKER_READY count=%ld heap=%lu",(long)count,(unsigned long)esp_get_free_heap_size());
    for(;;) {
        bsp_btn_t key;if(xQueueReceive(keys,&key,pdMS_TO_TICKS(1000))!=pdTRUE) continue;
        if(key==BSP_BTN_UP) count++;
        if(key==BSP_BTN_DOWN) count--;
        if(key==BSP_BTN_OK) {
            nvs_handle_t h;ESP_ERROR_CHECK(nvs_open("compat_probe",NVS_READWRITE,&h));
            ESP_ERROR_CHECK(nvs_set_i32(h,"count",count));ESP_ERROR_CHECK(nvs_commit(h));nvs_close(h);
            ESP_LOGI("compat-app","COMPAT_SAVED count=%ld",(long)count);
        }
        show();ESP_LOGI("compat-app","COMPAT_KEY key=%u count=%ld",(unsigned)key,(long)count);
    }
}
void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());ESP_ERROR_CHECK(bsp_i2c_init());
    ESP_ERROR_CHECK(bsp_display_init());
    if(!bsp_lvgl_init()) {ESP_LOGE("compat-app","COMPAT_FAIL lvgl-init");return;}
    (void)bsp_battery_init();
    workload_init();for(unsigned i=0;i<200;i++) workload_slice(i,4096);
    cpu_result_t r=workload_result();
    if(r.cycles!=819539 || r.hash!=1738864307 || r.pc!=32800 || r.irq!=19 || r.nmi!=29 || r.jammed) {
        ESP_LOGE("compat-app","COMPAT_FAIL golden");return;
    }
    ESP_LOGI("compat-app","COMPAT_GOLDEN cycles=%lu hash=%lu irq=%u nmi=%u",(unsigned long)r.cycles,(unsigned long)r.hash,r.irq,r.nmi);
    nvs_handle_t h;if(nvs_open("compat_probe",NVS_READONLY,&h)==ESP_OK) {(void)nvs_get_i32(h,"count",&count);nvs_close(h);}
    if(!bsp_lvgl_lock(2000)) return;
    lv_obj_t *screen=lv_obj_create(NULL);lv_obj_set_style_bg_color(screen,lv_color_hex(0x101820),0);
    lv_obj_set_style_bg_opa(screen,LV_OPA_COVER,0);lv_obj_set_style_text_color(screen,lv_color_hex(0xd0edb0),0);
    lv_obj_set_style_text_font(screen,LV_FONT_DEFAULT,0);
    lv_obj_t *title=lv_label_create(screen);lv_obj_set_pos(title,10,12);lv_label_set_text(title,"ESP-IDF / app_main");
    body=lv_label_create(screen);lv_obj_set_pos(body,10,65);lv_obj_set_width(body,218);lv_screen_load(screen);bsp_lvgl_unlock();show();
    keys=xQueueCreate(12,sizeof(bsp_btn_t));
    if(!keys) {ESP_LOGE("compat-app","COMPAT_FAIL queue");return;}
    ESP_ERROR_CHECK(bsp_button_init(button,NULL));
    if(xTaskCreate(worker,"compat-worker",3072,NULL,4,NULL)!=pdPASS) ESP_LOGE("compat-app","COMPAT_FAIL task");
    /* Normal IDF semantics: app_main returns and its task is retired. */
}
