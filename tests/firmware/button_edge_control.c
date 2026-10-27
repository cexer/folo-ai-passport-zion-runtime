#include "button_edge_control.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <stdatomic.h>
#include <stdio.h>
static atomic_flag busy=ATOMIC_FLAG_INIT;
static struct {
    unsigned key,milliseconds;
    void (*dispatch)(unsigned,unsigned);
    TaskHandle_t parent;
    int64_t elapsed;
} job;
static void pulse_worker(void *unused) {
    (void)unused;
    int64_t start=esp_timer_get_time();job.dispatch(job.key,0);
    vTaskDelay(pdMS_TO_TICKS(job.milliseconds));
    job.elapsed=esp_timer_get_time()-start;job.dispatch(job.key,4);
    /* Let the application's input dispatcher consume RELEASE before console
     * logging. UART acknowledgement latency must not extend the press. */
    vTaskDelay(1);xTaskNotifyGive(job.parent);vTaskDelete(NULL);
}
bool zion_test_button_pulse(unsigned key,unsigned milliseconds,void (*dispatch)(unsigned,unsigned)) {
    if(!dispatch || !milliseconds || milliseconds>5000 || atomic_flag_test_and_set(&busy)) return false;
    job.key=key;job.milliseconds=milliseconds;job.dispatch=dispatch;job.parent=xTaskGetCurrentTaskHandle();
    (void)ulTaskNotifyTake(pdTRUE,0);
    if(xTaskCreate(pulse_worker,"test-edge",4096,NULL,configMAX_PRIORITIES-3,NULL)!=pdPASS) {
        atomic_flag_clear(&busy);return false;
    }
    if(!ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(milliseconds+2000))) return false;
    printf("ZION_PULSE_DONE key=%u requested=%u device_us=%lld\n",key,milliseconds,(long long)job.elapsed);fflush(stdout);
    atomic_flag_clear(&busy);return true;
}
