#pragma once
#include "esp_err.h"
#include "meta_carve.h"
#include "zion_policy.h"
#include "zion_menu.h"
#include "zion_install_view.h"
typedef struct {
    bool busy,online,connected,ap,skip_autostart;
    unsigned state,error,percent,stage,generation;
    unsigned install_id;
    zion_install_info_t install;
    esp_err_t result;
    char detail[64],ap_name[33],ap_password[9];
    zion_prefs_t prefs;
} zion_service_state_t;
bool zion_service_catalog(meta_carve_t *catalog);
void zion_service_get(zion_service_state_t *state);
bool zion_service_request(zion_action_t action,int slot);
bool zion_service_native_claim(void);
void zion_service_native_release(void);
void zion_service_native_handoff(bool keep_network,bool keep_bluetooth,bool owns_usb);
void zion_service_native_control(const char *line);
void zion_service_package_progress(const char *name,uint32_t received,uint32_t total,int status,const char *detail);
void zion_service_package_footprint(uint32_t bytes);
void zion_launcher_init(void);
void zion_launcher_suspend(void);
void zion_launcher_key(zion_input_t key);
bool zion_launcher_cancel_auto(void);
void zion_launcher_tick(void);
void zion_launcher_snapshot(void);
void zion_launcher_fonts(void);
void zion_launcher_qr_pixels(void);
