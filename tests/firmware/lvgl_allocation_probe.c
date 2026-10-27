/* Native-test allocator telemetry only; never changes allocation semantics. */
#include "lvgl.h"
#include <stdio.h>
extern void *__real_lv_malloc_core(size_t);
extern void *__real_lv_realloc_core(void *,size_t);
static void failed(size_t size) {
    lv_mem_monitor_t memory;lv_mem_monitor(&memory);
    printf("ZION_LVGL_ALLOC_FAIL bytes=%u free=%u largest=%u max_used=%u total=%u\n",
        (unsigned)size,(unsigned)memory.free_size,(unsigned)memory.free_biggest_size,
        (unsigned)memory.max_used,(unsigned)memory.total_size);fflush(stdout);
}
void *__wrap_lv_malloc_core(size_t size) {
    void *result=__real_lv_malloc_core(size);if(!result && size) failed(size);return result;
}
void *__wrap_lv_realloc_core(void *memory,size_t size) {
    void *result=__real_lv_realloc_core(memory,size);if(!result && size) failed(size);return result;
}
