/* Per-package persistent storage, exposed through the normal IDF partition API.
 * Keep this descriptor alive for wear-levelling handles until power off. */
#include "zion_runtime.h"
#include "meta_carve_flash.h"
#include "esp_partition.h"
#include "esp_flash.h"
#include <string.h>

const esp_partition_t *__real_esp_partition_find_first(esp_partition_type_t,
    esp_partition_subtype_t,const char *);

const esp_partition_t *__wrap_esp_partition_find_first(esp_partition_type_t type,
    esp_partition_subtype_t subtype,const char *label) {
    static esp_partition_t partition;
    zion_package_t app;
    if(type==ESP_PARTITION_TYPE_DATA && label &&
       zion_runtime_selected_package(&app) && app.storage_size &&
       !strcmp(label,app.storage_label)) {
        const meta_carve_t *carve=meta_carve_flash_carve();
        int index=meta_carve_find_data(carve,app.id,label);
        if(index<0) return NULL;
        const meta_carve_data_t *data=&carve->data[index];
        if(subtype!=ESP_PARTITION_SUBTYPE_ANY && subtype!=data->subtype) return NULL;
        partition=(esp_partition_t){.flash_chip=esp_flash_default_chip,.type=type,
            .subtype=data->subtype,.address=data->offset,.size=data->size,.erase_size=4096};
        memcpy(partition.label,data->label,sizeof(partition.label));
        return &partition;
    }
    return __real_esp_partition_find_first(type,subtype,label);
}
