#include "zion_prefs.h"
#include "esp_flash.h"
#include "meta_carve_store.h"
#include <string.h>

/* Dedicated reserved store sector: never share child NVS or carve records. */
#define PREFS_ADDRESS (META_STORE_OFFSET + META_STORE_SCRATCH_SECTOR * META_STORE_SECTOR_SIZE)
/* One record is sufficient to scan the append-only sector. Keep no 4 KiB
 * settings buffer resident while an application owns the device. */
static esp_err_t scan(zion_prefs_t *best,unsigned *append) {
    zion_prefs_t current;bool found=false;memset(best,0,sizeof(*best));
    if(append) *append=0;
    for(unsigned i=0;i<ZION_PREFS_SECTOR_SIZE/sizeof(current);i++) {
        esp_err_t err=esp_flash_read(NULL,&current,PREFS_ADDRESS+i*sizeof(current),sizeof(current));
        if(err!=ESP_OK) return err;
        if(append) {
            const uint8_t *raw=(const uint8_t *)&current;
            for(unsigned j=0;j<sizeof(current);j++) if(raw[j]!=0xff) {*append=i+1;break;}
        }
        if(zion_prefs_valid(&current) && (!found || (int32_t)(current.sequence-best->sequence)>0)) {
            *best=current;found=true;
        }
    }
    return ESP_OK;
}

esp_err_t zion_prefs_load(zion_prefs_t *prefs)
{
    memset(prefs, 0, sizeof(*prefs));
    zion_prefs_t best;esp_err_t err=scan(&best,NULL);
    if(err==ESP_OK) *prefs=best;
    return err;
}

esp_err_t zion_prefs_save(zion_prefs_t *prefs)
{
    zion_prefs_t old;unsigned next=0;
    esp_err_t err=scan(&old,&next);if(err!=ESP_OK) return err;
    prefs->sequence = old.sequence + 1;
    zion_prefs_seal(prefs);
    /* Skip torn records. Only completely erased records are appendable. */
    if (next >= ZION_PREFS_SECTOR_SIZE / sizeof(*prefs)) {
        err = esp_flash_erase_region(NULL, PREFS_ADDRESS, ZION_PREFS_SECTOR_SIZE);
        if (err != ESP_OK) return err;
        next = 0;
    }
    const uint32_t address = PREFS_ADDRESS + next * sizeof(*prefs);
    err = esp_flash_write(NULL, prefs, address, sizeof(*prefs));
    if (err != ESP_OK) return err;
    zion_prefs_t verify;
    err = esp_flash_read(NULL, &verify, address, sizeof(verify));
    if (err != ESP_OK) return err;
    return memcmp(prefs, &verify, sizeof(verify)) == 0 && zion_prefs_valid(&verify)
               ? ESP_OK : ESP_FAIL;
}
