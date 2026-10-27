#include "zion_prefs.h"
#include "meta_carve_store.h"
#include "esp_flash.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t sector[ZION_PREFS_SECTOR_SIZE];
static unsigned reads,writes,erases,fail_read;
#define BASE (META_STORE_OFFSET+META_STORE_SCRATCH_SECTOR*META_STORE_SECTOR_SIZE)
esp_err_t esp_flash_read(void *chip,void *out,uint32_t address,uint32_t size) {
    (void)chip;reads++;
    if(fail_read && reads==fail_read) return ESP_FAIL;
    assert(address>=BASE && address-BASE+size<=sizeof(sector));
    memcpy(out,sector+address-BASE,size);return ESP_OK;
}
esp_err_t esp_flash_write(void *chip,const void *in,uint32_t address,uint32_t size) {
    (void)chip;writes++;assert(address>=BASE && address-BASE+size<=sizeof(sector));
    const uint8_t *p=in;for(unsigned i=0;i<size;i++) {assert((sector[address-BASE+i]&p[i])==p[i]);sector[address-BASE+i]&=p[i];}
    return ESP_OK;
}
esp_err_t esp_flash_erase_region(void *chip,uint32_t address,uint32_t size) {
    (void)chip;assert(address==BASE && size==sizeof(sector));erases++;memset(sector,255,sizeof(sector));return ESP_OK;
}
static zion_prefs_t record(uint32_t sequence) {
    zion_prefs_t p={.sequence=sequence,.play_id=42,.flags=ZION_PREFS_ENABLED};zion_prefs_seal(&p);return p;
}
int main(void) {
    uint32_t random=19;
    for(unsigned trial=0;trial<100;trial++) {
        memset(sector,255,sizeof(sector));
        for(unsigned i=0;i<sizeof(sector)/sizeof(zion_prefs_t);i++) {
            random=random*1664525u+1013904223u;if(random%3==0) continue;
            zion_prefs_t p=record(trial<50?i:UINT32_MAX-30u+i);
            if(random%5==0) p.crc^=1;
            memcpy(sector+i*sizeof(p),&p,sizeof(p));
        }
        zion_prefs_t expected={0},got;int latest=zion_prefs_latest(sector);
        if(latest>=0) memcpy(&expected,sector+(unsigned)latest*sizeof(expected),sizeof(expected));
        assert(zion_prefs_load(&got)==ESP_OK && !memcmp(&expected,&got,sizeof(got)));
    }
    memset(sector,255,sizeof(sector));zion_prefs_t p=record(1),got;
    memcpy(sector,&p,sizeof(p));sector[sizeof(p)]=0;sector[10*sizeof(p)+15]=0;
    p.play_id=77;assert(zion_prefs_save(&p)==ESP_OK && p.sequence==2 && erases==0);
    assert(!memcmp(sector+11*sizeof(p),&p,sizeof(p)));
    for(unsigned i=0;i<sizeof(sector)/sizeof(p);i++) {p=record(i+1);memcpy(sector+i*sizeof(p),&p,sizeof(p));}
    p.play_id=88;assert(zion_prefs_save(&p)==ESP_OK && erases==1 && p.sequence==65);
    assert(zion_prefs_load(&got)==ESP_OK && !memcmp(&p,&got,sizeof(p)));
    reads=0;fail_read=17;memset(&got,255,sizeof(got));
    assert(zion_prefs_load(&got)==ESP_FAIL);zion_prefs_t zero={0};assert(!memcmp(&got,&zero,sizeof(got)));
    unsigned old_writes=writes;reads=0;assert(zion_prefs_save(&p)==ESP_FAIL && writes==old_writes);
    puts("Prefs Flash: PASS (scan parity, sequence wrap, torn slots, rollover and read failure)");
}
