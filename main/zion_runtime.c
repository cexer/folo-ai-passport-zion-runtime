#include "zion_runtime.h"
#include "zion_runtime_model.h"
#include "zion_package.h"
#include "zion_builtin.h"
#include "zion_idf_compat.h"
#include "zion_service.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_freertos_hooks.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

extern void xiaozhi_runtime_entry(const zion_api_t *api);
extern bool zion_runtime_view_create(const char *name);
extern int32_t zion_runtime_text_set(uint32_t field,const char *text);
extern int32_t zion_runtime_glyph_add(const zion_glyph_t *glyphs,uint32_t count,uint32_t bpp);
extern void zion_runtime_glyph_clear(void);
extern int32_t zion_runtime_frame_begin(void);
extern int32_t zion_runtime_frame_write(uint32_t offset,const void *bytes,uint32_t count);
extern int32_t zion_runtime_frame_present(void);
extern void zion_runtime_frame_cancel(void);
extern uint32_t zion_runtime_asset_size(uint32_t asset);
extern int32_t zion_runtime_asset_read(uint32_t asset,uint32_t offset,void *out,uint32_t size);
static zion_runtime_model_t model;
static QueueHandle_t events;
static atomic_bool online;
static atomic_bool active;
static atomic_bool keys_active,manager_done;
static _Atomic(TaskHandle_t) pending_entry;
static bool entry_after_manager_cleanup(void) {
    /* IDF's idle loop reaps deleted tasks before invoking idle hooks. No waits,
     * allocation or logging here: wake the entry only after the manager exits. */
    TaskHandle_t task=atomic_exchange(&pending_entry,NULL);
    if(task) {
        esp_deregister_freertos_idle_hook_for_cpu(entry_after_manager_cleanup,0);
        xTaskNotifyGive(task);
    }
    return true;
}
static uint32_t pcm_rate;
static uint64_t uptime(void) {return (uint64_t)(esp_timer_get_time()/1000);}
static int32_t battery_percent(void) {return bsp_battery_soc();}
static int32_t network_online(void) {return atomic_load(&online)?1:0;}
static int32_t event_wait(zion_event_t *e,uint32_t ms) {
    if(!e || ms>60000) return ZION_E_ARGUMENT;
    return xQueueReceive(events,e,pdMS_TO_TICKS(ms))==pdTRUE?1:0;
}
static bool key_valid(const char *key) {return key && key[0] && strlen(key)<=15;}
static int32_t setting_get(const char *key,int32_t *value) {
    if(!key_valid(key) || !value || !model.plugin) return ZION_E_ARGUMENT;
    char space[16];snprintf(space,sizeof(space),"p_%s",model.plugin->id);
    nvs_handle_t h;if(nvs_open(space,NVS_READONLY,&h)!=ESP_OK) return ZION_E_IO;
    esp_err_t e=nvs_get_i32(h,key,value);nvs_close(h);return e==ESP_OK?0:ZION_E_IO;
}
static int32_t setting_set(const char *key,int32_t value) {
    if(!key_valid(key) || !model.plugin) return ZION_E_ARGUMENT;
    char space[16];snprintf(space,sizeof(space),"p_%s",model.plugin->id);
    nvs_handle_t h;if(nvs_open(space,NVS_READWRITE,&h)!=ESP_OK) return ZION_E_IO;
    esp_err_t e=nvs_set_i32(h,key,value);if(e==ESP_OK) e=nvs_commit(h);nvs_close(h);
    return e==ESP_OK?0:ZION_E_IO;
}
static int32_t pcm_open(uint32_t rate) {
    if(rate!=16000) return ZION_E_ARGUMENT; /* One duplex clock, no per-direction rate switch. */
    if(pcm_rate) return pcm_rate==rate?0:ZION_E_STATE;
#if ZION_RUNTIME_SIM
    return ZION_E_CAPABILITY;
#else
    esp_err_t e=bsp_audio_init();if(e==ESP_OK) e=bsp_audio_set_format(rate,16,1);
    if(e==ESP_OK) pcm_rate=rate;
    return e==ESP_OK?0:ZION_E_IO;
#endif
}
static int32_t pcm_read(int16_t *data,uint32_t count) {
    if(!pcm_rate || !data || !count || count>4096) return ZION_E_ARGUMENT;
    return bsp_audio_read(data,count*sizeof(*data))==ESP_OK?(int32_t)count:ZION_E_IO;
}
static int32_t pcm_write(const int16_t *data,uint32_t count) {
    if(!pcm_rate || !data || !count || count>4096) return ZION_E_ARGUMENT;
    return bsp_audio_write(data,count*sizeof(*data))==ESP_OK?(int32_t)count:ZION_E_IO;
}
static void volume_set(uint32_t v) {if(v<=100) bsp_audio_set_volume((uint8_t)v);}
static void backlight_set(uint32_t v) {if(v<=100) bsp_display_backlight((uint8_t)v);}
static void plugin_log(const char *message) {if(message) ESP_LOGI("zion-plugin","%s",message);}
static const zion_api_t api={
    .abi_version=ZION_ABI_VERSION,.struct_size=sizeof(zion_api_t),
    .capabilities=ZION_CAP_TEXT|ZION_CAP_KEYS|ZION_CAP_STORAGE|ZION_CAP_NETWORK_STATE
#if !ZION_RUNTIME_SIM
        |ZION_CAP_PCM
#endif
    , .uptime_ms=uptime,.text_set=zion_runtime_text_set,.event_wait=event_wait,
    .network_online=network_online,.battery_percent=battery_percent,
    .setting_get=setting_get,.setting_set=setting_set,.pcm_open=pcm_open,
    .pcm_read=pcm_read,.pcm_write=pcm_write,.volume_set=volume_set,.backlight_set=backlight_set,
    .glyph_add=zion_runtime_glyph_add,.glyph_clear=zion_runtime_glyph_clear,.log=plugin_log,
    .asset_size=zion_runtime_asset_size,.asset_read=zion_runtime_asset_read,
    .frame_begin=zion_runtime_frame_begin,.frame_write=zion_runtime_frame_write,
    .frame_present=zion_runtime_frame_present,.frame_cancel=zion_runtime_frame_cancel,
};
extern void zion_elf_installed_entry(const zion_api_t *api);
static const zion_plugin_t builtin={ZION_ABI_VERSION,sizeof(zion_plugin_t),sizeof(zion_api_t),ZION_CAP_TEXT|ZION_CAP_KEYS,"xiaozhi","内置小智",xiaozhi_runtime_entry};
static zion_plugin_t external[META_DATA_MAX];
static zion_package_t selected_package;
/* The package catalog already owns the complete headers. Retain only the
 * strings referenced by plugin descriptors, not a second copy of every header.
 * MEM-05: Octo Radio's Wi-Fi/MP3 pressure motivated this 912-byte C3 saving;
 * names must remain owned here until the next catalog refresh. */
