/* Real installer and carve persistence, mocked HTTP/NOR hardware. TLS trust and
 * IDF load-address policy are device checks, not claimed by this host fixture. */
#include "zion592_install.h"
#include "zion_manage.h"
#include "zion_prefs.h"
#include "zion592_image.h"
#include "meta_name.h"
#include "meta_carve_flash.h"
#include "esp_http_client.h"
#include "esp_image_format.h"
#include "mbedtls/sha256.h"
#include "freertos/task.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t flash[0x800000],source[0x21000];
static unsigned source_size=336,source_app;
static int64_t clock_us;
int64_t esp_timer_get_time(void) {clock_us+=1000;return clock_us;}
#define HTTP_REQUEST_MAX 128
static unsigned bytes_per_request[HTTP_REQUEST_MAX];
static unsigned requests,erase_calls,write_calls;
static unsigned read_quantum=37;
static bool truncate_second,mutate_second,corrupt_readback,bad_image,torn_commit;
static unsigned cancel_stage;
static bool open_failed,transient_timeout,permanent_timeout;
static bool prefs_fault,metadata_fault,erase_fault;
static unsigned final_fault,record_read_failures,cleanup_fault;
static bool final_attempted,table_fault;
static uint32_t erase_fault_at;
static int workspace_fail_at=-1,workspace_allocs;
static size_t workspace_budget=(size_t)-1,workspace_spent;
void *meta_workspace_test_calloc(size_t count,size_t size) {
    if(workspace_allocs++==workspace_fail_at) return NULL;
    size_t bytes=count*size;
    if(bytes>workspace_budget-workspace_spent) return NULL;
    workspace_spent+=bytes;
    return calloc(count,size);
}
static unsigned ready_count,error_count;
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t task) {(void)task;return 0;}
static bool protected_range(uint32_t at,uint32_t n) {
    return at<0x8000 || (at<0x10000+META_FACTORY_SIZE && at+n>0x10000) || (at<0x35a000 && at+n>0x356000);
}
esp_err_t esp_flash_read(void *chip,void *out,uint32_t at,uint32_t n) {
    (void)chip;if(at>sizeof(flash) || n>sizeof(flash)-at) return ESP_ERR_INVALID_ARG;
    if(record_read_failures && at>=META_STORE_OFFSET && at<META_STORE_OFFSET+8192) {
        if(record_read_failures!=999) record_read_failures--;
        return ESP_FAIL;
    }
    memcpy(out,flash+at,n);
    if(corrupt_readback && at>=META_POOL0_START && at<META_POOL0_END && n) ((uint8_t *)out)[0]^=1;
    return ESP_OK;
}
esp_err_t esp_flash_write(void *chip,const void *in,uint32_t at,uint32_t n) {
    (void)chip;assert(!protected_range(at,n));assert(at<=sizeof(flash) && n<=sizeof(flash)-at);
    if(table_fault && at==0x8000) return ESP_FAIL;
    if((prefs_fault && at>=META_STORE_OFFSET+3*4096 && at<META_STORE_OFFSET+4*4096) ||
       (metadata_fault && (at==META_STORE_OFFSET || at==META_STORE_OFFSET+4096))) return ESP_FAIL;
    const uint8_t *b=in;
    uint32_t written=(torn_commit && at==META_STORE_OFFSET+4096 && n>128)?128:n;
    for(uint32_t i=0;i<written;i++) flash[at+i]&=b[i];
    write_calls++;
    if(final_attempted && final_fault && at>=META_STORE_OFFSET && at<META_STORE_OFFSET+8192 && n>128) {
        unsigned mode=final_fault;final_fault=0;
        if(mode==1) return ESP_FAIL; /* Entire record written, driver reports failure. */
        if(mode==2) record_read_failures=1;
        if(mode==3) record_read_failures=999;
        if(mode==4) table_fault=true;
    }
    return written==n?ESP_OK:ESP_FAIL;
}
esp_err_t esp_flash_erase_region(void *chip,uint32_t at,uint32_t n) {
    (void)chip;assert(!protected_range(at,n));assert(!(at%4096) && !(n%4096));
    assert(at<=sizeof(flash) && n<=sizeof(flash)-at);if(erase_fault && at==erase_fault_at) return ESP_FAIL;memset(flash+at,0xff,n);erase_calls++;return ESP_OK;
}
struct host_http_client {uint32_t at;unsigned request,timeouts;};
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *cfg) {
    assert(cfg->crt_bundle_attach && cfg->disable_auto_redirect && cfg->timeout_ms>0);
    assert(cfg->tls_dyn_buf_strategy==HTTP_TLS_DYN_BUF_RX_STATIC);
    assert(!strncmp(cfg->url,"https://",8));
    struct host_http_client *c=calloc(1,sizeof(*c));assert(c);c->request=++requests;assert(c->request<HTTP_REQUEST_MAX);return c;
}
esp_err_t esp_http_client_open(esp_http_client_handle_t c,int n) {(void)c;assert(n==0);return open_failed?ESP_ERR_INVALID_STATE:ESP_OK;}
esp_err_t esp_http_client_get_and_clear_last_tls_error(esp_http_client_handle_t c,int *code,int *flags) {(void)c;*code=0;*flags=0;return ESP_OK;}
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t c) {(void)c;return source_size;}
int esp_http_client_get_status_code(esp_http_client_handle_t c) {(void)c;return 200;}
int esp_http_client_read(esp_http_client_handle_t c,char *out,int capacity) {
    if(c->at>=168 && (permanent_timeout || (transient_timeout && !c->timeouts))) {c->timeouts++;return -ESP_ERR_HTTP_EAGAIN;}
    if(c->at==source_size || (truncate_second && c->request==2 && c->at>=168)) return 0;
    unsigned n=source_size-c->at;if(n>read_quantum) n=read_quantum;if(n>(unsigned)capacity) n=(unsigned)capacity;
    memcpy(out,source+c->at,n);
    if(mutate_second && c->request==2 && c->at<=100 && c->at+n>100) out[100-c->at]^=1;
    c->at+=n;bytes_per_request[c->request]+=n;
    if(c->request==2 && c->at==source_size && cleanup_fault) {
        if(cleanup_fault==1) metadata_fault=true;
        else {erase_fault=true;erase_fault_at=META_POOL0_START;}
    }
    return (int)n;
}
esp_err_t esp_http_client_close(esp_http_client_handle_t c) {(void)c;return ESP_OK;}
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t c) {free(c);return ESP_OK;}
esp_err_t esp_crt_bundle_attach(void *config) {(void)config;return ESP_OK;}
esp_err_t esp_image_verify(esp_image_load_mode_t mode,const esp_partition_pos_t *part,esp_image_metadata_t *out) {
    assert(mode==ESP_IMAGE_VERIFY_SILENT);
    if(bad_image || part->offset+336>sizeof(flash) || memcmp(flash+part->offset,source+source_app,336)) return ESP_ERR_IMAGE_INVALID;
    memset(out,0,sizeof(*out));out->image_len=336;return ESP_OK;
}
static zion_install_info_t last_info;
static unsigned last_percent;
static char last_reason[64];
static void progress(unsigned stage,unsigned percent,const char *message,const zion_install_info_t *info,void *ctx) {
    last_info=*info;last_percent=percent;snprintf(last_reason,sizeof(last_reason),"%s",message);
    (void)message;(void)ctx;assert(percent<=100);if(stage==100) ready_count++;if(stage==255) error_count++;
    if(stage==50) final_attempted=true;
    if(stage==40) {
        const meta_carve_t *carve=meta_carve_flash_carve();
        unsigned empty=0;
        for(unsigned i=0;i<carve->count;i++) if(carve->slot[i].state==META_SLOT_EMPTY) {
            assert(flash[carve->slot[i].offset]==0xff);empty++;
        }
        assert(empty>0);
    }
    if(cancel_stage && stage==cancel_stage) z592_install_cancel();
}
static z592_request_t make_request(void) {
    z592_install_prepare();
    z592_request_t r={.type=2,.file_size=source_size};strcpy(r.url,"https://example.invalid/firmware.bin");
    mbedtls_sha256_context sha;mbedtls_sha256_init(&sha);mbedtls_sha256_starts(&sha,0);
    mbedtls_sha256_update(&sha,source,source_size);mbedtls_sha256_finish(&sha,r.sha256);mbedtls_sha256_free(&sha);return r;
}
static void reset(void) {
    memset(flash,0xff,sizeof(flash));meta_pt_safe(flash+0x8000);
    memset(flash+0x10000,0x6d,META_FACTORY_SIZE);memset(flash+0x356000,0x91,0x4000);
    meta_carve_flash_test_reset();assert(meta_carve_flash_ensure()==ESP_OK);
    requests=erase_calls=write_calls=ready_count=error_count=0;
    clock_us=0;memset(bytes_per_request,0,sizeof(bytes_per_request));
    read_quantum=37;
    final_fault=record_read_failures=cleanup_fault=0;final_attempted=table_fault=false;
    prefs_fault=metadata_fault=erase_fault=false;
    open_failed=transient_timeout=permanent_timeout=false;truncate_second=mutate_second=corrupt_readback=bad_image=torn_commit=false;cancel_stage=0;
}
static void assert_protected(void) {
    for(unsigned i=0x10000;i<0x10000+META_FACTORY_SIZE;i++) assert(flash[i]==0x6d);
    for(unsigned i=0x356000;i<0x35a000;i++) assert(flash[i]==0x91);
}
int main(void) {
    memset(source,0,sizeof(source));source[0]=0xe9;source[1]=1;source[12]=5;source[23]=1;source[29]=1;
    memcpy(source+80,"unit-app",8);
    reset();z592_request_t r=make_request();int slot=-1;
    assert(z592_install_request(&r,progress,NULL,&slot)==ESP_OK);
    assert(requests==2 && ready_count==1 && !error_count && slot==0);
    assert(bytes_per_request[1]<source_size && bytes_per_request[2]==source_size);
    assert(last_info.received_bytes==source_size && last_info.layout_bytes==bytes_per_request[1]);
    assert(!strcmp(last_info.name,"unit-app") && last_info.install_known);
    assert(last_info.package_bytes==source_size && last_info.install_bytes==meta_carve_need(336));
    assert(last_percent==100 && !strcmp(last_reason,"installed"));
    const meta_carve_t *c=meta_carve_flash_carve();assert(c->count==1 && c->slot[0].state==META_SLOT_VALID);
    assert(!memcmp(flash+c->slot[0].offset,source,source_size) && !strcmp(c->slot[0].name,"unit-app"));
    meta_carve_flash_test_reset();assert(meta_carve_flash_ensure()==ESP_OK);
    assert(meta_carve_flash_carve()->slot[0].state==META_SLOT_VALID);assert_protected();
    puts("PASS installer: bounded layout probe + exactly one full transfer, real SHA, NOR readback, durable VALID record");
    meta_carve_flash_workspace_release();workspace_allocs=0;workspace_spent=0;
    assert(meta_carve_flash_commit(meta_carve_flash_carve(),false)==ESP_OK);
    int cold_allocations=workspace_allocs;
    for(int i=0;i<cold_allocations;i++) {
        meta_carve_flash_workspace_release();workspace_fail_at=i;workspace_allocs=0;
        unsigned old_writes=write_calls,old_erases=erase_calls;
        assert(meta_carve_flash_commit(meta_carve_flash_carve(),false)==ESP_ERR_NO_MEM);
        assert(write_calls==old_writes && erase_calls==old_erases);
        workspace_fail_at=-1;
        assert(meta_carve_flash_commit(meta_carve_flash_carve(),false)==ESP_OK);
        assert(meta_carve_flash_carve()->slot[0].state==META_SLOT_VALID);
    }
    puts("PASS workspace: every allocation failure leaves Flash unchanged; release/reacquire retains canonical state");
    /* Reproduce the board's ~26 KiB free heap while reserving 8 KiB for live UI.
     * This is a test allocator budget, never a package or production quota. */
    meta_carve_flash_workspace_release();workspace_allocs=0;workspace_spent=0;workspace_budget=18000;
    assert(meta_carve_flash_commit(meta_carve_flash_carve(),true)==ESP_OK);
    assert(workspace_spent<=workspace_budget);
    meta_carve_flash_workspace_release();workspace_budget=(size_t)-1;workspace_spent=0;
    puts("PASS commit under memory pressure: durable record and table with UI reserve");
    reset();r=make_request();r.sha256[0]^=1;
    assert(z592_install_request(&r,progress,NULL,&slot)!=ESP_OK);
    assert(requests==2 && erase_calls && write_calls && !ready_count && error_count==1);assert_protected();
    assert(meta_carve_flash_carve()->count==0 && last_info.cleanup==1);
    puts("PASS full-transfer hash rejection: header never published and no VALID promotion");
    assert(!strcmp(last_info.name,"unit-app") && last_info.package_bytes==source_size);
    assert(last_info.install_known && !strcmp(last_reason,"package-sha256"));
    for(unsigned mode=0;mode<4;mode++) {
        reset();r=make_request();truncate_second=mode==0;mutate_second=mode==1;corrupt_readback=mode==2;bad_image=mode==3;
        assert(z592_install_request(&r,progress,NULL,&slot)!=ESP_OK && slot==-1);
        assert(!ready_count && error_count==1);
        assert(last_info.install_known && last_info.install_bytes==meta_carve_need(336));
        assert(last_info.package_bytes==source_size && !strcmp(last_info.name,"unit-app"));
        assert(last_info.phase>=20 && last_percent<100);
        c=meta_carve_flash_carve();assert(c->count==0 && last_info.cleanup==1);
        corrupt_readback=false;meta_carve_flash_test_reset();assert(meta_carve_flash_ensure()==ESP_OK);
        assert(meta_carve_flash_carve()->count==0);assert_protected();
    }
    puts("PASS failure paths: truncated/mutated full transfer, Flash readback fault, authority image rejection; no VALID promotion");
    reset();r=make_request();cancel_stage=10;
    assert(z592_install_request(&r,progress,NULL,&slot)!=ESP_OK && !erase_calls && !write_calls && !ready_count);
    puts("PASS cancellation before Flash mutation");
    reset();r=make_request();open_failed=true;
    assert(z592_install_request(&r,progress,NULL,&slot)!=ESP_OK);
    assert(!erase_calls && !write_calls && !ready_count && error_count==1);
    assert(meta_carve_flash_carve()->count==0);assert_protected();
    puts("PASS HTTPS connection failure: no Flash mutation or false READY");
    assert(last_info.package_bytes==source_size && !last_info.name[0] && !last_info.install_known);
    assert(!strcmp(last_reason,"https-open"));
    reset();r=make_request();transient_timeout=true;
    assert(z592_install_request(&r,progress,NULL,&slot)==ESP_OK && ready_count==1 && requests==2);
    puts("PASS transient HTTP EAGAIN: retries preserve full hash and installed bytes");
    reset();r=make_request();permanent_timeout=true;
    assert(z592_install_request(&r,progress,NULL,&slot)==ESP_ERR_TIMEOUT);
    assert(!ready_count && error_count==1 && meta_carve_flash_carve()->count==0 && last_info.cleanup==1);
    puts("PASS persistent HTTP EAGAIN: bounded retries, explicit timeout, no VALID promotion");
    reset();r=make_request();torn_commit=true;
    assert(z592_install_request(&r,progress,NULL,&slot)!=ESP_OK && !ready_count);
    meta_carve_flash_test_reset();assert(meta_carve_flash_ensure()==ESP_OK);
    assert(meta_carve_flash_carve()->slot[0].state==META_SLOT_EMPTY);assert_protected();
    puts("PASS torn final A/B record: restart retains EMPTY, never a successful install");
    for(unsigned mode=1;mode<=4;mode++) {
        reset();r=make_request();final_fault=mode;
        esp_err_t outcome=z592_install_request(&r,progress,NULL,&slot);
        if(mode<=2) assert(outcome==ESP_OK && ready_count==1 && error_count==0 && slot==0);
        else assert(outcome!=ESP_OK && !ready_count && error_count==1 && last_info.cleanup==(mode==3?2u:3u));
        assert(flash[META_POOL0_START]==0xe9); /* Uncertain VALID is never erased. */
        record_read_failures=0;table_fault=false;
        meta_carve_flash_test_reset();assert(meta_carve_flash_ensure()==ESP_OK);
        assert(meta_carve_flash_carve()->count==1 && meta_carve_flash_carve()->slot[0].state==META_SLOT_VALID);assert_protected();
    }
    puts("PASS uncertain commit: full write/error and transient readback recover; unreadable A/B or persistent table failure retain VALID bytes");
    for(unsigned mode=1;mode<=2;mode++) {
        reset();r=make_request();r.sha256[0]^=1;cleanup_fault=mode;
        assert(z592_install_request(&r,progress,NULL,&slot)!=ESP_OK && !ready_count);
        assert(last_info.cleanup==2 && !strcmp(last_reason,"package-sha256"));
        assert(meta_carve_flash_carve()->count==1);
        metadata_fault=erase_fault=false;cleanup_fault=0;
        zion_prefs_t empty={0};bool pending=false;
        assert(zion_delete(&empty,0,&pending)==ESP_OK && !pending && meta_carve_flash_carve()->count==0);
    }
    puts("PASS failed cleanup: original failure retained, explicit pending status, later management deletion reclaims reservation");
    for(unsigned stage=30;stage<=50;stage+=10) {
        reset();r=make_request();cancel_stage=stage;
        assert(z592_install_request(&r,progress,NULL,&slot)!=ESP_OK && !ready_count);
        assert(last_info.cleanup==1 && meta_carve_flash_carve()->count==0 && !strcmp(last_reason,"cancelled"));
    }
    puts("PASS cancellation during write, verification and precommit: complete reservation reclaimed");
    uint8_t app_bytes[336];memcpy(app_bytes,source,sizeof(app_bytes));
    reset();memset(source,0xff,sizeof(source));source_size=sizeof(source);source_app=0x10000;
    meta_pt_t table={.count=3};
    table.e[0]=(meta_pt_entry_t){.type=1,.subtype=2,.offset=0x9000,.size=0x6000,.label="nvs"};
    table.e[1]=(meta_pt_entry_t){.type=0,.subtype=0,.offset=0x10000,.size=0x10000,.label="factory"};
    table.e[2]=(meta_pt_entry_t){.type=1,.subtype=0x82,.offset=0x20000,.size=0x1000,.label="assets"};
    meta_pt_encode(&table,source+0x8000);memcpy(source+source_app,app_bytes,sizeof(app_bytes));
    memset(source+0x9000,0x31,0x6000);memset(source+0x20000,0x73,17);
    r=make_request();r.source_offset=0x10000;
    assert(z592_install_request(&r,progress,NULL,&slot)==ESP_OK);
    c=meta_carve_flash_carve();assert(c->count==1 && c->data_count==1);
    assert(last_info.install_known && last_info.install_bytes==meta_carve_need(336)+0x1000);
    const meta_carve_data_t data=c->data[0];
    assert(!strcmp(data.label,"assets") && data.play_id==c->slot[slot].play_id);
    assert(!memcmp(flash+data.offset,source+0x20000,17));
    for(unsigned i=17;i<data.size;i++) assert(flash[data.offset+i]==0xff);
    for(unsigned i=0x9000;i<0xf000;i++) assert(flash[i]==0xff);
    assert(meta_carve_flash_materialize_active(data.play_id)==ESP_OK);
    meta_pt_t runtime;assert(meta_pt_decode(flash+0x8000,&runtime));bool found=false;
    for(unsigned i=0;i<runtime.count;i++) if(!strcmp(runtime.e[i].label,"assets")) {
        assert(runtime.e[i].offset==data.offset && runtime.e[i].size==data.size);found=true;
    }
    assert(found);assert_protected();
    puts("PASS merged installer: relocates APP and owned DATA, preserves shared NVS, materializes child resource partition");
    /* Too-large resources preserve the parsed size/name; no destination write. */
    table.e[2].size=0x700000;meta_pt_encode(&table,source+0x8000);
    r=make_request();r.source_offset=0x10000;
    unsigned saved_writes=write_calls,saved_erases=erase_calls,saved_requests=requests;
    assert(z592_install_request(&r,progress,NULL,&slot)!=ESP_OK);
    assert(write_calls==saved_writes && erase_calls==saved_erases && requests==saved_requests+1);
    assert(last_info.install_known && last_info.install_bytes==meta_carve_need(336)+0x700000);
    assert(!strcmp(last_info.name,"unit-app") && !strcmp(last_reason,"allocation-no-space"));
    table.e[2].size=0x1000;table.e[2].subtype=0x40;meta_pt_encode(&table,source+0x8000);
    r=make_request();r.source_offset=0x10000;
    assert(z592_install_request(&r,progress,NULL,&slot)!=ESP_OK);
    assert(!last_info.install_known && last_info.package_bytes==source_size);
    assert(!strcmp(last_reason,"unsupported-child-data"));
    assert(write_calls==saved_writes && erase_calls==saved_erases);
    table.e[2].subtype=0x82;meta_pt_encode(&table,source+0x8000);
    r=make_request();r.source_offset=0x10000;
    puts("PASS install info: sizes, capacity/format/network/hash/write failures retain facts; rejected packages never mutate Flash");
    unsigned old_count=c->count;
    assert(z592_install_request(&r,progress,NULL,&slot)==ESP_OK);
    c=meta_carve_flash_carve();assert(c->count==old_count && c->data_count==1);
    assert(!memcmp(flash+c->data[0].offset,source+0x20000,17));
    puts("PASS same-URL reinstall: replaces exactly its own APP and DATA without duplicate records");
    strcpy(r.url,"https://example.invalid/second.bin");
    assert(z592_install_request(&r,progress,NULL,&slot)==ESP_OK);
    c=meta_carve_flash_carve();assert(c->count==2 && c->data_count==2);
    assert(c->data[0].play_id!=c->data[1].play_id && c->data[0].offset!=c->data[1].offset);
    assert_protected();
    puts("PASS multiple installs: distinct URL identities coexist with separate owned resource extents");
    zion_prefs_t prefs={0},loaded;bool cleanup=false;
    assert(zion_default_update(&prefs,0)==ESP_OK);
    assert(zion_prefs_load(&loaded)==ESP_OK && zion_prefs_valid(&loaded));
    assert(zion_default_find(c,&loaded)==0);
    meta_carve_t changed=*c;changed.slot[0].image_sha256[0]^=1;assert(zion_default_find(&changed,&loaded)==-1);
    prefs_fault=true;
    assert(zion_delete(&prefs,0,&cleanup)!=ESP_OK && !cleanup && meta_carve_flash_carve()->count==2);
    prefs_fault=false;metadata_fault=true;
    uint32_t old_offset=c->slot[0].offset;
    assert(zion_delete(&prefs,0,&cleanup)!=ESP_OK && !cleanup && flash[old_offset]==0xe9);
    metadata_fault=false;assert(meta_carve_flash_carve()->count==2);
    const meta_carve_slot_t deleted=c->slot[0],kept=c->slot[1];
    const meta_carve_data_t resource=c->data[0];uint32_t before_free=meta_carve_free(c);
    assert(zion_delete(&prefs,0,&cleanup)==ESP_OK && !cleanup);
    c=meta_carve_flash_carve();assert(c->count==1 && c->data_count==1 && c->slot[0].offset==kept.offset);
    assert(meta_carve_free(c)==before_free+deleted.size+resource.size);
    for(uint32_t i=0;i<deleted.size;i++) assert(flash[deleted.offset+i]==0xff);
    for(uint32_t i=0;i<resource.size;i++) assert(flash[resource.offset+i]==0xff);
    assert(flash[kept.offset]==0xe9 && flash[c->data[0].offset]==0x73 && !(prefs.flags&ZION_PREFS_ENABLED));
    assert(zion_prefs_load(&loaded)==ESP_OK && !(loaded.flags&ZION_PREFS_ENABLED));
    puts("PASS delete: persistent default cleared, owned APP/DATA erased, unrelated play preserved, exact capacity reclaimed; precommit faults never erase APP");
    erase_fault=true;erase_fault_at=kept.offset;
    assert(zion_delete(&prefs,0,&cleanup)!=ESP_OK && cleanup);
    assert(meta_carve_flash_carve()->count==0 && meta_carve_flash_carve()->data_count==0);
    erase_fault=false;meta_carve_flash_test_reset();assert(meta_carve_flash_ensure()==ESP_OK);
    assert(meta_carve_flash_carve()->count==0);assert_protected();
    puts("PASS postcommit erase failure: removal stays durable after restart, explicit cleanup-pending, no orphan resurrection");
    reset();r=make_request();r.source_offset=0x10000;
    assert(z592_install_request(&r,progress,NULL,&slot)==ESP_OK);
    c=meta_carve_flash_carve();uint32_t baseline_free=meta_carve_free(c);
    const meta_carve_slot_t survivor=c->slot[0];const meta_carve_data_t survivor_data=c->data[0];
    strcpy(r.url,"https://example.invalid/failure-with-assets.bin");r.sha256[0]^=1;
    assert(z592_install_request(&r,progress,NULL,&slot)!=ESP_OK && last_info.cleanup==1);
    c=meta_carve_flash_carve();assert(c->count==1 && c->data_count==1 && meta_carve_free(c)==baseline_free);
    assert(!memcmp(flash+survivor.offset,source+source_app,336) && flash[survivor_data.offset]==0x73);
    meta_carve_flash_test_reset();assert(meta_carve_flash_ensure()==ESP_OK);
    assert(meta_carve_flash_carve()->count==1 && meta_carve_flash_carve()->data_count==1);assert_protected();
    puts("PASS APP plus resources failure: all new capacity reclaimed across reboot; unrelated APP and resource bytes unchanged");
    reset();source_app=0;source_size=8192;memset(source,0xff,sizeof(source));memcpy(source,app_bytes,sizeof(app_bytes));
    memset(source+4096,0x5a,4096);memcpy(source+4096,"MSIG",4);read_quantum=3;
    r=make_request();assert(z592_install_request(&r,progress,NULL,&slot)==ESP_OK && requests==2);
    c=meta_carve_flash_carve();uint32_t tail_at=c->slot[slot].offset+4096;
    assert(!memcmp(flash+tail_at,source+4096,4096-40));
    char tail_name[33];assert(meta_name_unpack_tail(flash+tail_at+META_NAME_BLOB_OFF,40,tail_name,sizeof(tail_name)));
    assert(!strcmp(tail_name,"unit-app") && bytes_per_request[1]<source_size && bytes_per_request[2]==source_size);assert_protected();
    puts("PASS single-full-transfer MSIG: split magic, signature tail preserved, NOR-safe name update");
    reset();source_app=0x20000;source_size=0x21000;memset(source,0xff,sizeof(source));
    table.e[1]=(meta_pt_entry_t){.type=0,.subtype=0,.offset=0x20000,.size=0x10000,.label="factory"};
    table.e[2]=(meta_pt_entry_t){.type=1,.subtype=0x82,.offset=0x10000,.size=0x1000,.label="assets"};
    meta_pt_encode(&table,source+0x8000);memcpy(source+source_app,app_bytes,sizeof(app_bytes));memset(source+0x10000,0x6b,17);
    r=make_request();r.source_offset=0x10000;assert(z592_install_request(&r,progress,NULL,&slot)==ESP_OK);
    c=meta_carve_flash_carve();assert(c->data_count==1 && !memcmp(flash+c->data[0].offset,source+0x10000,4096));
    assert(bytes_per_request[1]<source_size && bytes_per_request[2]==source_size);assert_protected();
    puts("PASS data before APP source: prefix probe never loses resource bytes; full transfer routes them once");
}
