"""Compile the real BSP with a recording ADC/button driver and failure injection."""
import argparse,pathlib,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--cc',required=True);a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='zion-buttons-') as tmp:
    d=pathlib.Path(tmp)
    files={
    'esp_err.h':'''#pragma once
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_ARG 0x102
static inline const char *esp_err_to_name(int e){(void)e;return "error";}
''',
    'esp_log.h':'''#pragma once
static inline void fake_log(const char *tag,const char *fmt,...){(void)tag;(void)fmt;}
#define ESP_LOGI(...) fake_log(__VA_ARGS__)
#define ESP_LOGE(...) fake_log(__VA_ARGS__)
#define ESP_LOGW(...) fake_log(__VA_ARGS__)
''',
    'bsp_pins.h':'''#pragma once
#define BSP_BTN_COUNT 3
#define BSP_BTN_MV_TABLE {{0,150},{150,447},{447,1900}}
#define BSP_BTN_ADC_UNIT 1
#define BSP_BTN_ADC_CHANNEL 0
#define BSP_BTN_GPIO 0
''',
    'esp_adc/adc_oneshot.h':'''#pragma once
#include "esp_err.h"
typedef void *adc_oneshot_unit_handle_t;
typedef struct {int unit_id;} adc_oneshot_unit_init_cfg_t;
#define ADC_BITWIDTH_DEFAULT 12
#define ADC_ATTEN_DB_12 12
int adc_oneshot_new_unit(const adc_oneshot_unit_init_cfg_t *,adc_oneshot_unit_handle_t *);
int adc_oneshot_del_unit(adc_oneshot_unit_handle_t);
int adc_oneshot_read(adc_oneshot_unit_handle_t,int,int *);
''',
    'esp_adc/adc_cali.h':'''#pragma once
typedef void *adc_cali_handle_t;
int adc_cali_raw_to_voltage(adc_cali_handle_t,int,int *);
''',
    'esp_adc/adc_cali_scheme.h':'''#pragma once
#include "adc_cali.h"
typedef struct {int unit_id,chan,atten,bitwidth;} adc_cali_curve_fitting_config_t;
int adc_cali_create_scheme_curve_fitting(const adc_cali_curve_fitting_config_t *,adc_cali_handle_t *);
int adc_cali_delete_scheme_curve_fitting(adc_cali_handle_t);
''',
    'iot_button.h':'''#pragma once
#include "esp_err.h"
typedef struct fake_button *button_handle_t;
typedef void (*button_cb_t)(void *,void *);
typedef enum {BUTTON_PRESS_DOWN,BUTTON_PRESS_UP,BUTTON_SINGLE_CLICK,BUTTON_DOUBLE_CLICK,BUTTON_LONG_PRESS_START} button_event_t;
typedef struct {struct {unsigned press_time;} long_press;} button_event_args_t;
typedef struct {unsigned unused;} button_config_t;
int iot_button_delete(button_handle_t);
int iot_button_register_cb(button_handle_t,button_event_t,button_event_args_t *,button_cb_t,void *);
''',
    'button_adc.h':'''#pragma once
#include "iot_button.h"
#include "esp_adc/adc_oneshot.h"
typedef struct {adc_oneshot_unit_handle_t *adc_handle;int unit_id,adc_channel;unsigned button_index,min,max;} button_adc_config_t;
int iot_button_new_adc_device(const button_config_t *,const button_adc_config_t *,button_handle_t *);
''',
    'driver/gpio.h':'''#pragma once
#define GPIO_MODE_INPUT 1
#define GPIO_PULLUP_ONLY 1
int gpio_set_direction(int,int);int gpio_set_pull_mode(int,int);int gpio_get_level(int);
''',
    'freertos/FreeRTOS.h':'#pragma once\n#define pdMS_TO_TICKS(x) (x)\n',
    'freertos/task.h':'#pragma once\nvoid vTaskDelay(unsigned);\n'}
    for name,body in files.items():
        path=d/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text(body)
    test='''#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "bsp_button.h"
#include "button_adc.h"
#include "esp_adc/adc_cali_scheme.h"
struct fake_button {int live;unsigned min,max;button_cb_t cb[5];void *user[5];};
static struct fake_button buttons[4];
static int unit_live,cal_live,live,created,deleted,fail_index=-1,fail_register,gpio_changes;
static int adc_token,cal_token,seen_key=-1,seen_event=-1;
int adc_oneshot_new_unit(const adc_oneshot_unit_init_cfg_t *cfg,void **out) {assert(!unit_live&&cfg->unit_id==1);unit_live=1;created++;*out=&adc_token;return 0;}
int adc_oneshot_del_unit(void *unit) {assert(unit==&adc_token&&unit_live&&!live&&!cal_live);unit_live=0;deleted++;return 0;}
int adc_oneshot_read(void *unit,int channel,int *raw) {assert(unit==&adc_token&&channel==0);*raw=212;return 0;}
int adc_cali_create_scheme_curve_fitting(const adc_cali_curve_fitting_config_t *cfg,void **out) {assert(unit_live&&!cal_live&&cfg->atten==12);cal_live=1;*out=&cal_token;return 0;}
int adc_cali_delete_scheme_curve_fitting(void *cal) {assert(cal==&cal_token&&cal_live&&!live);cal_live=0;return 0;}
int adc_cali_raw_to_voltage(void *cal,int raw,int *mv) {assert(cal==&cal_token&&cal_live);*mv=raw;return 0;}
int iot_button_new_adc_device(const button_config_t *cfg,const button_adc_config_t *adc,button_handle_t *out) {
 (void)cfg;assert(unit_live&&*adc->adc_handle==&adc_token&&adc->button_index<4);
 if((int)adc->button_index==fail_index){fail_index=-1;return ESP_ERR_NO_MEM;}
 struct fake_button *b=&buttons[adc->button_index];assert(!b->live);memset(b,0,sizeof(*b));b->live=1;b->min=adc->min;b->max=adc->max;*out=b;live++;return 0;
}
int iot_button_delete(button_handle_t b) {assert(b->live);b->live=0;live--;return 0;}
int iot_button_register_cb(button_handle_t b,button_event_t ev,button_event_args_t *args,button_cb_t cb,void *user) {
 assert(b->live);if(fail_register){fail_register=0;return ESP_ERR_NO_MEM;}
 if(ev==BUTTON_LONG_PRESS_START)assert(args&&args->long_press.press_time==1500);
 b->cb[ev]=cb;b->user[ev]=user;return 0;
}
int gpio_set_direction(int pin,int mode){(void)pin;(void)mode;gpio_changes++;return 0;}
int gpio_set_pull_mode(int pin,int mode){(void)pin;(void)mode;gpio_changes++;return 0;}
int gpio_get_level(int pin){(void)pin;return 1;}
void vTaskDelay(unsigned ticks){(void)ticks;}
static void callback(bsp_btn_t key,bsp_btn_ev_t ev,void *user) {assert(user==(void *)17);seen_key=key;seen_event=ev;}
static int classify(unsigned mv){for(unsigned i=0;i<bsp_button_configured_count();i++)if(buttons[i].min<=mv&&mv<buttons[i].max)return (int)i;return -1;}
int main(void) {
 const uint16_t combo[][2]={{0,150},{250,447},{447,1900},{150,250}};
 const uint16_t normal[][2]={{0,150},{150,447},{447,1900}};
 assert(!bsp_button_init(callback,(void *)17)&&live==3&&created==1);
 assert(!bsp_button_init(callback,(void *)17)&&created==1);
 assert(!bsp_button_configure_ranges(4,combo)&&live==4&&created==2&&deleted==1&&gpio_changes==0);
 assert(classify(0)==0&&classify(300)==1&&classify(600)==2&&classify(212)==3&&classify(3300)==-1);
 buttons[3].cb[BUTTON_PRESS_DOWN](&buttons[3],buttons[3].user[BUTTON_PRESS_DOWN]);assert(seen_key==3&&seen_event==BSP_BTN_PRESS);
 buttons[3].cb[BUTTON_PRESS_UP](&buttons[3],buttons[3].user[BUTTON_PRESS_UP]);assert(seen_event==BSP_BTN_RELEASE);
 assert(!bsp_button_configure_ranges(4,combo)&&created==2);
 assert(bsp_button_configure_ranges(2,normal)==ESP_ERR_INVALID_ARG&&live==4);
 uint16_t bad[4][2];memcpy(bad,combo,sizeof(bad));bad[3][0]=100;
 assert(bsp_button_configure_ranges(4,bad)==ESP_ERR_INVALID_ARG&&live==4);
 assert(!bsp_button_configure_ranges(3,normal)&&live==3);
 fail_index=3;assert(bsp_button_configure_ranges(4,combo)==ESP_ERR_NO_MEM);
 assert(bsp_button_configured_count()==3&&live==3&&unit_live&&cal_live&&classify(212)==1);
 fail_register=1;assert(bsp_button_configure_ranges(4,combo)==ESP_ERR_NO_MEM&&live==3&&bsp_button_configured_count()==3);
 assert(!bsp_button_configure_ranges(4,combo)&&live==4&&bsp_button_read_mv()==212);
 assert(bsp_button_key_voltage(3)==((int)combo[3][0]+combo[3][1])/2);
 assert(bsp_button_key_voltage(4)==-1);
 int level=0;assert(!bsp_button_prepare_deep_sleep(&level)&&level==1&&!live&&!unit_live&&!cal_live&&gpio_changes==2);
 return 0;
}
'''
    (d/'test.c').write_text(test)
    exe=d/'buttons.exe'
    subprocess.run([a.cc,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(d),'-I'+str(root/'components/bsp/include'),str(root/'components/bsp/src/bsp_button.c'),str(d/'test.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: real BSP profile lifecycle, combo classification, callbacks, idempotence and rollback')
