/* ABI/lifecycle test only; no gameplay or ZION-specific application calls. */
#include <string>
#include <mutex>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
extern "C" int mixed_c_sum(int,int);
static std::string initialized=std::string("global-")+"constructor";
static std::mutex mutex;
static std::string &local() {static std::string value="local-static";return value;}
static void worker(void *queue) {
    int value=0;
    if(xQueueReceive((QueueHandle_t)queue,&value,pdMS_TO_TICKS(1000))==pdTRUE && value==42)
        ESP_LOGI("link-capability","CPP_QUEUE_TASK_PASS value=%d",value);
    else ESP_LOGE("link-capability","CPP_CAPABILITY_FAIL queue");
    vTaskDelete(nullptr);
}
extern "C" void app_main(void) {
    {std::lock_guard<std::mutex> lock(mutex);
        if(initialized!="global-constructor" || local()!="local-static" || mixed_c_sum(40,2)!=42) {
            ESP_LOGE("link-capability","CPP_CAPABILITY_FAIL initialization");return;
        }}
    ESP_LOGI("link-capability","CPP_CONSTRUCTOR_GUARD_MUTEX_C_LINK_PASS");
    QueueHandle_t queue=xQueueCreate(2,sizeof(int));
    if(!queue || xTaskCreate(worker,"cpp-cap-worker",2048,queue,3,nullptr)!=pdPASS) {
        ESP_LOGE("link-capability","CPP_CAPABILITY_FAIL task-create");return;
    }
    int value=42;xQueueSend(queue,&value,0);vTaskDelay(pdMS_TO_TICKS(100));vQueueDelete(queue);
    ESP_LOGI("link-capability","CPP_CAPABILITY_PASS");
}
