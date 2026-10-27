#include "zion_sdk.h"
#include "zion_fonts.h"
#include "display/lvgl_display/dynamic_glyph_cache.h"
#include "display/lvgl_display/lvgl_font.h"
extern "C" {
#include "bsp_display.h"
#include "bsp_battery.h"
#include "zion_service.h"
}
#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <cstring>
LV_FONT_DECLARE(font_noto_sans_basic_14_1);
static lv_obj_t *view,*labels[4],*battery;
static LvglBuiltInFont font(ZION_FONT);
static DynamicGlyphCache glyph_cache;
static TaskHandle_t frame_owner;
enum {FRAME_TILES=80,FRAME_ROW_BYTES=240*2,FRAME_BYTES=FRAME_ROW_BYTES*320};
static lv_obj_t *frame_canvas;
struct FrameStorage {
    lv_image_dsc_t descriptors[FRAME_TILES];
    uint8_t *pixels[FRAME_TILES];
    uint16_t rows[FRAME_TILES];
};
/* MEM-04 (Octo Radio): native LVGL apps never use the explicit SDK frame
 * surface. Lazy storage saves 2716 static bytes on C3; do not drop frame
 * precision or tile capacity. See docs/runtime-optimization-log.md. */
static FrameStorage *frames;
static unsigned frame_count;
static void frame_draw(lv_event_t *event) {
    lv_layer_t *layer=lv_event_get_layer(event);lv_area_t area;lv_obj_get_coords(frame_canvas,&area);
    int y=area.y1;
    for(unsigned i=0;i<frame_count;i++) {
        lv_draw_image_dsc_t draw;lv_draw_image_dsc_init(&draw);draw.src=&frames->descriptors[i];
        lv_area_t stripe={area.x1,y,area.x1+239,y+frames->rows[i]-1};
        lv_draw_image(layer,&draw,&stripe);y+=frames->rows[i];
    }
}
extern "C" bool zion_runtime_view_create(const char *name) {
    if(!bsp_lvgl_lock(2000)) return false;
    /* Only the frontend is disposed. BSP display, LVGL task and buses survive. */
    zion_launcher_suspend();
    view=lv_obj_create(nullptr);
    lv_obj_set_style_bg_opa(view,LV_OPA_COVER,0);
    lv_obj_set_style_bg_color(view,lv_color_hex(0x101810),0);
    lv_obj_set_style_text_color(view,lv_color_hex(0xE0F0C0),0);
    lv_obj_set_style_text_font(view,font.font(),0);
    const int ys[]={10,42,78,290};
    for(unsigned i=0;i<4;i++) {
        labels[i]=lv_label_create(view);lv_obj_set_pos(labels[i],10,ys[i]);
        lv_obj_set_width(labels[i],220);lv_label_set_text(labels[i],"");
    }
    lv_obj_set_width(labels[0],160);lv_obj_set_height(labels[2],204);
    lv_label_set_long_mode(labels[2],LV_LABEL_LONG_WRAP);
    lv_label_set_text(labels[0],name);
    lv_label_set_text(labels[3],"短按OK对话 上下调节音量");
    battery=lv_label_create(view);lv_obj_set_pos(battery,185,10);lv_obj_set_width(battery,45);
    lv_obj_set_style_text_align(battery,LV_TEXT_ALIGN_RIGHT,0);
    char text[12];int soc=bsp_battery_soc();
    if(soc>=0) snprintf(text,sizeof(text),"%d%%",soc);else snprintf(text,sizeof(text),"--%%");
    lv_label_set_text(battery,text);
    lv_obj_t *old=lv_screen_active();lv_screen_load(view);if(old!=view) lv_obj_delete(old);
    bsp_lvgl_unlock();return true;
}
extern "C" int32_t zion_runtime_text_set(uint32_t field,const char *text) {
    if(field>=4 || !text) return ZION_E_ARGUMENT;
    if(!bsp_lvgl_lock(1000)) return ZION_E_STATE;
    if(!view) {bsp_lvgl_unlock();return ZION_E_STATE;}
    lv_label_set_text(labels[field],text);bsp_lvgl_unlock();return 0;
}
extern "C" void zion_runtime_view_surrender(void) {
    if(!view || !bsp_lvgl_lock(1000)) return;
    /* Keep the default screen itself valid: original apps may retain and
     * delete that handle after loading their own screen. Only our children go. */
    for(unsigned i=0;i<frame_count;i++) {heap_caps_free(frames->pixels[i]);frames->pixels[i]=nullptr;}
    frame_count=0;frame_canvas=nullptr;frame_owner=nullptr;
    heap_caps_free(frames);frames=nullptr;
    lv_obj_clean(view);view=nullptr;battery=nullptr;
    for(auto &label:labels) label=nullptr;
    bsp_lvgl_unlock();
}
extern "C" int32_t zion_runtime_glyph_add(const zion_glyph_t *input,uint32_t count,uint32_t bpp) {
    if(bpp!=1 || !input || count>256) return ZION_E_ARGUMENT;
    std::vector<TextGlyph> glyphs;
    for(uint32_t i=0;i<count;i++) {
        const auto &g=input[i];
        if(!g.bitmap || g.width>128 || g.height>128 || g.bitmap_size!=(g.width*g.height+7u)/8u) return ZION_E_ARGUMENT;
        TextGlyph out;out.codepoint=g.codepoint;out.adv_w=g.advance;
        out.box_w=g.width;out.box_h=g.height;out.ofs_x=g.offset_x;out.ofs_y=g.offset_y;
        out.bitmap.assign(g.bitmap,g.bitmap+g.bitmap_size);glyphs.push_back(std::move(out));
    }
    if(!bsp_lvgl_lock(1000)) return ZION_E_STATE;
    auto fallback=glyph_cache.EnsureFont(ZION_FONT,1);
    bool ok=fallback && glyph_cache.AddGlyphs(glyphs);
    if(ok) {font.SetFallback(fallback);if(view) lv_obj_invalidate(view);}
    bsp_lvgl_unlock();return ok?0:ZION_E_IO;
}
extern "C" void zion_runtime_glyph_clear(void) {
    if(!bsp_lvgl_lock(1000)) return;
    font.SetFallback(nullptr);glyph_cache.Clear();if(view) lv_obj_invalidate(view);bsp_lvgl_unlock();
}
extern "C" int32_t zion_runtime_frame_begin(void) {
    if(frame_owner==xTaskGetCurrentTaskHandle()) return ZION_E_STATE;
    if(!bsp_lvgl_lock(1000)) return ZION_E_STATE;
    if(!view) {bsp_lvgl_unlock();return ZION_E_STATE;}
    if(!frames) frames=(FrameStorage *)heap_caps_calloc(1,sizeof(*frames),MALLOC_CAP_8BIT|MALLOC_CAP_INTERNAL);
    if(!frames) {bsp_lvgl_unlock();return ZION_E_IO;}
    if(!frames->pixels[0]) {
        unsigned remaining=320;frame_count=0;
        while(remaining && frame_count<FRAME_TILES) {
            unsigned i=frame_count,rows=remaining<16?remaining:16;
            for(;;) {
                frames->pixels[i]=(uint8_t *)heap_caps_calloc(1,rows*FRAME_ROW_BYTES,MALLOC_CAP_8BIT|MALLOC_CAP_INTERNAL);
                if(frames->pixels[i] || rows==1) break;
                rows=(rows+1)/2;
            }
            if(!frames->pixels[i]) break;
            frames->rows[i]=(uint16_t)rows;remaining-=rows;frame_count++;
        }
        if(remaining) {
                ESP_LOGE("zion-frame","ALLOC_FAIL tiles=%u remaining_rows=%u free=%u largest=%u",frame_count,remaining,
                    (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT|MALLOC_CAP_INTERNAL),
                    (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT|MALLOC_CAP_INTERNAL));
                for(unsigned j=0;j<frame_count;j++) {heap_caps_free(frames->pixels[j]);frames->pixels[j]=nullptr;}
                frame_count=0;
                bsp_lvgl_unlock();return ZION_E_IO;
        }
        ESP_LOGI("zion-frame","SURFACE_READY bytes=%u tiles=%u free=%u",FRAME_BYTES,frame_count,
            (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT|MALLOC_CAP_INTERNAL));
    }
    frame_owner=xTaskGetCurrentTaskHandle();return 0;
}
extern "C" void zion_runtime_frame_cancel(void) {
    if(frame_owner!=xTaskGetCurrentTaskHandle()) return;
    frame_owner=nullptr;bsp_lvgl_unlock();
}
extern "C" int32_t zion_runtime_frame_write(uint32_t offset,const void *input,uint32_t count) {
    if(frame_owner!=xTaskGetCurrentTaskHandle()) return ZION_E_STATE;
    if(!input || offset>FRAME_BYTES || count>FRAME_BYTES-offset) return ZION_E_ARGUMENT;
    const uint8_t *bytes=(const uint8_t *)input;
    unsigned tile=0;uint32_t within=offset;
    while(tile<frame_count && within>=frames->rows[tile]*FRAME_ROW_BYTES) {within-=frames->rows[tile]*FRAME_ROW_BYTES;tile++;}
    while(count) {
        if(tile>=frame_count) return ZION_E_STATE;
        uint32_t length=frames->rows[tile]*FRAME_ROW_BYTES-within;if(length>count) length=count;
        memcpy(frames->pixels[tile]+within,bytes,length);bytes+=length;count-=length;tile++;within=0;
    }
    return 0;
}
extern "C" int32_t zion_runtime_frame_present(void) {
    if(frame_owner!=xTaskGetCurrentTaskHandle()) return ZION_E_STATE;
    if(!view) {zion_runtime_frame_cancel();return ZION_E_STATE;}
    if(!frame_canvas) {
        lv_obj_set_style_pad_all(view,0,0);lv_obj_set_style_border_width(view,0,0);
        frame_canvas=lv_obj_create(view);lv_obj_remove_style_all(frame_canvas);lv_obj_set_size(frame_canvas,240,320);lv_obj_set_pos(frame_canvas,0,0);
        lv_obj_add_event_cb(frame_canvas,frame_draw,LV_EVENT_DRAW_MAIN,nullptr);
    }
    for(unsigned i=0;i<frame_count;i++) {
        auto &descriptor=frames->descriptors[i];
        descriptor.header.magic=LV_IMAGE_HEADER_MAGIC;descriptor.header.cf=LV_COLOR_FORMAT_RGB565;
        descriptor.header.w=240;descriptor.header.h=frames->rows[i];descriptor.header.stride=480;
        descriptor.data_size=frames->rows[i]*FRAME_ROW_BYTES;descriptor.data=frames->pixels[i];
    }
    lv_obj_invalidate(frame_canvas);
    zion_runtime_frame_cancel();return 0;
}
