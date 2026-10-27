/* Executed once under the LVGL lock in an explicitly enabled test build.
 * Compare LVGL's real A8 decoder against every UI glyph's raw 4-bpp pixels. */
#include "zion_fonts.h"
#include "../../assets/fonts/zion_font_ui_inventory.h"
#include <stdio.h>
#include <string.h>
#define GLYPH_EDGE_MAX 32u
static _Alignas(16) uint8_t pixels[GLYPH_EDGE_MAX*GLYPH_EDGE_MAX];
static unsigned grays;
static bool glyph(const lv_font_t *font,uint32_t cp,const lv_font_t *resolved,unsigned bpp) {
    lv_font_glyph_dsc_t g;
    if(!lv_font_get_glyph_dsc(font,&g,cp,0) || g.is_placeholder || g.resolved_font!=resolved) return false;
    const lv_font_fmt_txt_dsc_t *fd=(const lv_font_fmt_txt_dsc_t *)g.resolved_font->dsc;
    if(fd->bpp!=bpp) return false;
    if(!g.box_w || !g.box_h) return true;
    if(g.box_w>GLYPH_EDGE_MAX || g.box_h>GLYPH_EDGE_MAX) return false;
    g.req_raw_bitmap=1;
    const uint8_t *raw=(const uint8_t *)g.resolved_font->get_glyph_bitmap(&g,NULL);
    if(!raw) return false;
    g.req_raw_bitmap=0;
    uint32_t stride=lv_draw_buf_width_to_stride(g.box_w,LV_COLOR_FORMAT_A8);
    if(stride*g.box_h>sizeof(pixels)) return false;
    memset(pixels,0x5a,sizeof(pixels));lv_draw_buf_t buffer;
    if(lv_draw_buf_init(&buffer,g.box_w,g.box_h,LV_COLOR_FORMAT_A8,stride,pixels,sizeof(pixels))!=LV_RESULT_OK) return false;
    if(g.resolved_font->get_glyph_bitmap(&g,&buffer)!=&buffer) return false;
    unsigned mask=(1u<<bpp)-1;
    for(unsigned y=0;y<g.box_h;y++) for(unsigned x=0;x<g.box_w;x++) {
        unsigned bit=g.stride?y*g.stride*8+x*bpp:(y*g.box_w+x)*bpp;
        unsigned expected=fd->bitmap_format?pixels[y*stride+x]:((raw[bit/8]>>(8-bpp-bit%8))&mask)*255/mask;
        if(fd->bitmap_format && expected%17) return false;
        if(pixels[y*stride+x]!=expected) return false;
        if(expected && expected!=255) grays++;
    }
    return true;
}
bool zion_font_selftest(void) {
    grays=0;
    for(unsigned i=0;i<ZION_UI_FONT_COUNT;i++) if(!glyph(ZION_FONT,zion_ui_font_codepoints[i],ZION_FONT,4)) {
        printf("ZION_FONT_TEST fail=ui cp=%lu\n",(unsigned long)zion_ui_font_codepoints[i]);return false;
    }
    for(unsigned cp=32;cp<127;cp++) if(!glyph(ZION_FONT_SMALL,cp,ZION_FONT_SMALL,4)) {
        printf("ZION_FONT_TEST fail=small cp=%u\n",cp);return false;
    }
    if(!glyph(ZION_FONT,ZION_FALLBACK_PROBE,&zion_font_14,4) || !grays) {
        printf("ZION_FONT_TEST fail=fallback-or-grayscale\n");return false;
    }
    printf("ZION_FONT_TEST pass=1 ui=%u small=95 fallback=1 grayscale_pixels=%u\n",ZION_UI_FONT_COUNT,grays);
    fflush(stdout);return true;
}
