#include "zion_idf_ota.h"
const esp_partition_t *zion_idf_ota_next(const esp_partition_t *start) {(void)start;return NULL;}
esp_err_t zion_idf_ota_begin(const esp_partition_t *partition,size_t length,esp_ota_handle_t *handle) {
    (void)partition;(void)length;if(handle) *handle=0;return ESP_ERR_NOT_SUPPORTED;
}
esp_err_t zion_idf_ota_write(esp_ota_handle_t handle,const void *data,size_t length) {
    (void)handle;(void)data;(void)length;return ESP_ERR_NOT_SUPPORTED;
}
esp_err_t zion_idf_ota_abort(esp_ota_handle_t handle) {(void)handle;return ESP_ERR_NOT_SUPPORTED;}
esp_err_t zion_idf_ota_end(esp_ota_handle_t handle) {(void)handle;return ESP_ERR_NOT_SUPPORTED;}
esp_err_t zion_idf_ota_boot(const esp_partition_t *partition) {(void)partition;return ESP_ERR_NOT_SUPPORTED;}
esp_err_t zion_idf_ota_valid(void) {return ESP_ERR_NOT_SUPPORTED;}
