/* Exercise the real shared-library dispatcher, including original-wrapper
 * delegation, staged/non-active addresses and calls made by another provider. */
#include "zion_idf_link_hooks.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct OpusEncoder {int token;} OpusEncoder;
static OpusEncoder state={42};
static unsigned real_alloc,app_alloc,real_free,app_free,real_create,app_create,real_destroy,app_destroy;
void *__real_esp_mbedtls_mem_calloc(size_t n,size_t s) {assert(n==2&&s==7);real_alloc++;return &state;}
void __real_esp_mbedtls_mem_free(void *p) {assert(p==&state);real_free++;}
OpusEncoder *__real_opus_encoder_create(int32_t r,int c,int a,int *e) {assert(r==16000&&c==1&&a==2048);real_create++;*e=0;return &state;}
void __real_opus_encoder_destroy(OpusEncoder *p) {assert(p==&state);real_destroy++;}
extern void *__wrap_esp_mbedtls_mem_calloc(size_t,size_t);
extern void __wrap_esp_mbedtls_mem_free(void *);
extern OpusEncoder *__wrap_opus_encoder_create(int32_t,int,int,int *);
extern void __wrap_opus_encoder_destroy(OpusEncoder *);
static void *module_alloc(size_t n,size_t s) {app_alloc++;return ((void *(*)(size_t,size_t))zion_idf_link_real("esp_mbedtls_mem_calloc"))(n,s);}
static void module_free(void *p) {app_free++;((void (*)(void *))zion_idf_link_real("esp_mbedtls_mem_free"))(p);}
static OpusEncoder *module_create(int32_t r,int c,int a,int *e) {app_create++;return ((OpusEncoder *(*)(int32_t,int,int,int *))zion_idf_link_real("opus_encoder_create"))(r,c,a,e);}
static void module_destroy(OpusEncoder *p) {app_destroy++;((void (*)(OpusEncoder *))zion_idf_link_real("opus_encoder_destroy"))(p);}
static void sdk_provider(void) {
    void *p=__wrap_esp_mbedtls_mem_calloc(2,7);__wrap_esp_mbedtls_mem_free(p);
    int error=99;OpusEncoder *opus=__wrap_opus_encoder_create(16000,1,2048,&error);
    assert(error==0&&opus==&state);__wrap_opus_encoder_destroy(opus);
}
int main(void) {
    sdk_provider();assert(real_alloc==1&&!app_alloc);
    zion_idf_stage_link_hook("unreviewed",1);assert(!zion_idf_link_real("unreviewed"));
    zion_idf_stage_link_hook("__wrap_esp_mbedtls_mem_calloc",(uintptr_t)module_alloc);
    zion_idf_stage_link_hook("__wrap_esp_mbedtls_mem_free",(uintptr_t)module_free);
    zion_idf_stage_link_hook("__wrap_opus_encoder_create",(uintptr_t)module_create);
    zion_idf_stage_link_hook("__wrap_opus_encoder_destroy",(uintptr_t)module_destroy);
    sdk_provider();assert(real_alloc==2&&!app_alloc&&!app_free&&!app_create&&!app_destroy);
    /* A cache-commit failure never activates its staged pointers. Starting
     * another load discards them, including when that module has no wrappers. */
    zion_idf_begin_link_hooks();zion_idf_activate_link_hooks();sdk_provider();
    assert(real_alloc==3&&!app_alloc&&!app_free&&!app_create&&!app_destroy);
    zion_idf_stage_link_hook("__wrap_esp_mbedtls_mem_calloc",(uintptr_t)module_alloc);
    zion_idf_stage_link_hook("__wrap_esp_mbedtls_mem_free",(uintptr_t)module_free);
    zion_idf_stage_link_hook("__wrap_opus_encoder_create",(uintptr_t)module_create);
    zion_idf_stage_link_hook("__wrap_opus_encoder_destroy",(uintptr_t)module_destroy);
    zion_idf_activate_link_hooks();sdk_provider();
    assert(real_alloc==4&&real_free==4&&real_create==4&&real_destroy==4);
    assert(app_alloc==1&&app_free==1&&app_create==1&&app_destroy==1);
    puts("link hooks: PASS (SDK-internal dispatch, post-commit activation, original delegation without recursion)");
}
