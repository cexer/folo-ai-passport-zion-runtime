#pragma once

#include "lvgl.h"
#include "zion_fonts.h"

#define UI_SKY        0x101912
#define UI_SKY_DARK   0x81966B
#define UI_INK        0xD6E2B6
#define UI_INK2       0x81966B
#define UI_PAPER      0x18251B
#define UI_GRASS      0x101912
#define UI_GRASS_DARK 0x243326
#define UI_YELLOW     0xBBD980
#define UI_ORANGE     0xFFB23E
#define UI_RED        0xE43B2F
#define UI_MUTED      0xD9E7EC

lv_obj_t *ui_pixel_screen_create(const char *title);
lv_obj_t *ui_pixel_panel_create(lv_obj_t *parent, int x, int y, int w, int h,
                                uint32_t color);
lv_obj_t *ui_pixel_panel_create_shadowed(lv_obj_t *parent, int x, int y, int w, int h,
                                         uint32_t color, lv_obj_t **shadow_out);
lv_obj_t *ui_pixel_panel_create_plain(lv_obj_t *parent, int x, int y, int w, int h,
                                      uint32_t color);   // 无阴影:非物品行降级
lv_obj_t *ui_pixel_label(lv_obj_t *parent, const char *text,
                         const lv_font_t *font, uint32_t color);
lv_obj_t *ui_pixel_mascot_create(lv_obj_t *parent, int x, int y);
void ui_pixel_mascot_jump(lv_obj_t *mascot);
void ui_pixel_set_selected(lv_obj_t *panel, bool selected, bool enabled);
