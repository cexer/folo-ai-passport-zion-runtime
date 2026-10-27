#pragma once
#include "esp_ota_ops.h"
/* Standalone-image OTA cannot update a live relocatable module or its host. */
const esp_partition_t *zion_idf_ota_next(const esp_partition_t *start);
esp_err_t zion_idf_ota_begin(const esp_partition_t *,size_t,esp_ota_handle_t *);
esp_err_t zion_idf_ota_write(esp_ota_handle_t,const void *,size_t);
esp_err_t zion_idf_ota_abort(esp_ota_handle_t);
esp_err_t zion_idf_ota_end(esp_ota_handle_t);
esp_err_t zion_idf_ota_boot(const esp_partition_t *);
esp_err_t zion_idf_ota_valid(void);
