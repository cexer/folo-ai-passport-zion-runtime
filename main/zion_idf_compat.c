#include "zion_idf_compat.h"
#include "zion_idf_contract.h"
#include "zion_idf_dns.h"
#include "zion_idf_ota.h"
#include "zion_button_events.h"
#include "zion_idf_link_hooks.h"
#include "bsp_display.h"
#include "bsp_battery.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "lvgl.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
/* Picolibc's supported errno accessor mode avoids baking the host TLS layout
 * into a module while preserving errno independently for every FreeRTOS task. */
int *zion_idf_errno(void) {return &errno;}
/* Reviewed import provenance: Pinball added scalar math; V-Pet added observer
 * GAP and LVGL timer exports; Snoopy added clock/image-cache/audio-wake exports;
 * Connect Four adds central/peripheral GATT and landscape drawing providers.
 * Case revisions, failures and device evidence: docs/runtime-optimization-log.md.
 * These declarations force real SDK providers, never success-only substitutes. */
#define ZION_AUTO_IMPORT(index,name) extern unsigned char zion_auto_##index[] __asm__(name);
#include "zion_idf_auto_imports.inc"
#undef ZION_AUTO_IMPORT

static _Atomic(bsp_btn_cb_t) callback;
static atomic_bool polling_buttons;
static atomic_flag registered=ATOMIC_FLAG_INIT;
static void *callback_user;
static unsigned callback_events=31u;
static unsigned callback_release=4u;
static void configure_button_events(unsigned mask) {callback_events=mask&31u;}
static void configure_button_release(unsigned release) {
    callback_release=release;
    ESP_LOGI("zion-compat","BUTTON_EVENTS mask=%u release=%u",callback_events,release);
}
/* These subsystems are already owned by the runtime. Reinitialization would
 * replace callbacks/drivers. Low-level driver ownership is intentionally absent. */
static esp_err_t compat_init(void) {return ESP_OK;}
static lv_display_t *compat_lvgl_init(void) {return lv_display_get_default();}
static esp_err_t compat_button_init(bsp_btn_cb_t cb,void *user) {
    /* NULL means that the app polls the shared calibrated ADC itself. */
    if(!cb) {atomic_store(&polling_buttons,true);return ESP_OK;}
    if(atomic_flag_test_and_set(&registered)) return ESP_ERR_INVALID_STATE;
    callback_user=user;atomic_store(&callback,cb);return ESP_OK;
}
bool zion_idf_key(unsigned key,unsigned phase) {
    bsp_btn_cb_t cb=atomic_load(&callback);
    if(cb && zion_button_event_allowed(callback_events,phase)) cb((bsp_btn_t)key,(bsp_btn_ev_t)zion_button_event_translate(callback_release,phase),callback_user);
    return cb!=NULL;
}
bool zion_idf_polling_buttons(void) {return atomic_load(&polling_buttons);}
static void contract_accept(void) {ESP_LOGI("zion-compat","CONTRACT_ACCEPT fingerprint=%s",ZION_IDF_CONTRACT_HASH);}
extern void zion_idf_run_initializers(void);
uintptr_t zion_idf_resolve(const char *name) {
    if(!strcmp(name,ZION_IDF_CONTRACT_SYMBOL)) return (uintptr_t)contract_accept;
    uintptr_t real=zion_idf_link_real(name);if(real) return real;
#if ZION_RUNTIME_TEST_KEYS
    extern uintptr_t zion_test_runtime_resolve(const char *);
    uintptr_t test=zion_test_runtime_resolve(name);if(test) return test;
#endif
    uintptr_t board=zion_idf_board_resolve(name);if(board) return board;
#define ZION_IMPORT(symbol,target) if(!strcmp(name,#symbol)) return (uintptr_t)&target;
#include "zion_idf_exports.inc"
#undef ZION_IMPORT
#define ZION_AUTO_IMPORT(index,symbol) if(!strcmp(name,symbol)) return (uintptr_t)zion_auto_##index;
#include "zion_idf_auto_imports.inc"
#undef ZION_AUTO_IMPORT
    return 0;
}
