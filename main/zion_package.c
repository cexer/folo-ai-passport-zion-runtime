#include "zion_package.h"
#include "meta_carve_flash.h"
#include "zion_service.h"
#include "zion592_install.h"
#include "esp_flash.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "psa/crypto.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "driver/usb_serial_jtag.h"
/* Small FIFO-only VFS writes can drop an entire stop-and-wait ACK. Use the
 * installed interrupt driver's bounded, buffered write for protocol records. */
static void control_reply(const char *format, ...) {
    char line[192];va_list args;
    va_start(args,format);int size=vsnprintf(line,sizeof(line),format,args);va_end(args);
    if(size<=0 || (unsigned)size>=sizeof(line)) return;
    fflush(stdout);
#if !ZION_RUNTIME_SIM
    if(usb_serial_jtag_is_driver_installed()) {
        int sent=usb_serial_jtag_write_bytes(line,(size_t)size,pdMS_TO_TICKS(2000));
        if(sent!=size || usb_serial_jtag_wait_tx_done(pdMS_TO_TICKS(2000))!=ESP_OK)
            ESP_LOGE("zion-package","USB_CONTROL_TX_FAILED sent=%d expected=%d",sent,size);
        return;
    }
#endif
    fputs(line,stdout);fflush(stdout);
}
static zion_package_t packages[META_DATA_MAX];
static int slots[META_DATA_MAX];
static unsigned count;
void zion_package_refresh(void) {
    count=0;const meta_carve_t *c=meta_carve_flash_carve();
    for(unsigned i=0;i<c->data_count;i++) {
        const meta_carve_data_t *d=&c->data[i];
        if(d->subtype!=ZION_PACKAGE_SUBTYPE || strncmp(d->label,"zp_",3)) continue;
        uint8_t header[ZION_PACKAGE_HEADER];zion_package_t p;
        if(esp_flash_read(NULL,header,d->offset,sizeof(header))!=ESP_OK || !zion_package_parse(header,&p) || p.id!=d->play_id) continue;
        p.flash_offset=d->offset;
        if(!zion_package_extent(&p,d->size,&p.extent)) continue;
        p.extent=d->size;packages[count]=p;slots[count]=ZION_PACKAGE_SLOT_BASE-(int)i;count++;
    }
}
unsigned zion_package_count(void) {return count;}
int zion_package_slot(unsigned index) {return index<count?slots[index]:-1;}
int zion_package_slot_for_id(uint32_t id) {for(unsigned i=0;i<count;i++) if(packages[i].id==id) return slots[i];return -1;}
bool zion_package_info(int slot,zion_package_t *out) {
    for(unsigned i=0;i<count;i++) if(slots[i]==slot) {*out=packages[i];return true;}
    return false;
}
bool zion_package_verify(const zion_package_t *p) {
    uint8_t bytes[1024],digest[32];size_t length;
    psa_hash_operation_t sha=PSA_HASH_OPERATION_INIT;
    ESP_LOGI("zion-package","VERIFY_BEGIN bytes=%lu free=%lu largest=%lu",(unsigned long)p->total,
        (unsigned long)esp_get_free_heap_size(),(unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    psa_status_t crypto=psa_crypto_init();
    ESP_LOGI("zion-package","VERIFY_CRYPTO status=%ld free=%lu largest=%lu",(long)crypto,
        (unsigned long)esp_get_free_heap_size(),(unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    if(crypto!=PSA_SUCCESS || psa_hash_setup(&sha,PSA_ALG_SHA_256)!=PSA_SUCCESS) return false;
    bool ok=true;
    for(uint32_t at=ZION_PACKAGE_HEADER;at<p->total && ok;) {
        uint32_t n=p->total-at;if(n>sizeof(bytes)) n=sizeof(bytes);
        ok=esp_flash_read(NULL,bytes,p->flash_offset+at,n)==ESP_OK && psa_hash_update(&sha,bytes,n)==PSA_SUCCESS;at+=n;
    }
    ok=ok && psa_hash_finish(&sha,digest,sizeof(digest),&length)==PSA_SUCCESS && length==32 && !memcmp(digest,p->sha256,32);
    psa_hash_abort(&sha);return ok;
}
static bool read_exact(void *out,size_t size) {
    uint8_t *bytes=out;size_t at=0;int64_t deadline=esp_timer_get_time()+15000000;
    while(at<size) {
        int value=getchar();
        if(value==EOF) {
            clearerr(stdin);
            if(esp_timer_get_time()>deadline) {
                ESP_LOGE("zion-package","USB_RX_TIMEOUT received=%u expected=%u",(unsigned)at,(unsigned)size);
                return false;
            }
            vTaskDelay(pdMS_TO_TICKS(2));continue;
        }
        bytes[at++]=(uint8_t)value;deadline=esp_timer_get_time()+15000000;
    }
    return true;
}
int zion_package_receive(uint32_t total) {
    uint32_t flash_size=0;
    if(esp_flash_get_size(NULL,&flash_size)!=ESP_OK || total<ZION_PACKAGE_HEADER ||
       total>flash_size || !zion_service_native_claim()) {
        control_reply("ZION_UPLOAD_DONE result=-1 stage=busy-or-size\n");fflush(stdout);return -1;
    }
    control_reply("ZION_UPLOAD_READY bytes=%lu\n",(unsigned long)total);fflush(stdout);
    zion_service_package_progress(NULL,0,total,0,"usb-receiving");
    uint8_t header[ZION_PACKAGE_HEADER];zion_package_t p;int result=-1;const char *stage="header";
    meta_carve_t *next=calloc(1,sizeof(*next));uint8_t *buffer=malloc(1024);uint32_t offset=0,extent=0,received=0;bool touched=false,committed=false;
    uint32_t storage_offset=0,storage_extent=0;bool storage_touched=false;
    if(!next || !buffer || !read_exact(header,sizeof(header)) || !zion_package_parse(header,&p) || p.total!=total) goto done;
    zion_service_package_progress(p.name,sizeof(header),total,0,"usb-writing");
    stage="allocation";*next=*meta_carve_flash_carve();
    if(!zion_package_extent(&p,flash_size,&extent)) goto done;
    int old=-1;for(unsigned i=0;i<next->data_count;i++) if(next->data[i].subtype==ZION_PACKAGE_SUBTYPE && next->data[i].play_id==p.id) old=(int)i;
    if(!meta_carve_place_data(next,extent,&offset)) goto done;
    /* Carve DATA starts are 64 KiB aligned, so the offset-zero extent above is
     * exact. Worst-case padding falsely rejects reusable gaps after updates. */
    if(offset%META_CARVE_OFFSET_ALIGN) goto done;
    uint32_t planned_extent=extent;
    p.flash_offset=offset;if(!zion_package_extent(&p,planned_extent,&extent) || extent!=planned_extent) goto done;
    uint32_t storage_bytes=p.storage_size;
    int storage_index=p.storage_size?meta_carve_find_data(next,p.id,p.storage_label):-1;
    if(storage_index>=0) storage_bytes=next->data[storage_index].size;
    zion_service_package_footprint(extent+storage_bytes);
    stage="erase";
    if(esp_flash_erase_region(NULL,offset,extent)!=ESP_OK) goto done;
    touched=true;stage="flash-write";
    if(esp_flash_write(NULL,header,offset,sizeof(header))!=ESP_OK) goto done;
    received=sizeof(header);
    control_reply("ZION_UPLOAD_ACK bytes=%u\n",(unsigned)sizeof(header));fflush(stdout);
    for(uint32_t at=sizeof(header);at<total;) {
        uint32_t n=total-at;if(n>1024) n=1024;
        stage="usb-read";
        if(!read_exact(buffer,n)) goto done;
        stage="flash-write";
        esp_err_t written=esp_flash_write(NULL,buffer,offset+at,n);
        if(written!=ESP_OK) {
            ESP_LOGE("zion-package","FLASH_WRITE_FAIL result=%d offset=0x%lx bytes=%u",(int)written,(unsigned long)(offset+at),(unsigned)n);
            goto done;
        }
        at+=n;received=at;
        zion_service_package_progress(p.name,at,total,0,"usb-writing");
        control_reply("ZION_UPLOAD_ACK bytes=%lu\n",(unsigned long)at);fflush(stdout);
    }
    stage="payload-sha256";p.flash_offset=offset;p.extent=extent;if(!zion_package_verify(&p)) goto done;
    uint32_t old_offset=0,old_size=0;
    if(old>=0) {old_offset=next->data[old].offset;old_size=next->data[old].size;meta_carve_remove_data(next,(uint8_t)old);}
    meta_carve_data_t d={.play_id=p.id,.offset=offset,.size=extent,.state=META_DATA_DIRTY,.subtype=ZION_PACKAGE_SUBTYPE,.type=1};
    snprintf(d.label,sizeof(d.label),"zp_%08lx",(unsigned long)p.id);
    stage="metadata";if(!meta_carve_data_append(next,&d)) goto done;
    if(p.storage_size) {
        stage="storage-allocation";int existing=meta_carve_find_data(next,p.id,p.storage_label);
        if(existing>=0) {
            if(next->data[existing].size<p.storage_size || next->data[existing].subtype!=p.storage_subtype) goto done;
        } else {
            storage_extent=p.storage_size;
            if(!meta_carve_place_data(next,storage_extent,&storage_offset)) goto done;
            meta_carve_data_t storage={.play_id=p.id,.offset=storage_offset,.size=storage_extent,
                .state=META_DATA_DIRTY,.subtype=p.storage_subtype,.type=1};
            snprintf(storage.label,sizeof(storage.label),"%s",p.storage_label);
            if(!meta_carve_data_append(next,&storage)) goto done;
            stage="storage-erase";
            if(esp_flash_erase_region(NULL,storage_offset,storage_extent)!=ESP_OK) goto done;
            storage_touched=true;
        }
    }
    /* Once a metadata commit has been attempted, never erase possibly-live
     * bytes merely because an uncertain readback returned an error. */
    committed=true;
    if(meta_carve_flash_commit(next,true)!=ESP_OK) {
        (void)meta_carve_flash_reload();goto done;
    }
    committed=true;if(old_size) (void)esp_flash_erase_region(NULL,old_offset,old_size);
    result=0;stage="installed";ESP_LOGI("zion-package","INSTALLED id=%lu bytes=%lu offset=0x%lx",(unsigned long)p.id,(unsigned long)total,(unsigned long)offset);
done:
    if(touched && !committed) (void)esp_flash_erase_region(NULL,offset,extent);
    if(storage_touched && !committed) (void)esp_flash_erase_region(NULL,storage_offset,storage_extent);
    free(buffer);free(next);meta_carve_flash_workspace_release();zion_service_native_release();
    zion_service_package_progress(NULL,received,total,result==0?1:-1,result==0?"installed":stage);
    control_reply("ZION_UPLOAD_DONE result=%d stage=%s received=%lu total=%lu\n",result,stage,(unsigned long)received,(unsigned long)total);fflush(stdout);return result;
}
int zion_package_remove(int slot) {
    zion_package_t p;if(!zion_package_info(slot,&p)) return -1;
    for(;;) {
        const meta_carve_t *c=meta_carve_flash_carve();bool found=false;
        for(unsigned i=0;i<c->data_count;i++) if(c->data[i].play_id==p.id) {found=true;break;}
        if(!found) return 0;
        unsigned before=c->data_count;
        if(meta_carve_flash_erase_data(p.id,NULL)!=ESP_OK || meta_carve_flash_carve()->data_count>=before) return -1;
    }
}
void zion_package_recover(void) {
    if(meta_carve_find_data(meta_carve_flash_carve(),UINT32_MAX,"zp_stage")>=0) {
        esp_err_t result=meta_carve_flash_erase_data(UINT32_MAX,"zp_stage");
        ESP_LOGW("zion-package","STAGE_RECOVERY result=%d",(int)result);
    }
}
typedef struct {uint32_t total;} legacy_context_t;
static void legacy_progress(unsigned stage,unsigned percent,const char *detail,const zion_install_info_t *info,void *ctx) {
    (void)percent;legacy_context_t *c=ctx;
    zion_service_package_progress(info->name[0]?info->name:"独立固件",info->received_bytes?info->received_bytes:1,c->total,
        stage==255?-1:stage==100?1:0,detail);
}
int zion_legacy_receive(uint32_t total,const uint8_t expected[32]) {
    if(total<256 || total>0x200000 || !zion_service_native_claim()) {
        control_reply("ZION_UPLOAD_DONE result=-1 stage=busy-or-size\n");fflush(stdout);return -1;
    }
    const char *stage="stage-allocation";int result=-1,slot=-1;uint32_t received=0,offset=0;
    uint8_t *buffer=malloc(1024);meta_carve_t *next=calloc(1,sizeof(*next));bool reserved=false;
    zion_service_package_progress("独立固件",0,total,0,"usb-staging");
    if(!buffer || !next) goto done;
    if(meta_carve_find_data(meta_carve_flash_carve(),UINT32_MAX,"zp_stage")>=0 &&
       meta_carve_flash_erase_data(UINT32_MAX,"zp_stage")!=ESP_OK) goto done;
    *next=*meta_carve_flash_carve();uint32_t extent=(total+4095u)&~4095u;
    if(!meta_carve_place_data(next,extent,&offset)) goto done;
    meta_carve_data_t data={.play_id=UINT32_MAX,.offset=offset,.size=extent,.state=META_DATA_DIRTY,.type=1,.subtype=ZION_PACKAGE_SUBTYPE};
    strcpy(data.label,"zp_stage");
    if(!meta_carve_data_append(next,&data) || meta_carve_flash_commit(next,true)!=ESP_OK) goto done;
    reserved=true;stage="stage-erase";
    if(esp_flash_erase_region(NULL,offset,extent)!=ESP_OK) goto done;
    control_reply("ZION_UPLOAD_READY bytes=%lu header=0\n",(unsigned long)total);fflush(stdout);
    stage="usb-stage-write";
    while(received<total) {
        uint32_t n=total-received;if(n>1024) n=1024;
        if(!read_exact(buffer,n) || esp_flash_write(NULL,buffer,offset+received,n)!=ESP_OK) goto done;
        received+=n;zion_service_package_progress("独立固件",received,total,0,"usb-staging");
        control_reply("ZION_UPLOAD_ACK bytes=%lu\n",(unsigned long)received);fflush(stdout);
    }
    stage="source-sha256";
    psa_hash_operation_t hash=PSA_HASH_OPERATION_INIT;uint8_t digest[32];size_t length=0;
    if(psa_crypto_init()!=PSA_SUCCESS || psa_hash_setup(&hash,PSA_ALG_SHA_256)!=PSA_SUCCESS) goto done;
    bool hash_ok=true;
    for(uint32_t at=0;at<total && hash_ok;) {
        uint32_t n=total-at;if(n>1024) n=1024;
        hash_ok=esp_flash_read(NULL,buffer,offset+at,n)==ESP_OK && psa_hash_update(&hash,buffer,n)==PSA_SUCCESS;at+=n;
    }
    hash_ok=hash_ok && psa_hash_finish(&hash,digest,sizeof(digest),&length)==PSA_SUCCESS && length==32 && !memcmp(digest,expected,32);
    psa_hash_abort(&hash);if(!hash_ok) goto done;
    /* The canonical DIRTY stage record now owns its reservation. The normal
     * installer needs its own workspace; don't retain our duplicate proposal. */
    free(buffer);buffer=NULL;free(next);next=NULL;
    stage="legacy-install";legacy_context_t context={total};
    result=z592_install_flash(offset,total,expected,legacy_progress,&context,&slot)==ESP_OK?0:-1;
done:
    if(reserved) {
        esp_err_t cleanup=meta_carve_flash_erase_data(UINT32_MAX,"zp_stage");
        if(cleanup!=ESP_OK) ESP_LOGE("zion-package","STAGE_CLEANUP result=%d",(int)cleanup);
    }
    free(buffer);free(next);meta_carve_flash_workspace_release();zion_service_native_release();
    zion_service_package_progress(NULL,received,total,result==0?1:-1,result==0?"installed":stage);
    control_reply("ZION_UPLOAD_DONE result=%d kind=legacy slot=%d stage=%s\n",result,slot,stage);fflush(stdout);return result;
}
