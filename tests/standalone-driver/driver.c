/* Extra test component only. No changes to third-party business sources. */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "button_adc.h"
#include "iot_button.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
/* Passport OS exposed a lost ACK while the application and LCD capture wrote
 * concurrently. IDF's stdio VFS queues characters with a short drop timeout;
 * control records instead need one bounded, atomic USB ring item. */
static int test_control_reply(const char *format,...) __attribute__((format(printf,1,2)));
static int test_control_reply(const char *format,...) {
    char line[384];va_list args;va_start(args,format);
    int length=vsnprintf(line,sizeof(line),format,args);va_end(args);
    if(length<0 || (unsigned)length>=sizeof(line)) return -1;
    if(usb_serial_jtag_is_driver_installed())
        return usb_serial_jtag_write_bytes(line,(size_t)length,pdMS_TO_TICKS(2000));
    size_t written=fwrite(line,1,(size_t)length,stdout);fflush(stdout);return (int)written;
}
#define printf test_control_reply
#ifdef ZION_STANDALONE_BSP
#include "bsp_button.h"
#include "bsp_pins.h"
typedef bsp_btn_cb_t board_callback_t;
static board_callback_t board_callback;
static void *board_user;
#if defined(ZION_STANDALONE_KEY_EDGES) && !defined(ZION_STANDALONE_ADC_POLL)
static void pulse_dispatch(unsigned key,unsigned phase) {board_callback((bsp_btn_t)key,(bsp_btn_ev_t)phase,board_user);}
#endif
#endif
typedef struct {button_handle_t handle;button_event_t event;button_cb_t callback;void *user;} registration_t;
static registration_t registrations[32];
static unsigned registration_count;
static button_handle_t handles[3];
#ifdef ZION_STANDALONE_BSP
esp_err_t __real_bsp_button_init(board_callback_t,void *);
esp_err_t __wrap_bsp_button_init(board_callback_t callback,void *user) {
    board_callback=callback;board_user=user;
    return __real_bsp_button_init(callback,user);
}
#endif
esp_err_t __real_iot_button_new_adc_device(const button_config_t *,const button_adc_config_t *,button_handle_t *);
esp_err_t __wrap_iot_button_new_adc_device(const button_config_t *cfg,const button_adc_config_t *adc,button_handle_t *out) {
    esp_err_t r=__real_iot_button_new_adc_device(cfg,adc,out);
    if(r==ESP_OK && adc->button_index<3) handles[adc->button_index]=*out;
    return r;
}
esp_err_t __real_iot_button_register_cb(button_handle_t,button_event_t,button_event_args_t *,button_cb_t,void *);
esp_err_t __wrap_iot_button_register_cb(button_handle_t handle,button_event_t event,button_event_args_t *args,button_cb_t cb,void *user) {
    esp_err_t r=__real_iot_button_register_cb(handle,event,args,cb,user);
    if(r==ESP_OK && registration_count<32) registrations[registration_count++]=(registration_t){handle,event,cb,user};
    return r;
}
static void key(unsigned index,bool long_press) {
#ifdef ZION_STANDALONE_ADC_POLL
    extern void zion_test_adc_key(unsigned,bool);
    zion_test_adc_key(index,long_press);return;
#endif
#ifdef ZION_STANDALONE_BSP
    if(board_callback) {board_callback((bsp_btn_t)index,long_press?BSP_BTN_LONG:BSP_BTN_CLICK,board_user);return;}
#endif
    button_event_t event=long_press?BUTTON_LONG_PRESS_START:BUTTON_SINGLE_CLICK;
    for(unsigned i=0;i<registration_count;i++) {
        registration_t *r=&registrations[i];if(r->handle==handles[index] && r->event==event) r->callback(r->handle,r->user);
    }
}
extern bool zion_test_capture_begin(void);
void zion_usb_test_control(const char *line) {
    unsigned adc_key,adc_ms;
    if(sscanf(line,"ZION ADC %u %u",&adc_key,&adc_ms)==2) {
#ifdef ZION_STANDALONE_ADC_POLL
        extern bool zion_test_adc_pulse(unsigned,unsigned);
        bool valid=zion_test_adc_pulse(adc_key,adc_ms);
        printf("ZION_ADC_DONE key=%u ms=%u result=%d\n",adc_key,adc_ms,valid?0:-1);
#else
        puts("ZION_ADC_UNSUPPORTED");
#endif
        return;
    }
#if defined(ZION_STANDALONE_BSP) && defined(ZION_STANDALONE_KEY_EDGES) && !defined(ZION_STANDALONE_ADC_POLL)
        unsigned edge_key,edge_phase;
        if(sscanf(line,"ZION PULSE %u %u",&edge_key,&edge_phase)==2) {
            extern bool zion_test_button_pulse(unsigned,unsigned,void (*)(unsigned,unsigned));
            if(!board_callback || edge_key>=BSP_BTN_COUNT || !zion_test_button_pulse(edge_key,edge_phase,pulse_dispatch)) puts("ZION_PULSE_FAIL");
            fflush(stdout);return;
        }
        if(sscanf(line,"ZION EDGE %u %u",&edge_key,&edge_phase)==2) {
            bool valid=board_callback && edge_key<BSP_BTN_COUNT && (edge_phase==BSP_BTN_PRESS || edge_phase==BSP_BTN_RELEASE);
            if(valid) board_callback((bsp_btn_t)edge_key,(bsp_btn_ev_t)edge_phase,board_user);
            printf("ZION_EDGE_DONE key=%u phase=%u result=%d\n",edge_key,edge_phase,valid?0:-1);fflush(stdout);return;
        }
#endif
        if(!strcmp(line,"ZION CAPTURE")) {printf("ZION_CAPTURE armed=%d\n",zion_test_capture_begin());}
        else if(!strcmp(line,"ZION MEM")) printf("ZION_MEMORY free=%u largest=%u\n",(unsigned)esp_get_free_heap_size(),
            (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        else if(!strncmp(line,"ZION KEY ",9)) {
            const char *name=line+9;bool lng=strstr(name,"LONG")!=NULL;
            unsigned index=!strncmp(name,"UP",2)?0:!strncmp(name,"DOWN",4)?1:2;
            key(index,lng);printf("STANDALONE_KEY key=%u long=%d\n",index,lng);
        }
}
#ifndef ZION_STANDALONE_USB
static void console(void *unused) {
    (void)unused;char line[64];unsigned used=0;
    for(;;) {
        int ch=getchar();if(ch==EOF) {clearerr(stdin);vTaskDelay(pdMS_TO_TICKS(5));continue;}
        if(ch=='\r') continue;
        if(ch!='\n') {if(used+1<sizeof(line)) line[used++]=(char)ch;continue;}
        line[used]=0;used=0;
        zion_usb_test_control(line);
    }
}
#endif
extern void __real_app_main(void);
void __wrap_app_main(void) {
#if !defined(ZION_STANDALONE_USB) || defined(ZION_STANDALONE_STDIO)
    /* FIFO-only stdio can lose an ACK while an audio or BLE task logs. Match
     * the host's buffered test console without taking over app-owned USB. */
    if(!usb_serial_jtag_is_driver_installed()) {
        usb_serial_jtag_driver_config_t config={.rx_buffer_size=1024,.tx_buffer_size=1024};
        ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&config));
    }
    usb_serial_jtag_vfs_use_driver();
    usb_serial_jtag_vfs_set_rx_line_endings(ESP_LINE_ENDINGS_LF);
#ifdef ZION_STANDALONE_USB
    extern bool zion_test_usb_control_init(void);
    configASSERT(zion_test_usb_control_init());
#endif
#ifndef ZION_STANDALONE_USB
    configASSERT(xTaskCreate(console,"standalone-test",4096,NULL,3,NULL)==pdPASS);
#endif
#endif
    puts("STANDALONE_TEST_READY");__real_app_main();
}
