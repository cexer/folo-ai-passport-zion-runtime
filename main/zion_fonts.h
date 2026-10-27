#pragma once
#include "lvgl.h"
LV_FONT_DECLARE(zion_font_14);
#if defined(CONFIG_ZION_LAUNCHER) && CONFIG_ZION_LAUNCHER
LV_FONT_DECLARE(zion_font_ui_14);
LV_FONT_DECLARE(zion_font_ui_10);
#define ZION_FONT (&zion_font_ui_14)
#define ZION_FONT_SMALL (&zion_font_ui_10)
#else
#define ZION_FONT (&zion_font_14)
#define ZION_FONT_SMALL (&lv_font_unscii_8)
#endif
