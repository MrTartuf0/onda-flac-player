// Font generati con tools/gen_fonts.sh (Figtree + Font Awesome Free 6)
#pragma once
#include "../lvgl/lvgl.h"

LV_FONT_DECLARE(font_r10);
LV_FONT_DECLARE(font_r12);
LV_FONT_DECLARE(font_r14);
LV_FONT_DECLARE(font_b8);    // solo ASCII: intestazioni piccole in maiuscolo
LV_FONT_DECLARE(font_b10);
LV_FONT_DECLARE(font_b12);
LV_FONT_DECLARE(font_b14);
LV_FONT_DECLARE(font_b16);
LV_FONT_DECLARE(font_b20);
LV_FONT_DECLARE(font_d52);   // solo cifre "0-9 : + -" a larghezza fissa (timer)
LV_FONT_DECLARE(font_d36);
LV_FONT_DECLARE(font_b26);
LV_FONT_DECLARE(icons_14);
LV_FONT_DECLARE(icons_18);
LV_FONT_DECLARE(icons_22);
LV_FONT_DECLARE(icons_28);

// Icone (usare con i font icons_*)
#define IC_HOME          "\xEF\x80\x95"
#define IC_SEARCH        "\xEF\x80\x82"
#define IC_LIBRARY       "\xEE\x94\x9E"
#define IC_PLAY          "\xEF\x81\x8B"
#define IC_PAUSE         "\xEF\x81\x8C"
#define IC_PREV          "\xEF\x81\x88"
#define IC_NEXT          "\xEF\x81\x91"
#define IC_SHUFFLE       "\xEF\x81\xB4"
#define IC_REPEAT        "\xEF\x8D\xA3"
#define IC_HEART         "\xEF\x80\x84"
#define IC_HEART_O       "\xEE\x80\x84"
#define IC_HEADPHONES    "\xEF\x80\xA5"
#define IC_SPEAKER       "\xEF\x80\xA8"
#define IC_VOL_LOW       "\xEF\x80\xA7"
#define IC_VOL_MUTE      "\xEF\x9A\xA9"
#define IC_MORE          "\xEF\x85\x81"
#define IC_DOWN          "\xEF\x81\xB8"
#define IC_BACK          "\xEF\x81\x93"
#define IC_RIGHT         "\xEF\x81\x94"
#define IC_QUEUE         "\xEF\x80\xBA"
#define IC_MUSIC         "\xEF\x80\x81"
#define IC_DISC          "\xEF\x94\x9F"
#define IC_USER          "\xEF\x80\x87"
#define IC_GEAR          "\xEF\x80\x93"
#define IC_REFRESH       "\xEF\x8B\xB1"
#define IC_CLOSE         "\xEF\x80\x8D"
#define IC_CHECK         "\xEF\x80\x8C"
#define IC_SORT          "\xEF\x85\xA0"
#define IC_PLUS          "\xEF\x81\xA7"
#define IC_SDCARD        "\xEF\x9F\x82"
#define IC_SLIDERS       "\xEF\x87\x9E"
#define IC_CLOCK         "\xEF\x80\x97"
#define IC_ARROW_LEFT    "\xEF\x81\xA0"
#define IC_BARS          "\xEF\x95\x90"
#define IC_SUN           "\xEF\x86\x85"
#define IC_CHECK_CIRCLE  "\xEF\x81\x98"
#define IC_WIFI          "\xEF\x87\xAB"
#define IC_LOCK          "\xEF\x80\xA3"
#define IC_UPLOAD        "\xEF\x82\x93"
#define IC_BOLT          "\xEF\x83\xA7"
#define IC_STOPWATCH     "\xEF\x8B\xB2"
#define IC_STOP          "\xEF\x81\x8D"
#define IC_MIC           "\xEF\x84\xB0"
