/* Actual LVGL formatter/decompressor, with a host memory/cache port. */
#include "src/core/lv_global.h"
#include "src/font/fmt_txt/lv_font_fmt_txt.h"
#include "src/draw/lv_draw_buf.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
LV_FONT_DECLARE(zion_font_14);
lv_global_t lv_global;
static size_t largest_request;
void *lv_malloc(size_t size) {if(size>largest_request) largest_request=size;return malloc(size);}
void *lv_malloc_zeroed(size_t size) {return calloc(1,size);}
void *lv_memcpy(void *destination,const void *source,size_t size) {return memcpy(destination,source,size);}
void lv_free(void *memory) {free(memory);}
void *lv_utils_bsearch(const void *key,const void *base,size_t n,size_t size,
                     int (*compare)(const void *,const void *)) {return bsearch(key,base,n,size,compare);}
uint32_t lv_draw_buf_width_to_stride(uint32_t width,lv_color_format_t format) {
    assert(format==LV_COLOR_FORMAT_A8);return (width+3u)&~3u;
}
void lv_draw_buf_flush_cache(const lv_draw_buf_t *buffer,const lv_area_t *area) {(void)buffer;(void)area;}
int main(void) {
    const lv_font_fmt_txt_dsc_t *font=zion_font_14.dsc;
    assert(font->bpp==4);
    unsigned count=0,grays=0;unsigned long long hash=14695981039346656037ull;
    for(uint32_t cp=32;cp<65536;cp++) {
        lv_font_glyph_dsc_t g={0};
        if(!zion_font_14.get_glyph_dsc(&zion_font_14,&g,cp,0)) continue;
        count++;g.resolved_font=&zion_font_14;
        assert(g.box_w<=32 && g.box_h<=32);
        if(!g.box_w || !g.box_h) continue;
        unsigned char pixels[32*32+16];memset(pixels,0x5a,sizeof(pixels));
        unsigned stride=lv_draw_buf_width_to_stride(g.box_w,LV_COLOR_FORMAT_A8);
        lv_draw_buf_t buffer={0};buffer.data=pixels;
        assert(zion_font_14.get_glyph_bitmap(&g,&buffer)==&buffer);
        for(unsigned i=stride*g.box_h;i<sizeof(pixels);i++) assert(pixels[i]==0x5a);
        for(unsigned y=0;y<g.box_h;y++) for(unsigned x=0;x<g.box_w;x++) {
            unsigned v=pixels[y*stride+x];assert(v%17==0);
            grays+=v!=0 && v!=255;hash=(hash^v)*1099511628211ull;
        }
    }
    /* Same checksum as an independently generated uncompressed 4bpp font,
     * decoded with this LVGL engine: compression preserves every A8 pixel. */
    assert(count==6873 && grays>0 && largest_request<=32);
    assert(hash==0xa2d630c0a7cfc038ull);
    printf("PASS actual LVGL font decoder: glyphs=%u gray_pixels=%u checksum=%llx max_allocation=%zu\n",
           count,grays,hash,largest_request);
    return 0;
}
