#pragma once
/* ZION ABI 1. No ESP-IDF, LVGL, C++ objects or allocator ownership crosses it.
 * Strings are UTF-8, copied during calls. Buffers remain caller-owned.
 * Blocking calls (PCM, event wait, storage) are worker-task-only.
 * One application per boot. There is deliberately no unload API. */
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define ZION_ABI_VERSION 1u
#define ZION_CAP_TEXT (1u << 0)
#define ZION_CAP_KEYS (1u << 1)
#define ZION_CAP_STORAGE (1u << 2)
#define ZION_CAP_NETWORK_STATE (1u << 3)
#define ZION_CAP_PCM (1u << 4)
enum { ZION_SUCCESS=0, ZION_E_ARGUMENT=-1, ZION_E_ABI=-2,
       ZION_E_CAPABILITY=-3, ZION_E_STATE=-4, ZION_E_IO=-5 };
enum { ZION_TEXT_TITLE, ZION_TEXT_STATUS, ZION_TEXT_BODY, ZION_TEXT_FOOTER };
enum { ZION_EVENT_KEY=1, ZION_EVENT_NETWORK=2 };
enum { ZION_KEY_UP, ZION_KEY_DOWN, ZION_KEY_OK };
enum { ZION_KEY_PRESS, ZION_KEY_CLICK, ZION_KEY_DOUBLE, ZION_KEY_LONG };
typedef struct { uint32_t type, code, value; } zion_event_t;
typedef struct {
    uint32_t codepoint, advance;
    uint16_t width,height;
    int16_t offset_x,offset_y;
    const uint8_t *bitmap;
    uint32_t bitmap_size;
} zion_glyph_t;
typedef struct {
    uint32_t abi_version, struct_size, capabilities;
    uint64_t (*uptime_ms)(void);
    int32_t (*text_set)(uint32_t field,const char *text);
    int32_t (*event_wait)(zion_event_t *event,uint32_t timeout_ms);
    int32_t (*network_online)(void);
    int32_t (*battery_percent)(void);
    int32_t (*setting_get)(const char *key,int32_t *value);
    int32_t (*setting_set)(const char *key,int32_t value);
    int32_t (*pcm_open)(uint32_t rate);
    int32_t (*pcm_read)(int16_t *samples,uint32_t count);
    int32_t (*pcm_write)(const int16_t *samples,uint32_t count);
    void (*volume_set)(uint32_t percent);
    void (*backlight_set)(uint32_t percent);
    int32_t (*glyph_add)(const zion_glyph_t *glyphs,uint32_t count,uint32_t bpp);
    void (*glyph_clear)(void);
    void (*log)(const char *message);
    uint32_t (*asset_size)(uint32_t asset);
    int32_t (*asset_read)(uint32_t asset,uint32_t offset,void *out,uint32_t size);
    /* Paired calls on the plugin worker only. RGB565 little-endian surface is
     * host-owned in tiles, avoiding a 150 KiB contiguous allocation on C3. */
    int32_t (*frame_begin)(void);
    int32_t (*frame_write)(uint32_t offset,const void *bytes,uint32_t count);
    int32_t (*frame_present)(void);
    void (*frame_cancel)(void);
} zion_api_t;
typedef struct {
    uint32_t abi_version, struct_size, required_api_size, required_capabilities;
    const char *id;
    const char *name;
    void (*entry)(const zion_api_t *api);
} zion_plugin_t;
#ifdef __cplusplus
}
#endif
