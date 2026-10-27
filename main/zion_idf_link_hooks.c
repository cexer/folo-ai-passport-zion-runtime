/* Passport OS's unchanged Muse and Opus wrappers must intercept calls made
 * inside shared SDK libraries too. A module-only ld --wrap cannot do that.
 * Keep each reviewed signature typed; never patch instructions or SDK objects.
 * __real imports bypass dispatch so an original wrapper can delegate safely. */
#include "zion_idf_link_hooks.h"
#include <stddef.h>
#include <stdatomic.h>
#include <string.h>

typedef struct OpusEncoder OpusEncoder;
typedef void *(*calloc_hook_t)(size_t,size_t);
typedef void (*free_hook_t)(void *);
typedef OpusEncoder *(*opus_create_hook_t)(int32_t,int,int,int *);
typedef void (*opus_destroy_hook_t)(OpusEncoder *);
extern void *__real_esp_mbedtls_mem_calloc(size_t,size_t);
extern void __real_esp_mbedtls_mem_free(void *);
extern OpusEncoder *__real_opus_encoder_create(int32_t,int,int,int *);
extern void __real_opus_encoder_destroy(OpusEncoder *);

static uintptr_t pending[4];
static _Atomic(calloc_hook_t) active_calloc;
static _Atomic(free_hook_t) active_free;
static _Atomic(opus_create_hook_t) active_create;
static _Atomic(opus_destroy_hook_t) active_destroy;
void zion_idf_begin_link_hooks(void) {memset(pending,0,sizeof(pending));}
void zion_idf_stage_link_hook(const char *name,uintptr_t address) {
    static const char *const names[]={"__wrap_esp_mbedtls_mem_calloc",
        "__wrap_esp_mbedtls_mem_free","__wrap_opus_encoder_create","__wrap_opus_encoder_destroy"};
    for(unsigned i=0;i<4;i++) if(!strcmp(name,names[i])) pending[i]=address;
}
void zion_idf_activate_link_hooks(void) {
    atomic_store(&active_calloc,(calloc_hook_t)pending[0]);
    atomic_store(&active_free,(free_hook_t)pending[1]);
    atomic_store(&active_create,(opus_create_hook_t)pending[2]);
    atomic_store(&active_destroy,(opus_destroy_hook_t)pending[3]);
}
void *__wrap_esp_mbedtls_mem_calloc(size_t count,size_t size) {
    calloc_hook_t fn=atomic_load(&active_calloc);
    return fn?fn(count,size):__real_esp_mbedtls_mem_calloc(count,size);
}
void __wrap_esp_mbedtls_mem_free(void *ptr) {
    free_hook_t fn=atomic_load(&active_free);
    if(fn) fn(ptr);else __real_esp_mbedtls_mem_free(ptr);
}
OpusEncoder *__wrap_opus_encoder_create(int32_t rate,int channels,int application,int *error) {
    opus_create_hook_t fn=atomic_load(&active_create);
    return fn?fn(rate,channels,application,error):__real_opus_encoder_create(rate,channels,application,error);
}
void __wrap_opus_encoder_destroy(OpusEncoder *ptr) {
    opus_destroy_hook_t fn=atomic_load(&active_destroy);
    if(fn) fn(ptr);else __real_opus_encoder_destroy(ptr);
}
uintptr_t zion_idf_link_real(const char *name) {
    if(!strcmp(name,"esp_mbedtls_mem_calloc")) return (uintptr_t)&__real_esp_mbedtls_mem_calloc;
    if(!strcmp(name,"esp_mbedtls_mem_free")) return (uintptr_t)&__real_esp_mbedtls_mem_free;
    if(!strcmp(name,"opus_encoder_create")) return (uintptr_t)&__real_opus_encoder_create;
    if(!strcmp(name,"opus_encoder_destroy")) return (uintptr_t)&__real_opus_encoder_destroy;
    return 0;
}