static struct {char key[17],name[41];} external_names[META_DATA_MAX];
static int external_slots[META_DATA_MAX];
static unsigned external_count;
static atomic_uint default_id;
static void default_load(void) {
    nvs_handle_t h;uint32_t id=0;
    if(nvs_open("zion_builtin",NVS_READONLY,&h)==ESP_OK) {
        if(nvs_get_u32(h,"native",&id)!=ESP_OK) {uint8_t old=0;(void)nvs_get_u8(h,"default",&old);if(old==1) id=1;}
        nvs_close(h);
    }
    atomic_store(&default_id,id);
}
void zion_runtime_refresh(void) {
    if(zion_runtime_active()) return;
    zion_package_refresh();external_count=zion_package_count();
    for(unsigned i=0;i<external_count;i++) {
        external_slots[i]=zion_package_slot(i);
        zion_package_t p;
        if(!zion_package_info(external_slots[i],&p)) {external_count=i;break;}
        memcpy(external_names[i].key,p.key,sizeof(p.key));
        memcpy(external_names[i].name,p.name,sizeof(p.name));
        external[i]=(zion_plugin_t){p.abi,sizeof(zion_plugin_t),p.api_size,ZION_CAP_TEXT|ZION_CAP_KEYS,
            external_names[i].key,external_names[i].name,zion_elf_installed_entry};
    }
}
bool zion_runtime_selected_package(zion_package_t *out) {
    if(!selected_package.id) return false;
    *out=selected_package;return true;
}
static const zion_plugin_t *find(int slot) {
    if(slot==ZION_BUILTIN_SLOT) return &builtin;
    for(unsigned i=0;i<external_count;i++) if(external_slots[i]==slot) return &external[i];
    return NULL;
}
const char *zion_runtime_name(int slot) {const zion_plugin_t *p=find(slot);return p?p->name:NULL;}
const zion_api_t *zion_runtime_api(void) {return &api;}
void zion_runtime_init(void) {events=xQueueCreate(24,sizeof(zion_event_t));configASSERT(events);default_load();zion_runtime_refresh();}
bool zion_runtime_active(void) {return atomic_load(&active);}
bool zion_runtime_manager_done(void) {return atomic_load(&manager_done);}
bool zion_runtime_request(int slot) {
    if(zion_runtime_active()) return false;
    const zion_plugin_t *p=find(slot);
    memset(&selected_package,0,sizeof(selected_package));
    if(slot!=ZION_BUILTIN_SLOT && !zion_package_info(slot,&selected_package)) return false;
    int r=zion_runtime_validate(&api,p);
    if(r || model.phase!=ZION_RUNTIME_LIST || !zion_service_native_claim()) return false;
    r=zion_runtime_select(&model,&api,p);
    if(r) {zion_service_native_release();return false;}
    atomic_store(&active,true);
    ESP_LOGI("zion-runtime","SELECT slot=%d result=%d uptime_ms=%llu",slot,r,(unsigned long long)uptime());
    return r==0;
}
static void plugin_task(void *unused) {
    (void)unused;ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
    ESP_LOGI("zion-runtime","ENTER id=%s restart=0 uptime_ms=%llu free_heap=%lu",model.plugin->id,
        (unsigned long long)uptime(),(unsigned long)esp_get_free_heap_size());
    model.plugin->entry(&api);
    /* A returned entry may have live workers: never hot-unload it. */
    ESP_LOGE("zion-runtime","entry-returned:power-cycle-required");
    vTaskDelete(NULL);
}
void zion_runtime_tick(void) {
    if(model.phase!=ZION_RUNTIME_PENDING) return;
    TaskHandle_t task=NULL;
    uint32_t stack=selected_package.entry_stack?selected_package.entry_stack:selected_package.id?4096:8192;
    if(xTaskCreate(plugin_task,"zion-plugin",stack,NULL,5,&task)!=pdPASS) {
        model=(zion_runtime_model_t){0};atomic_store(&active,false);zion_service_native_release();
        ESP_LOGE("zion-runtime","load:task-allocation-failed");return;
    }
    if(!zion_runtime_view_create(model.plugin->name)) {
        vTaskDelete(task);model=(zion_runtime_model_t){0};atomic_store(&active,false);zion_service_native_release();
        ESP_LOGE("zion-runtime","load:view-lock-failed");return;
    }
    zion_service_native_handoff(!selected_package.id || (selected_package.features&1u),
        (selected_package.features&8u)!=0,(selected_package.features&16u)!=0);
    (void)zion_runtime_enter(&model);atomic_store(&keys_active,true);
    if(esp_register_freertos_idle_hook_for_cpu(entry_after_manager_cleanup,0)==ESP_OK) {
        atomic_store(&pending_entry,task);atomic_store(&manager_done,true);
    } else {
        ESP_LOGE("zion-runtime","manager-retirement:idle-hook-unavailable");xTaskNotifyGive(task);
    }
}
void zion_runtime_key(unsigned key,unsigned phase) {
    if(!atomic_load(&keys_active) || key>=bsp_button_configured_count() || phase>BSP_BTN_RELEASE) return;
    if(zion_idf_key(key,phase)) return;
    if(key>ZION_KEY_OK || phase>ZION_KEY_LONG) return;
    zion_event_t e={ZION_EVENT_KEY,key,phase};(void)xQueueSend(events,&e,0);
}
void zion_runtime_network(bool state) {
    atomic_store(&online,state);
    /* Network state is level-triggered through network_online; keys cannot lose
     * their queue capacity to a storm of reconnection notifications. */
}
int zion_runtime_default_slot(void) {
    /* Rendering runs concurrently with partition materialization. Never retain
     * or acquire NVS handles while its cached partition pointers are replaced. */
    uint32_t id=atomic_load(&default_id);
    return id==1?ZION_BUILTIN_SLOT:zion_package_slot_for_id(id);
}
bool zion_runtime_set_default(int slot) {
    uint32_t id=0;zion_package_t p;
    if(slot==ZION_BUILTIN_SLOT) id=1;
    else if(slot!=-1) {if(!zion_package_info(slot,&p)) return false;id=p.id;}
    nvs_handle_t h;if(nvs_open("zion_builtin",NVS_READWRITE,&h)!=ESP_OK) return false;
    esp_err_t e=nvs_set_u32(h,"native",id);
    if(e==ESP_OK) e=nvs_commit(h);
    nvs_close(h);if(e==ESP_OK) atomic_store(&default_id,id);return e==ESP_OK;
}
