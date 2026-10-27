#pragma once
#include "esp_dpp.h"
/* Xiao Liu Ren exposed the SDK5 deprecated callback argument removed in SDK6.
 * NULL uses real SDK6 WIFI_EVENT_DPP_* events unchanged. A non-NULL legacy
 * callback is explicitly unsupported, never silently accepted or discarded. */
static inline esp_err_t zion_sdk5_dpp_init(void *callback) {
    return callback ? ESP_ERR_NOT_SUPPORTED : esp_supp_dpp_init();
}
#define esp_supp_dpp_init(callback) zion_sdk5_dpp_init(callback)
