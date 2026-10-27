#include "zion592_install.h"
#include "zion592_image.h"
#include "meta_carve_flash.h"
#include "meta_name.h"
#include "zion_title.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_flash.h"
#include "esp_image_format.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "psa/crypto.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdatomic.h>
#include <stdio.h>

static const char *TAG="z592-install";
static bool s_flash_source_enabled;
static uint32_t s_flash_source_offset;
static atomic_bool s_cancelled;
static atomic_int s_test_power_save=-1;
void z592_install_test_power_save(int enabled) {
    atomic_store(&s_test_power_save,enabled);
    if(enabled>=0) {
        esp_err_t result=esp_wifi_set_ps(enabled?WIFI_PS_MIN_MODEM:WIFI_PS_NONE);
        ESP_LOGI(TAG,"Z592_TEST_PS enabled=%d result=%d",enabled,(int)result);
    }
}
void z592_install_prepare(void) {atomic_store(&s_cancelled,false);}
void z592_install_cancel(void) {atomic_store(&s_cancelled,true);}
typedef struct {
    const z592_request_t *request;
    z592_image_t *image;
    meta_carve_t *proposal;
    int slot;
    uint8_t *buffer,*tail;
    z592_progress_fn progress;
    void *context;
    const char *failure;
    unsigned last_percent;
    bool app_touched,commit_started;
    int replace;
    int source_app;
    uint32_t layout_crc;
    bool signed_tail;
    bool layout_verified;
    int64_t started_us,read_us,write_us,last_report_us;
    uint64_t read_cpu_us;
    zion_install_info_t info;
} install_t;
static uint32_t task_cpu_us(void) {
#if CONFIG_META_E2E_TEST_CONTROL && CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
    TaskStatus_t status;vTaskGetInfo(NULL,&status,pdFALSE,eInvalid);
    return (uint32_t)status.ulRunTimeCounter;
#else
    return 0;
#endif
}
static bool task_cpu_stats_enabled(void) {
#if CONFIG_META_E2E_TEST_CONTROL && CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
    return true;
#else
    return false;
#endif
}
static void update_info(install_t *s) {
    if(!s->image) return;
    const z592_image_t *p=s->image;
    int chosen=p->selected;
    if(chosen<0) {
        /* A factory takes precedence. Do not mislabel an ambiguous OTA bundle. */
        int ota=-1;unsigned otas=0;
        for(unsigned i=0;i<p->app_count;i++) {
            if(p->apps[i].subtype==0) {chosen=(int)i;break;}
            if(p->apps[i].present) {ota=(int)i;otas++;}
        }
        if(chosen<0 && otas==1) chosen=ota;
    }
    if(chosen<0) return;
    const z592_source_app_t *a=&p->apps[chosen];
    if(!a->bad && a->header_ready && p->received>=a->source+sizeof(a->prefix)) {
        char name[33]={0};memcpy(name,a->prefix+80,32);
        bool clean=true;for(unsigned i=0;name[i];i++) if((unsigned char)name[i]<32) clean=false;
        if(!s->info.name[0] && clean && name[0] && meta_name_utf8_ok(name,strlen(name))) {
            strcpy(s->info.name,name);s->info.project_name=true;
        }
    }
    if(a->done) {
        uint32_t bytes=meta_carve_need(a->image_len);
        for(unsigned i=0;i<p->data_count;i++) bytes+=p->data[i].size;
        s->info.install_bytes=bytes;s->info.install_known=true;
    }
}
static void report(install_t *s,unsigned stage,unsigned percent,const char *detail) {
    update_info(s);s->last_percent=percent;
    if(stage!=255) s->info.phase=stage;
    ESP_LOGI(TAG,"Z592_INSTALL stage=%u percent=%u detail=%s heap=%lu largest=%lu",stage,percent,detail,
        (unsigned long)esp_get_free_heap_size(),(unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    s->progress(stage,percent,detail,&s->info,s->context);
    ESP_LOGI(TAG,"Z592_STACK free_units=%u",(unsigned)uxTaskGetStackHighWaterMark(NULL));
}
static esp_err_t write_checked(uint32_t target,const uint8_t *bytes,uint32_t size) {
    esp_err_t err=esp_flash_write(NULL,bytes,target,size);
    uint8_t verify[256];
    for(uint32_t at=0;err==ESP_OK && at<size;) {
        uint32_t n=size-at>sizeof(verify)?sizeof(verify):size-at;
        err=esp_flash_read(NULL,verify,target+at,n);
        if(err==ESP_OK && memcmp(verify,bytes+at,n)) err=ESP_ERR_INVALID_CRC;
        at+=n;
    }
    return err;
}
static esp_err_t route_span(const uint8_t *bytes,uint32_t offset,uint32_t n,
                            uint32_t source,uint32_t size,uint32_t target) {
    uint32_t lo=offset>source?offset:source,hi=offset+n<source+size?offset+n:source+size;
    if(lo>=hi) return ESP_OK;
    bool erased=true;for(uint32_t at=lo;at<hi;at++) if(bytes[at-offset]!=0xff) {erased=false;break;}
    if(erased) return ESP_OK;
    return write_checked(target+lo-source,bytes+lo-offset,hi-lo);
}
static esp_err_t write_chunk(install_t *s,const uint8_t *bytes,uint32_t offset,uint32_t n) {
    if(!s->image->table_ready) return ESP_OK;
    if(!s->layout_verified) {
        if(s->image->merged && meta_carve_crc32(s->image->table,META_PT_SIZE)!=s->layout_crc) {
            s->failure="source-layout-changed";return ESP_ERR_INVALID_RESPONSE;
        }
        s->layout_verified=true;
    }
    if(s->source_app<0 || s->source_app>=s->image->app_count) return ESP_ERR_INVALID_STATE;
    const z592_source_app_t *a=&s->image->apps[s->source_app];
    const meta_carve_slot_t *slot=&s->proposal->slot[s->slot];
    /* Publish the 24-byte ESP header only after the complete firmware SHA.
     * A power cut during streaming therefore leaves magic=FF, never a bootable
     * partial/unverified app that a subsequent scanner could promote. */
    if(a->bad) {s->failure="source-app-invalid";return ESP_ERR_INVALID_RESPONSE;}
    uint32_t image_limit=a->done?a->image_len:slot->size-META_CARVE_TAIL;
    if(a->header_ready && meta_carve_need(z592_image_minimum_image(a))>slot->size) {
        s->failure="allocation-app-too-large";return ESP_ERR_INVALID_SIZE;
    }
    esp_err_t err=ESP_OK;
    if(a->header_ready && image_limit>=24)
        err=route_span(bytes,offset,n,a->source+24,image_limit-24,slot->offset+24);
    if(err!=ESP_OK) return err;
    if(a->done && !memcmp(a->tail_magic,"MSIG",4)) {
        uint32_t tail=(a->image_len+4095)&~4095u;
        if(tail+4096>slot->size) {s->failure="signature-tail-capacity";return ESP_ERR_INVALID_SIZE;}
        if(!s->signed_tail) {
            err=write_checked(slot->offset+tail,a->tail_magic,4);
            if(err!=ESP_OK) return err;
            s->signed_tail=true;
        }
        err=route_span(bytes,offset,n,a->source+tail,4096,slot->offset+tail);
        if(err!=ESP_OK) return err;
    }
    for(unsigned i=0;i<s->image->data_count;i++) {
        const z592_source_data_t *d=&s->image->data[i];
        int index=meta_carve_find_data(s->proposal,slot->play_id,d->label);
        if(index<0) return ESP_ERR_INVALID_STATE;
        if(d->size!=s->proposal->data[index].size) {s->failure="source-layout-changed";return ESP_ERR_INVALID_SIZE;}
        err=route_span(bytes,offset,n,d->source,d->size,s->proposal->data[index].offset);
        if(err!=ESP_OK) return err;
    }
    return ESP_OK;
}
static esp_err_t download(install_t *s,bool writing) {
    meta_carve_flash_workspace_release();
    bool local=s_flash_source_enabled;
    esp_err_t err=ESP_OK;esp_http_client_handle_t client=NULL;
    if(!local) {
    esp_http_client_config_t cfg={.url=s->request->url,.crt_bundle_attach=esp_crt_bundle_attach,
        .timeout_ms=20000,.buffer_size=1024,.buffer_size_tx=1024,.disable_auto_redirect=true,.addr_type=HTTP_ADDR_TYPE_INET,
        .tls_dyn_buf_strategy=HTTP_TLS_DYN_BUF_RX_STATIC};
    client=esp_http_client_init(&cfg);
    if(!client) {s->failure="https-client-allocation";return ESP_ERR_NO_MEM;}
    err=esp_http_client_open(client,0);
    if(err!=ESP_OK) {
        int tls_code=0,tls_flags=0;
        esp_err_t tls_err=client?esp_http_client_get_and_clear_last_tls_error(client,&tls_code,&tls_flags):ESP_FAIL;
        ESP_LOGE(TAG,"Z592_TLS open=%d transport=%d tls=%d flags=%d heap=%lu largest=%lu stack=%u",
            (int)err,(int)tls_err,tls_code,tls_flags,(unsigned long)esp_get_free_heap_size(),
            (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),(unsigned)uxTaskGetStackHighWaterMark(NULL));
        s->failure="https-open";esp_http_client_cleanup(client);return err;
    }
    ESP_LOGI(TAG,"Z592_HTTP_CONNECTED heap=%lu largest=%lu",(unsigned long)esp_get_free_heap_size(),
        (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    int64_t length=esp_http_client_fetch_headers(client);
    int status=esp_http_client_get_status_code(client);
    if(status!=200 || length!=(int64_t)s->request->file_size) {
        ESP_LOGE(TAG,"HTTP contract status=%d length=%lld expected=%lu",status,(long long)length,(unsigned long)s->request->file_size);
        s->failure="https-status-or-length";esp_http_client_cleanup(client);return ESP_ERR_INVALID_RESPONSE;
    }
    }
    psa_hash_operation_t sha=PSA_HASH_OPERATION_INIT;
    if(psa_crypto_init()!=PSA_SUCCESS || psa_hash_setup(&sha,PSA_ALG_SHA_256)!=PSA_SUCCESS) {psa_hash_abort(&sha);s->failure="sha256-init";if(client) esp_http_client_cleanup(client);return ESP_FAIL;}
    int64_t pass_start=esp_timer_get_time();
    uint32_t pass_cpu_start=task_cpu_us();
    s->read_us=0;s->write_us=0;s->read_cpu_us=0;s->last_report_us=pass_start;
    if(writing) {s->started_us=pass_start;s->info.received_bytes=0;}
    uint32_t received=0;unsigned stalls=0;
    bool layout_done=false;
    while(received<s->request->file_size) {
        if(atomic_load(&s_cancelled)) {err=ESP_ERR_INVALID_STATE;s->failure="cancelled";break;}
        unsigned want=s->request->file_size-received;
        if(want>1024) want=1024;
        int64_t read_start=esp_timer_get_time();errno=0;
        uint32_t cpu_start=task_cpu_us();
        int got=local?(esp_flash_read(NULL,s->buffer,s_flash_source_offset+received,want)==ESP_OK?(int)want:-1):esp_http_client_read(client,(char *)s->buffer,want);
        s->read_cpu_us+=(uint32_t)(task_cpu_us()-cpu_start);
        int64_t read_duration=esp_timer_get_time()-read_start;
        s->read_us+=read_duration;
        if(read_duration>=1000000) ESP_LOGW(TAG,"Z592_READ_WAIT pass=%s offset=%lu blocked_ms=%lu got=%d",
            writing?"full":"layout",(unsigned long)received,(unsigned long)(read_duration/1000),got);
        if(got==-ESP_ERR_HTTP_EAGAIN && stalls++<3) {
            ESP_LOGW(TAG,"Z592_READ_RETRY attempt=%u received=%lu",stalls,(unsigned long)received);
            continue;
        }
        if(got<=0) {
            int read_errno=errno,tls_code=0,tls_flags=0;
            esp_err_t tls_err=client?esp_http_client_get_and_clear_last_tls_error(client,&tls_code,&tls_flags):ESP_FAIL;
            ESP_LOGE(TAG,"Z592_READ_ERROR returned=%d errno=%d transport=%d tls=%d flags=%d heap=%lu largest=%lu received=%lu",
                got,read_errno,(int)tls_err,tls_code,tls_flags,(unsigned long)esp_get_free_heap_size(),
                (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),(unsigned long)received);
            err=got==-ESP_ERR_HTTP_EAGAIN?ESP_ERR_TIMEOUT:ESP_ERR_INVALID_RESPONSE;
            s->failure=got==-ESP_ERR_HTTP_EAGAIN?"https-read-timeout":got==0?"https-read-truncated":"https-read-error";
            s->info.read_result=got;s->info.read_errno=read_errno;s->info.tls_error=tls_code;
            break;
        }
        stalls=0;
        if(psa_hash_update(&sha,s->buffer,(size_t)got)!=PSA_SUCCESS) {err=ESP_FAIL;s->failure="sha256-update";break;}
        if(!z592_image_feed(s->image,s->buffer,(size_t)got)) {err=ESP_ERR_INVALID_RESPONSE;s->failure=s->image->error;}
        if(err==ESP_OK && writing) {
            int64_t write_start=esp_timer_get_time();
            err=write_chunk(s,s->buffer,received,(uint32_t)got);
            s->write_us+=esp_timer_get_time()-write_start;
        }
        if(err!=ESP_OK) {if(!s->failure) s->failure="flash-write-readback";break;}
        received+=(uint32_t)got;
        int64_t now=esp_timer_get_time();
        uint32_t ms=(uint32_t)((now-pass_start)/1000);
        s->info.elapsed_ms=ms;
        s->info.bytes_per_second=ms?(uint32_t)(((uint64_t)received*1000)/ms):0;
        s->info.read_ms=(uint32_t)(s->read_us/1000);s->info.write_ms=(uint32_t)(s->write_us/1000);
        if(writing) s->info.received_bytes=received;else s->info.layout_bytes=received;
        if(!writing) {
            int candidate=z592_image_stream_app(s->image);
            if(candidate==-2) {s->failure="source-app-invalid";err=ESP_ERR_INVALID_RESPONSE;break;}
            if(candidate>=0) {
                const z592_source_app_t *a=&s->image->apps[candidate];
                uint32_t prefix=sizeof(a->prefix);
                if(prefix>a->size) prefix=a->size;
                uint32_t available=a->source<s->image->file_size?s->image->file_size-a->source:0;
                if(prefix>available) prefix=available;
                if(a->header_ready && (z592_image_minimum_image(a)>available || z592_image_minimum_image(a)>a->size)) {
                    s->failure="source-app-invalid";err=ESP_ERR_INVALID_RESPONSE;break;
                }
                if(a->header_ready && !a->bad && s->image->received>=a->source+prefix) {
                    s->source_app=candidate;s->layout_crc=meta_carve_crc32(s->image->table,META_PT_SIZE);
                    s->info.layout_bytes=received;layout_done=true;break;
                }
            }
        }
        unsigned percent=writing?2+(unsigned)(((uint64_t)received*88)/s->request->file_size):0;
        if(percent!=s->last_percent || now-s->last_report_us>=1000000) {
            update_info(s);s->last_percent=percent;s->info.phase=writing?30:10;s->last_report_us=now;
            s->progress(writing?30:10,percent,writing?"writing":"reading-layout",&s->info,s->context);
        }
        if(writing && (received==(uint32_t)got || now-s->started_us>=2000000)) {
            ESP_LOGI(TAG,"Z592_TRANSFER pass=full bytes=%lu total=%lu ms=%lu bps=%lu read_ms=%lu write_ms=%lu read_cpu_ms=%lu task_cpu_ms=%lu cpu_stats=%d",
                (unsigned long)received,(unsigned long)s->request->file_size,(unsigned long)s->info.elapsed_ms,
                (unsigned long)s->info.bytes_per_second,(unsigned long)s->info.read_ms,(unsigned long)s->info.write_ms,
                (unsigned long)(s->read_cpu_us/1000),(unsigned long)((uint32_t)(task_cpu_us()-pass_cpu_start)/1000),
                task_cpu_stats_enabled());
            s->started_us=now;
        }
    }
    uint8_t actual[32];size_t hash_len=0;
    if(psa_hash_finish(&sha,actual,sizeof(actual),&hash_len)!=PSA_SUCCESS || hash_len!=sizeof(actual)) {if(err==ESP_OK) {err=ESP_FAIL;s->failure="sha256-finish";}}
    psa_hash_abort(&sha);
    if(client) {esp_http_client_close(client);esp_http_client_cleanup(client);}
    s->info.elapsed_ms=(uint32_t)((esp_timer_get_time()-pass_start)/1000);
    s->info.bytes_per_second=s->info.elapsed_ms?(uint32_t)(((uint64_t)received*1000)/s->info.elapsed_ms):0;
    s->info.read_ms=(uint32_t)(s->read_us/1000);s->info.write_ms=(uint32_t)(s->write_us/1000);
    ESP_LOGI(TAG,"Z592_TRANSFER_END pass=%s bytes=%lu total=%lu ms=%lu",writing?"full":"layout",
        (unsigned long)received,(unsigned long)s->request->file_size,(unsigned long)((esp_timer_get_time()-pass_start)/1000));
    if(err==ESP_OK && !writing && !layout_done) {err=ESP_ERR_INVALID_RESPONSE;s->failure="source-app-invalid";}
    if(err==ESP_OK && writing && memcmp(actual,s->request->sha256,32)) {err=ESP_ERR_INVALID_CRC;s->failure="package-sha256";}
    return err;
}
static esp_err_t erase_proposal(install_t *s) {
    meta_carve_slot_t *slot=&s->proposal->slot[s->slot];
    if(s->replace>=0) {
        esp_err_t removed=meta_carve_flash_remove_app_and_data(s->replace);
        if(removed!=ESP_OK) return removed;
    }
    /* The header is erased before EMPTY is durable, so a restart cannot scan
     * an orphan old image into this new identity. No existing app is removed. */
    esp_err_t err=esp_flash_erase_region(NULL,slot->offset,4096);
    if(err==ESP_OK) s->app_touched=true;
    if(err==ESP_OK) err=meta_carve_flash_commit(s->proposal,false);
    if(err!=ESP_OK) return err;
    if(slot->size>4096) err=esp_flash_erase_region(NULL,slot->offset+4096,slot->size-4096);
    for(unsigned i=0;err==ESP_OK && i<s->image->data_count;i++) {
        int index=meta_carve_find_data(s->proposal,slot->play_id,s->image->data[i].label);
        if(index<0) return ESP_ERR_INVALID_STATE;
        const meta_carve_data_t *d=&s->proposal->data[index];
        err=esp_flash_erase_region(NULL,d->offset,d->size);
    }
    return err;
}
static esp_err_t finalize(install_t *s) {
    meta_carve_slot_t *slot=&s->proposal->slot[s->slot];
    const z592_source_app_t *a=&s->image->apps[s->image->selected];
    if(atomic_load(&s_cancelled)) {s->failure="cancelled";return ESP_ERR_INVALID_STATE;}
    esp_err_t err=write_checked(slot->offset,a->prefix,24);
    if(err!=ESP_OK) {s->failure="app-header-publish";return err;}
    esp_partition_pos_t pos={.offset=slot->offset,.size=slot->size};
    esp_image_metadata_t *meta=calloc(1,sizeof(*meta));
    if(!meta) return ESP_ERR_NO_MEM;
    err=esp_image_verify(ESP_IMAGE_VERIFY_SILENT,&pos,meta);
    if(err==ESP_OK && meta->image_len!=a->image_len) err=ESP_ERR_INVALID_SIZE;
    free(meta);
    if(err!=ESP_OK) {s->failure="app-image-verification";return err;}
    char name[33]={0};
    if(s->info.community_title) strcpy(name,s->info.name);
    else memcpy(name,a->prefix+80,32);
    if(!name[0] || !meta_name_utf8_ok(name,strlen(name))) strcpy(name,"社区玩法");
    if(s->tail) {
        char existing[33];
        if(!s->info.community_title && meta_name_unpack_tail(s->tail+META_NAME_BLOB_OFF,40,existing,sizeof(existing))) {
            strcpy(name,existing);s->info.project_name=false;
        }
        if(!meta_name_pack_tail(name,s->tail+META_NAME_BLOB_OFF,40)) return ESP_ERR_INVALID_ARG;
        uint32_t at=slot->offset+((a->image_len+4095)&~4095u);
        err=esp_flash_erase_region(NULL,at,4096);
        if(err==ESP_OK) err=write_checked(at,s->tail,4096);
    } else {
        uint8_t blob[40];memset(blob,0xff,sizeof(blob));
        if(!meta_name_pack_tail(name,blob,sizeof(blob))) return ESP_ERR_INVALID_ARG;
        err=write_checked(slot->offset+((a->image_len+4095)&~4095u)+META_NAME_BLOB_OFF,blob,sizeof(blob));
    }
    if(err!=ESP_OK) {s->failure="name-metadata-write";return err;}
    psa_hash_operation_t sha=PSA_HASH_OPERATION_INIT;
    if(psa_crypto_init()!=PSA_SUCCESS || psa_hash_setup(&sha,PSA_ALG_SHA_256)!=PSA_SUCCESS) {psa_hash_abort(&sha);s->failure="sha256-init";return ESP_FAIL;}
    for(uint32_t at=0;at<a->image_len;) {
        uint32_t n=a->image_len-at>1024?1024:a->image_len-at;
        err=esp_flash_read(NULL,s->buffer,slot->offset+at,n);
        if(err!=ESP_OK) break;
        if(psa_hash_update(&sha,s->buffer,n)!=PSA_SUCCESS) {err=ESP_FAIL;s->failure="sha256-readback";break;}at+=n;
    }
    uint8_t digest[32];size_t hash_len=0;
    if(psa_hash_finish(&sha,digest,sizeof(digest),&hash_len)!=PSA_SUCCESS || hash_len!=sizeof(digest)) {if(err==ESP_OK) {err=ESP_FAIL;s->failure="sha256-finish";}}
    psa_hash_abort(&sha);
    if(err!=ESP_OK) return err;
    slot->state=META_SLOT_VALID;slot->image_len=a->image_len;
    memcpy(slot->image_sha256,digest,32);strcpy(slot->name,name);
    strcpy(s->info.name,name);
    /* A/B record and runtime table are committed only after full package hash,
     * Flash readback, application verification and metadata writes succeeded. */
    report(s,50,98,"commit-record");
    if(atomic_load(&s_cancelled)) {s->failure="cancelled";return ESP_ERR_INVALID_STATE;}
    s->commit_started=true;
    err=meta_carve_flash_commit(s->proposal,true);
    if(err!=ESP_OK) s->failure="record-or-table-commit";
    return err;
}
static bool cleanup_span_safe(const meta_carve_t *c,uint32_t id,uint32_t offset,uint32_t size) {
    for(unsigned i=0;i<c->count;i++) {
        const meta_carve_slot_t *a=&c->slot[i];
        if(offset<a->offset+a->size && a->offset<offset+size &&
           (a->play_id!=id || a->offset!=offset || a->size!=size || a->state==META_SLOT_VALID)) return false;
    }
    for(unsigned i=0;i<c->data_count;i++) {
        const meta_carve_data_t *d=&c->data[i];
        if(offset<d->offset+d->size && d->offset<offset+size &&
           (d->play_id!=id || d->offset!=offset || d->size!=size)) return false;
    }
    return true;
}
/* Returns ESP_OK only when an uncertain final commit is proven VALID and its
 * runtime table repaired. Other failures retain their original error/reason. */
static esp_err_t recover_or_cleanup(install_t *s) {
    const meta_carve_slot_t *app=&s->proposal->slot[s->slot];
    esp_err_t err=meta_carve_flash_reload();
    if(err!=ESP_OK) goto pending;
    const meta_carve_t *c=meta_carve_flash_carve();int index=-1;
    for(unsigned i=0;i<c->count;i++) if(c->slot[i].play_id==app->play_id && c->slot[i].offset==app->offset) {index=(int)i;break;}
    if(index>=0 && c->slot[index].state==META_SLOT_VALID) {
        const meta_carve_slot_t *actual=&c->slot[index];
        if(s->commit_started && actual->image_len==app->image_len && !memcmp(actual->image_sha256,app->image_sha256,32)) {
            err=meta_carve_flash_materialize_active(0);
            if(err==ESP_OK) {s->slot=index;return ESP_OK;}
            s->info.cleanup=3;s->info.cleanup_error=err;return err;
        }
        err=ESP_ERR_INVALID_STATE;goto pending;
    }
    /* The durable EMPTY reservation can be larger than the final APP span.
     * Use that full extent when reclaiming a late verification failure. */
    uint32_t app_size=index>=0?c->slot[index].size:app->size;
    if(!cleanup_span_safe(c,app->play_id,app->offset,app_size)) {err=ESP_ERR_INVALID_STATE;goto pending;}
    for(unsigned i=0;i<s->proposal->data_count;i++) {
        const meta_carve_data_t *d=&s->proposal->data[i];
        if(d->play_id==app->play_id && !cleanup_span_safe(c,app->play_id,d->offset,d->size)) {err=ESP_ERR_INVALID_STATE;goto pending;}
    }
    /* Erase before removing the EMPTY record: interrupted cleanup retains a
     * deletable reservation and can never expose an executable orphan. */
    err=esp_flash_erase_region(NULL,app->offset,app_size);
    for(unsigned i=0;err==ESP_OK && i<s->proposal->data_count;i++) {
        const meta_carve_data_t *d=&s->proposal->data[i];
        if(d->play_id==app->play_id) err=esp_flash_erase_region(NULL,d->offset,d->size);
    }
    if(err!=ESP_OK) goto pending;
    if(index>=0) {
        err=meta_carve_flash_remove_app_and_data(index);
        if(err!=ESP_OK) {
            /* Removal may have persisted before its readback/table failed. */
            if(meta_carve_flash_reload()!=ESP_OK) goto pending;
            c=meta_carve_flash_carve();
            for(unsigned i=0;i<c->count;i++) if(c->slot[i].play_id==app->play_id) goto pending;
            for(unsigned i=0;i<c->data_count;i++) if(c->data[i].play_id==app->play_id) goto pending;
            err=meta_carve_flash_materialize_active(0);if(err!=ESP_OK) goto pending;
        }
    }
    s->info.cleanup=1;s->info.cleanup_error=0;
    ESP_LOGI(TAG,"Z592_CLEANUP reclaimed=1 app_bytes=%lu",(unsigned long)app_size);
    return ESP_ERR_NOT_FINISHED; /* cleaned, original installation still failed */
pending:
    s->info.cleanup=2;s->info.cleanup_error=err;
    ESP_LOGE(TAG,"Z592_CLEANUP reclaimed=0 error=%d",(int)err);
    return err;
}
static bool lookup_title(install_t *s) {
    char url[192];
    if(!zion_title_lookup_url(s->request->url,url,sizeof(url))) return false;
    report(s,5,0,"community-metadata");
    meta_carve_flash_workspace_release();
    esp_http_client_config_t cfg={.url=url,.crt_bundle_attach=esp_crt_bundle_attach,
        .timeout_ms=8000,.buffer_size=1024,.buffer_size_tx=1024,.disable_auto_redirect=true,
        .addr_type=HTTP_ADDR_TYPE_INET,.tls_dyn_buf_strategy=HTTP_TLS_DYN_BUF_RX_STATIC};
    esp_http_client_handle_t client=esp_http_client_init(&cfg);
    if(!client) return false;
    zion_title_parser_t *parser=NULL;bool found=false;uint32_t received=0;
    if(esp_http_client_open(client,0)!=ESP_OK) goto done;
    int64_t length=esp_http_client_fetch_headers(client);
    if(esp_http_client_get_status_code(client)!=200 || length>65536) goto done;
    parser=calloc(1,sizeof(*parser));if(!parser) goto done;
    zion_title_init(parser);
    while(received<65536 && !atomic_load(&s_cancelled)) {
        unsigned want=65536-received;if(want>1024) want=1024;
        int got=esp_http_client_read(client,(char *)s->buffer,(int)want);
        if(got<=0) break;
        received+=(uint32_t)got;
        if(!zion_title_feed(parser,(const char *)s->buffer,(size_t)got)) break;
        if(zion_title_ready(parser)) {
            found=zion_title_result(parser,s->request->file_size,s->request->sha256,s->info.name);
            break;
        }
    }
done:
    esp_http_client_close(client);esp_http_client_cleanup(client);free(parser);
    s->info.community_title=found;
    ESP_LOGI(TAG,"Z592_TITLE matched=%d metadata_bytes=%lu",found,(unsigned long)received);
    return found;
}
esp_err_t z592_install_request(const z592_request_t *r,z592_progress_fn cb,void *context,int *installed_slot) {
    int64_t install_start=esp_timer_get_time();
    install_t s={.request=r,.progress=cb,.context=context,.slot=-1,.replace=-1,.source_app=-1};
    s.info.package_bytes=r->file_size;s.info.phase=10;
    *installed_slot=-1;
    s.image=calloc(1,sizeof(*s.image));s.proposal=calloc(1,sizeof(*s.proposal));s.buffer=malloc(1024);
    if(s.image) z592_image_init(s.image,r->file_size,r->source_offset!=0);
    esp_err_t err=ESP_ERR_NO_MEM;
    wifi_ps_type_t previous_ps=WIFI_PS_MIN_MODEM;
    bool restore_ps=esp_wifi_get_ps(&previous_ps)==ESP_OK;
    int override=atomic_load(&s_test_power_save);
    wifi_ps_type_t requested_ps=override==1?WIFI_PS_MIN_MODEM:WIFI_PS_NONE;
    esp_err_t ps_err=esp_wifi_set_ps(requested_ps);
    wifi_ap_record_t ap={0};esp_err_t ap_err=esp_wifi_sta_get_ap_info(&ap);
    ESP_LOGI(TAG,"Z592_RADIO old_ps=%d requested_ps=%d result=%d rssi=%d channel=%u ap_result=%d",
        (int)previous_ps,(int)requested_ps,(int)ps_err,(int)ap.rssi,(unsigned)ap.primary,(int)ap_err);
    if(!s.image || !s.proposal || !s.buffer) {s.failure="preflight-allocation";goto done;}
    (void)lookup_title(&s);
    if(atomic_load(&s_cancelled)) {s.failure="cancelled";err=ESP_ERR_INVALID_STATE;goto done;}
    report(&s,10,0,"preflight");
    err=download(&s,false);if(err!=ESP_OK) goto done;
    if(atomic_load(&s_cancelled)) {s.failure="cancelled";err=ESP_ERR_INVALID_STATE;goto done;}
    uint32_t identity=meta_carve_crc32(r->url,strlen(r->url))|0x80000000u;
    const meta_carve_t *current=meta_carve_flash_carve();
    for(unsigned i=0;i<current->count;i++) if(current->slot[i].play_id==identity) {s.replace=(int)i;break;}
    if(!z592_image_stream_plan(s.image,current,s.replace,identity,s.proposal,&s.slot)) {
        s.failure="allocation-no-space";
        err=ESP_ERR_NO_MEM;goto done;
    }
    update_info(&s);
    snprintf(s.proposal->slot[s.slot].name,sizeof(s.proposal->slot[s.slot].name),"%s",s.info.name);
    report(&s,20,2,"reserve-and-erase");
    if(atomic_load(&s_cancelled)) {s.failure="cancelled";err=ESP_ERR_INVALID_STATE;goto done;}
    err=erase_proposal(&s);if(err!=ESP_OK) {s.failure="reserve-or-erase";goto done;}
    z592_image_init(s.image,r->file_size,r->source_offset!=0);
    err=download(&s,true);if(err!=ESP_OK) goto done;
    if(atomic_load(&s_cancelled)) {s.failure="cancelled";err=ESP_ERR_INVALID_STATE;goto done;}
    if(!z592_image_finish(s.image)) {s.failure=s.image->error;err=ESP_ERR_INVALID_RESPONSE;goto done;}
    if(s.image->selected!=s.source_app) {s.failure="source-app-changed";err=ESP_ERR_INVALID_RESPONSE;goto done;}
    const z592_source_app_t *app=&s.image->apps[s.image->selected];
    uint32_t final_size=meta_carve_need(app->image_len);
    if(final_size>s.proposal->slot[s.slot].size) {s.failure="allocation-app-too-large";err=ESP_ERR_INVALID_SIZE;goto done;}
    s.proposal->slot[s.slot].size=final_size;
    if(!memcmp(app->tail_magic,"MSIG",4)) {
        s.tail=malloc(4096);if(!s.tail) {s.failure="tail-allocation";err=ESP_ERR_NO_MEM;goto done;}
        err=esp_flash_read(NULL,s.tail,s.proposal->slot[s.slot].offset+((app->image_len+4095)&~4095u),4096);
        if(err!=ESP_OK || memcmp(s.tail,"MSIG",4)) {s.failure="signature-tail-readback";err=ESP_ERR_INVALID_RESPONSE;goto done;}
    }
    report(&s,40,95,"verify-installed-image");
    err=finalize(&s);
    if(err==ESP_OK) {*installed_slot=s.slot;report(&s,100,100,"installed");}
done:
    if(err!=ESP_OK && s.app_touched) {
        free(s.tail);s.tail=NULL;free(s.buffer);s.buffer=NULL;
        if(!s.commit_started) (void)esp_flash_erase_region(NULL,s.proposal->slot[s.slot].offset,4096);
        if(recover_or_cleanup(&s)==ESP_OK) {
            err=ESP_OK;*installed_slot=s.slot;report(&s,100,100,"installed");
        }
    }
    if(err!=ESP_OK) {
        const char *detail=s.failure?s.failure:"backend-error";
        ESP_LOGE(TAG,"Z592_INSTALL_FAILED layer=%s code=%s",detail,esp_err_to_name(err));
        update_info(&s);cb(255,s.last_percent,detail,&s.info,context);
    }
    free(s.tail);free(s.buffer);free(s.proposal);free(s.image);
    if(restore_ps) {
        esp_err_t restored=esp_wifi_set_ps(previous_ps);
        ESP_LOGI(TAG,"Z592_RADIO_RESTORE ps=%d result=%d",(int)previous_ps,(int)restored);
    }
    ESP_LOGI(TAG,"Z592_INSTALL_END result=%d ms=%lu layout=%lu full=%lu min_heap=%lu",(int)err,
        (unsigned long)((esp_timer_get_time()-install_start)/1000),
        (unsigned long)s.info.layout_bytes,(unsigned long)s.info.received_bytes,
        (unsigned long)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
    return err;
}
esp_err_t z592_boot_installed(int slot) {
    const meta_carve_t *c=meta_carve_flash_carve();
    if(slot<0 || slot>=c->count || c->slot[slot].state!=META_SLOT_VALID) return ESP_ERR_INVALID_STATE;
    esp_partition_pos_t pos={.offset=c->slot[slot].offset,.size=c->slot[slot].size};
    esp_image_metadata_t *meta=calloc(1,sizeof(*meta));if(!meta) return ESP_ERR_NO_MEM;
    esp_err_t err=esp_image_verify(ESP_IMAGE_VERIFY_SILENT,&pos,meta);free(meta);
    if(err!=ESP_OK) return err;
    err=meta_carve_flash_materialize_active(c->slot[slot].play_id);
    if(err!=ESP_OK) return err;
    const esp_partition_t *p=esp_partition_find_first(ESP_PARTITION_TYPE_APP,
        (esp_partition_subtype_t)(ESP_PARTITION_SUBTYPE_APP_OTA_0+slot),NULL);
    return p?esp_ota_set_boot_partition(p):ESP_ERR_NOT_FOUND;
}

esp_err_t z592_install_flash(uint32_t offset,uint32_t size,const uint8_t sha256[32],z592_progress_fn cb,void *context,int *slot) {
    /* This transport stages an APP image, not a merged partitioned image. */
    z592_request_t request={.source_offset=0,.file_size=size};memcpy(request.sha256,sha256,32);
    memcpy(request.url,"usb-sha256:",11);
    for(unsigned i=0;i<32;i++) snprintf(request.url+11+i*2,3,"%02x",sha256[i]);
    s_flash_source_offset=offset;s_flash_source_enabled=true;z592_install_prepare();
    esp_err_t result=z592_install_request(&request,cb,context,slot);
    s_flash_source_enabled=false;return result;
}
