#pragma once
#include "esp_err.h"
#include "zion592_wire.h"
#include "zion_install_view.h"
typedef void (*z592_progress_fn)(unsigned stage,unsigned percent,const char *detail,
                                  const zion_install_info_t *info,void *context);
void z592_install_prepare(void);
void z592_install_cancel(void);
esp_err_t z592_install_request(const z592_request_t *request,z592_progress_fn progress,void *context,int *installed_slot);
esp_err_t z592_boot_installed(int slot);
esp_err_t z592_install_flash(uint32_t offset,uint32_t size,const uint8_t sha256[32],z592_progress_fn progress,void *context,int *installed_slot);
/* Test-only comparison override: -1 restores normal policy (no modem sleep).
 * Production has no console command that can change this override. */
void z592_install_test_power_save(int enabled);
