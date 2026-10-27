/* Bounded diagnostics at SDK import boundaries; application sources stay intact. */
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static QueueHandle_t key_queue;
static bool tracing;
void zion_test_runtime_trace(bool value) {tracing=value;}
QueueHandle_t zion_test_queue_create(UBaseType_t length,UBaseType_t size,uint8_t type) {
    QueueHandle_t queue=xQueueGenericCreate(length,size,type);
    if(size==1) {key_queue=queue;if(tracing) printf("ZION_KEY_QUEUE created=%p capacity=%u\n",queue,(unsigned)length);}
    return queue;
}
BaseType_t zion_test_queue_send(QueueHandle_t queue,const void *item,TickType_t wait,BaseType_t where) {
    BaseType_t r=xQueueGenericSend(queue,item,wait,where);
    if(tracing && queue==key_queue) printf("ZION_KEY_TX value=%u result=%d\n",*(const uint8_t *)item,(int)r);
    return r;
}
BaseType_t zion_test_queue_receive(QueueHandle_t queue,void *item,TickType_t wait) {
    BaseType_t r=xQueueReceive(queue,item,wait);
    if(tracing && queue==key_queue && r) printf("ZION_KEY_RX value=%u stack=%u\n",*(uint8_t *)item,(unsigned)uxTaskGetStackHighWaterMark(NULL));
    return r;
}
bool zion_test_lvgl_lock(uint32_t ms) {
    int64_t start=esp_timer_get_time();bool result=lvgl_port_lock(ms);int64_t elapsed=esp_timer_get_time()-start;
    if(tracing && (!result || elapsed>100000)) printf("ZION_LOCK task=%s result=%d elapsed_us=%lld\n",pcTaskGetName(NULL),result,(long long)elapsed);
    return result;
}
void zion_test_tasks(void) {
    const char *names[]={"zion-plugin","folotoy_room","turn_tone","LVGL","zion-control"};
    for(unsigned i=0;i<sizeof(names)/sizeof(names[0]);i++) {
        TaskHandle_t task=xTaskGetHandle(names[i]);if(task) printf("ZION_TASK name=%s state=%d stack=%u\n",names[i],(int)eTaskGetState(task),(unsigned)uxTaskGetStackHighWaterMark(task));
    }
    fflush(stdout);
}
extern int zion_test_usb_read(void *,uint32_t,uint32_t);
extern int zion_test_adc_read(void);
extern int zion_test_fgetc(FILE *);
extern void zion_service_native_control(const char *);
void zion_usb_test_control(const char *line) {zion_service_native_control(line);}
uintptr_t zion_test_runtime_resolve(const char *name) {
#define MAP(symbol,target) if(!strcmp(name,#symbol)) return (uintptr_t)&target;
    MAP(usb_serial_jtag_read_bytes,zion_test_usb_read)
    MAP(fgetc,zion_test_fgetc)
    MAP(bsp_button_read_mv,zion_test_adc_read)
    MAP(xQueueGenericCreate,zion_test_queue_create)
    MAP(xQueueGenericSend,zion_test_queue_send)
    MAP(xQueueReceive,zion_test_queue_receive)
    MAP(lvgl_port_lock,zion_test_lvgl_lock)
#undef MAP
    return 0;
}
