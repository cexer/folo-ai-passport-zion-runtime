#include "zion_elf.h"
#include "zion_sdk.h"
#include "zion_runtime.h"
#include "zion_idf_compat.h"
#include "zion_idf_link_hooks.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_memory_utils.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_partition.h"
#include "esp_cache.h"
#include "bsp_display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>
extern long long __divdi3(long long,long long);
extern unsigned long long __udivdi3(unsigned long long,unsigned long long);
extern unsigned long long __umoddi3(unsigned long long,unsigned long long);
#if CONFIG_ESP_SYSTEM_MEMPROT
#error "Native ELF profile requires CONFIG_ESP_SYSTEM_MEMPROT=n; use a fresh runtime build directory"
#endif

typedef struct code_stage {struct code_stage *next;size_t offset,size;} code_stage_t;
static struct {
    uint32_t offset,capacity;
    size_t used;
    const void *entry;
    code_stage_t *head,*tail;
    esp_partition_mmap_handle_t mapping;
    const char *error;
    bool erased;
    unsigned sections;
    size_t peak_stage;
} executable_cache;
static struct {
    void *entry;
    size_t offset,size;
    esp_partition_mmap_handle_t mapping;
    bool flash;
} readonly_cache;
static code_stage_t *reserved_stage;
static size_t reserved_stage_bytes;
static void *allocate_readonly(size_t n) {
    readonly_cache.offset=(executable_cache.used+15u)&~15u;
    readonly_cache.size=n;
    if(executable_cache.capacity && readonly_cache.offset<=executable_cache.capacity &&
       n<=executable_cache.capacity-readonly_cache.offset) {
        esp_partition_t region={.flash_chip=esp_flash_default_chip,.type=ESP_PARTITION_TYPE_DATA,
            .address=executable_cache.offset,.size=executable_cache.capacity,.readonly=true};
        const void *mapped=NULL;
        if(esp_partition_mmap(&region,readonly_cache.offset,n,ESP_PARTITION_MMAP_DATA,&mapped,&readonly_cache.mapping)!=ESP_OK)
            return NULL;
        readonly_cache.entry=(void *)mapped;readonly_cache.flash=true;
        executable_cache.used=readonly_cache.offset+n;
        ESP_LOGI("zion-elf","READONLY_FLASH bytes=%u ram_saved=%u",(unsigned)n,(unsigned)n);
    } else {
        /* Older packages reserve code only; preserve their loading contract. */
        readonly_cache.entry=heap_caps_malloc(n,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
        ESP_LOGI("zion-elf","READONLY_RAM bytes=%u reason=legacy-cache-capacity",(unsigned)n);
    }
    return readonly_cache.entry;
}
static void *allocate(size_t n,bool executable) {
    if(executable && executable_cache.capacity) {
        if(n>executable_cache.capacity) {executable_cache.error="elf:code-cache-size";return NULL;}
        esp_partition_t region={.flash_chip=esp_flash_default_chip,.type=ESP_PARTITION_TYPE_DATA,
            .address=executable_cache.offset,.size=executable_cache.capacity,.erase_size=4096};
        if(esp_partition_mmap(&region,0,n,ESP_PARTITION_MMAP_INST,&executable_cache.entry,&executable_cache.mapping)!=ESP_OK) {
            executable_cache.error="elf:code-cache-mmap";return NULL;
        }
        if(!esp_ptr_executable(executable_cache.entry)) {
            executable_cache.error="elf:code-cache-not-executable";
            esp_partition_munmap(executable_cache.mapping);executable_cache.entry=NULL;
            return NULL;
        }
        executable_cache.used=n;return (void *)executable_cache.entry;
    }
    uint32_t caps=MALLOC_CAP_INTERNAL|(executable?MALLOC_CAP_EXEC:MALLOC_CAP_8BIT);
    void *result=heap_caps_malloc(n,caps);
    if(!result) ESP_LOGE("zion-elf","ALLOC_FAIL executable=%d bytes=%u free=%u largest=%u",executable,(unsigned)n,
        (unsigned)heap_caps_get_free_size(caps),(unsigned)heap_caps_get_largest_free_block(caps));
    return result;
}
static void *writable(void *memory,bool executable) {
    if(executable && esp_ptr_in_diram_iram(memory)) return esp_ptr_diram_iram_to_dram(memory);
    return memory;
}
static void free_stages(void) {
    while(executable_cache.head) {
        code_stage_t *next=executable_cache.head->next;
        if(executable_cache.head!=reserved_stage) heap_caps_free(executable_cache.head);
        executable_cache.head=next;
    }
    executable_cache.tail=NULL;
}
static bool prepare_staging(size_t code_max,size_t readonly_max) {
    if(!executable_cache.capacity) return true;
    size_t maximum=code_max;
    if(readonly_cache.flash && readonly_max>maximum) maximum=readonly_max;
    if(!maximum || maximum>SIZE_MAX-sizeof(code_stage_t)) return false;
    reserved_stage=heap_caps_malloc(sizeof(code_stage_t)+maximum,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    if(!reserved_stage) return false;
    reserved_stage_bytes=maximum;
    ESP_LOGI("zion-elf","STAGING_RESERVED bytes=%u",(unsigned)maximum);
    return true;
}
static void release_staging(void) {
    free_stages();heap_caps_free(reserved_stage);reserved_stage=NULL;reserved_stage_bytes=0;
}
static void *section_writable(void *execution,size_t offset,size_t length) {
    if(execution==readonly_cache.entry) {
        if(offset>readonly_cache.size || length>readonly_cache.size-offset) return NULL;
        if(!readonly_cache.flash) return (uint8_t *)execution+offset;
        execution=(void *)executable_cache.entry;offset+=readonly_cache.offset;
    }
    if(!executable_cache.capacity) return (uint8_t *)writable(execution,true)+offset;
    if(execution!=executable_cache.entry || offset>executable_cache.used || length>executable_cache.used-offset ||
       length>SIZE_MAX-sizeof(code_stage_t)) return NULL;
    if(reserved_stage && (length>reserved_stage_bytes || executable_cache.head)) return NULL;
    code_stage_t *stage=reserved_stage?reserved_stage:heap_caps_malloc(sizeof(*stage)+length,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    if(!stage) {ESP_LOGE("zion-elf","STAGE_ALLOC_FAIL bytes=%u",(unsigned)length);return NULL;}
    *stage=(code_stage_t){.offset=offset,.size=length};
    if(executable_cache.tail) executable_cache.tail->next=stage;else executable_cache.head=stage;
    executable_cache.tail=stage;return stage+1;
}
static bool section_complete(void *execution,size_t offset,size_t length,void *staging) {
    if(execution==readonly_cache.entry) {
        if(offset>readonly_cache.size || length>readonly_cache.size-offset) return false;
        if(!readonly_cache.flash) return staging==(uint8_t *)execution+offset;
        execution=(void *)executable_cache.entry;offset+=readonly_cache.offset;
    }
    if(!executable_cache.capacity) return true;
    code_stage_t *stage=executable_cache.head;
    if(execution!=executable_cache.entry || !stage || stage!=executable_cache.tail ||
       staging!=stage+1 || stage->offset!=offset || stage->size!=length) return false;
    if(!executable_cache.erased) {
        if(esp_flash_erase_region(NULL,executable_cache.offset,executable_cache.capacity)!=ESP_OK) {
            executable_cache.error="elf:code-cache-erase";return false;
        }
        executable_cache.erased=true;
    }
    executable_cache.error="elf:code-cache-write";
    if(esp_flash_write(NULL,staging,executable_cache.offset+offset,length)!=ESP_OK) return false;
    uint8_t check[256];
    for(size_t at=0;at<length;at+=sizeof(check)) {
        size_t n=length-at;if(n>sizeof(check)) n=sizeof(check);
        if(esp_flash_read(NULL,check,executable_cache.offset+offset+at,n)!=ESP_OK ||
           memcmp(check,(uint8_t *)staging+at,n)) return false;
    }
    if(length>executable_cache.peak_stage) executable_cache.peak_stage=length;
    executable_cache.sections++;if(stage!=reserved_stage) heap_caps_free(stage);
    executable_cache.head=executable_cache.tail=NULL;executable_cache.error=NULL;
    /* This worker may write hundreds of small sections while LVGL is locked. */
    vTaskDelay(1);return true;
}
static void release(void *memory) {
    if(memory && memory==readonly_cache.entry) {
        if(readonly_cache.flash) esp_partition_munmap(readonly_cache.mapping);
        else heap_caps_free(memory);
        memset(&readonly_cache,0,sizeof(readonly_cache));
    } else if(memory && memory==executable_cache.entry) {
        esp_partition_munmap(executable_cache.mapping);executable_cache.entry=NULL;free_stages();
    } else heap_caps_free(memory);
}
static bool commit_executable(void) {
    if(!executable_cache.entry) return true;
    if(executable_cache.head || !executable_cache.erased || !executable_cache.sections) return false;
    esp_err_t sync=esp_cache_msync((void *)executable_cache.entry,executable_cache.capacity,
        ESP_CACHE_MSYNC_FLAG_DIR_M2C|ESP_CACHE_MSYNC_FLAG_TYPE_INST);
    if(sync!=ESP_OK && sync!=ESP_ERR_NOT_SUPPORTED) return false;
    free_stages();
    ESP_LOGI("zion-elf","EXEC_FLASH bytes=%u sections=%u offset=0x%lx peak_stage=%u ram_stage_released=1",
        (unsigned)executable_cache.used,executable_cache.sections,(unsigned long)executable_cache.offset,
        (unsigned)executable_cache.peak_stage);
    return true;
}
static uintptr_t resolve(const char *name) {
    if(!strcmp(name,"memcpy")) return (uintptr_t)memcpy;
    if(!strcmp(name,"memset")) return (uintptr_t)memset;
    if(!strcmp(name,"memcmp")) return (uintptr_t)memcmp;
    if(!strcmp(name,"strlen")) return (uintptr_t)strlen;
    if(!strcmp(name,"__divdi3")) return (uintptr_t)__divdi3;
    if(!strcmp(name,"__udivdi3")) return (uintptr_t)__udivdi3;
    if(!strcmp(name,"__umoddi3")) return (uintptr_t)__umoddi3;
    return zion_idf_resolve(name);
}
static void loader_cooperate(void) {
    static int64_t last;
    int64_t now=esp_timer_get_time();
    if(now-last>=100000) {vTaskDelay(1);last=esp_timer_get_time();}
}
static const zion_elf_ops_t ops={allocate,writable,release,resolve,section_writable,section_complete,allocate_readonly,zion_idf_stage_link_hook,loader_cooperate,prepare_staging,release_staging};
static zion_elf_t loaded; /* One module, intentionally retained until power off. */
void zion_idf_run_initializers(void) {
#if ZION_RUNTIME_TEST_KEYS
    /* App-owned USB readers may have tiny stacks. Start the test command
     * worker here, on the loader stack, before constructors/app tasks run. */
    extern bool zion_test_usb_control_init(void);
    if(!zion_test_usb_control_init()) ESP_LOGE("zion-test","USB control worker unavailable");
#endif
    /* Generated source-compatible entries own ordinary LVGL screens, whereas
     * explicit ZION API apps keep the host's text/frame frontend. */
    extern void zion_runtime_view_surrender(void);
    zion_runtime_view_surrender();
    for(unsigned i=0;i<loaded.constructor_count;i++) {
        uintptr_t entry=loaded.constructors[i];
        if(entry && entry!=UINT32_MAX) ((void (*)(void))entry)();
    }
    ESP_LOGI("zion-elf","INITIALIZERS count=%u",loaded.constructor_count);
}
void zion_elf_installed_entry(const zion_api_t *api) {
    zion_package_t p;
    if(!zion_runtime_selected_package(&p) || !zion_package_verify(&p)) {
        api->text_set(ZION_TEXT_STATUS,"插件包校验失败");return;
    }
    memset(&executable_cache,0,sizeof(executable_cache));
    memset(&readonly_cache,0,sizeof(readonly_cache));
    executable_cache.capacity=p.code_cache_size;
    uint32_t required_extent=0;
    if(!zion_package_extent(&p,p.extent,&required_extent)) return;
    executable_cache.offset=p.flash_offset+required_extent-p.code_cache_size;
    /* Read the verified ELF directly through Flash MMU mapping. A temporary
     * file-sized RAM copy makes metadata-heavy modules fail before relocation. */
    const void *file=NULL;esp_partition_mmap_handle_t mapping;
    const esp_partition_t storage={.flash_chip=esp_flash_default_chip,.type=ESP_PARTITION_TYPE_DATA,
        .address=p.flash_offset,.size=p.extent,.readonly=true};
    if(esp_partition_mmap(&storage,p.elf_offset,p.elf_size,ESP_PARTITION_MMAP_DATA,&file,&mapping)!=ESP_OK) {
        api->text_set(ZION_TEXT_STATUS,"ELF 加载失败");ESP_LOGE("zion-elf","LOAD_FAIL stage=flash-mmap");return;
    }
    /* Failed temporary allocations must be released before the renderer tries
     * to allocate draw tasks. App entry runs only after this lock is released. */
    if(!bsp_lvgl_lock(2000)) {
        esp_partition_munmap(mapping);api->text_set(ZION_TEXT_STATUS,"ELF 加载失败");
        ESP_LOGE("zion-elf","LOAD_FAIL stage=ui-lock");return;
    }
    zion_idf_begin_link_hooks();
    int result=zion_elf_load_mapped(file,p.elf_size,&ops,&loaded);
    if(result && executable_cache.error) loaded.error=executable_cache.error;
    if(!result && !commit_executable()) {
        result=-1;loaded.error="elf:code-cache-write";zion_elf_release(&ops,&loaded);
    }
    if(result || !loaded.mapped_bytes) esp_partition_munmap(mapping);
    bsp_lvgl_unlock();
    if(result) {
        ESP_LOGE("zion-elf","LOAD_FAIL stage=%s relocation=%u",loaded.error?loaded.error:"flash-read",loaded.relocation_type);
        bool memory_failure=loaded.error && (!strcmp(loaded.error,"elf:out-of-memory") || !strcmp(loaded.error,"elf:section-memory"));
        api->text_set(ZION_TEXT_STATUS,memory_failure?"内存不足":"ELF 加载失败");
        api->text_set(ZION_TEXT_BODY,loaded.error?loaded.error:"flash-read");return;
    }
    ESP_LOGI("zion-elf","LOADED source=external-flash id=%lu bytes=%lu code=%u data=%u mapped=%u entry=%p data_sections=%u map_pages=%u",
        (unsigned long)p.id,(unsigned long)p.elf_size,(unsigned)loaded.code_bytes,(unsigned)loaded.data_bytes,(unsigned)loaded.mapped_bytes,(void *)loaded.entry,loaded.data_block_count,loaded.mapping_page_count);
    __asm__ volatile("fence.i" ::: "memory");
    zion_idf_activate_link_hooks();
    ((void (*)(const zion_api_t *))loaded.entry)(api);
}
uint32_t zion_runtime_asset_size(uint32_t asset) {
    zion_package_t p;return asset==1 && zion_runtime_selected_package(&p)?p.asset_size:0;
}
int32_t zion_runtime_asset_read(uint32_t asset,uint32_t offset,void *out,uint32_t count) {
    zion_package_t p;
    if(asset!=1 || !out || !zion_runtime_selected_package(&p) || offset>p.asset_size || count>p.asset_size-offset) return ZION_E_ARGUMENT;
    return esp_flash_read(NULL,out,p.flash_offset+p.asset_offset+offset,count)==ESP_OK?0:ZION_E_IO;
}
