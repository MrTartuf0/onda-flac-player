/*******************************************************************************
 * Size: 8 px
 * Bpp: 4
 * Opts: --font fonts/Figtree-Bold.ttf -r 0x20-0x7E --size 8 --bpp 4 --format lvgl --no-compress --lv-include ../lvgl/lvgl.h --lv-font-name font_b8 -o ../src/fonts/font_b8.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "../lvgl/lvgl.h"
#endif

#ifndef FONT_B8
#define FONT_B8 1
#endif

#if FONT_B8

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */

    /* U+0021 "!" */
    0x5d, 0x5c, 0x4c, 0x3a, 0x4b,

    /* U+0022 "\"" */
    0x79, 0xc6, 0x8b,

    /* U+0023 "#" */
    0x6, 0x5a, 0x16, 0xec, 0xfa, 0xb, 0x1c, 0x9,
    0xfc, 0xe7, 0xb, 0x57, 0x0,

    /* U+0024 "$" */
    0x3, 0xc0, 0x4, 0xec, 0xb0, 0x7b, 0x1, 0x1,
    0x9e, 0x90, 0x23, 0x1f, 0x13, 0xde, 0x90, 0x2,
    0xb0, 0x0,

    /* U+0025 "%" */
    0x6b, 0x90, 0xb5, 0x5, 0xb8, 0x78, 0x0, 0x0,
    0x4c, 0xb9, 0x0, 0x2d, 0x76, 0xa1, 0xc, 0x32,
    0xba, 0x0,

    /* U+0026 "&" */
    0x1c, 0xd8, 0x0, 0x1d, 0x7a, 0x0, 0x2c, 0xe3,
    0x60, 0x98, 0x4f, 0xa0, 0x3e, 0xeb, 0xc0,

    /* U+0027 "'" */
    0x77, 0x66,

    /* U+0028 "(" */
    0x0, 0x0, 0xa6, 0x2e, 0x7, 0x80, 0x97, 0x7,
    0x90, 0x2e, 0x0, 0x96,

    /* U+0029 ")" */
    0x0, 0x5, 0xb0, 0xd, 0x30, 0x88, 0x6, 0xa0,
    0x88, 0xd, 0x36, 0xa0,

    /* U+002A "*" */
    0x17, 0x61, 0x3d, 0xd2, 0x7, 0x80,

    /* U+002B "+" */
    0x0, 0x80, 0x0, 0xd, 0x0, 0x4d, 0xfd, 0x50,
    0xd, 0x0,

    /* U+002C "," */
    0x58, 0xa3,

    /* U+002D "-" */
    0x7e, 0xb0,

    /* U+002E "." */
    0x11, 0x77,

    /* U+002F "/" */
    0x0, 0x62, 0x0, 0xe0, 0x5, 0xa0, 0xb, 0x50,
    0xe, 0x0, 0x5a, 0x0, 0xb4, 0x0,

    /* U+0030 "0" */
    0xa, 0xfc, 0x18, 0xb0, 0x8b, 0xb6, 0x3, 0xe8,
    0xb0, 0x8b, 0xa, 0xfc, 0x10,

    /* U+0031 "1" */
    0x3d, 0x95, 0xa9, 0x7, 0x90, 0x79, 0x7, 0x90,

    /* U+0032 "2" */
    0x3d, 0xe9, 0x5, 0x51, 0xf2, 0x1, 0xa9, 0x2,
    0xd5, 0x0, 0x9f, 0xff, 0x40,

    /* U+0033 "3" */
    0x3d, 0xe8, 0x2, 0x23, 0xe0, 0x7, 0xf8, 0x2,
    0x22, 0xf1, 0x4d, 0xe9, 0x0,

    /* U+0034 "4" */
    0x0, 0xbd, 0x0, 0x8b, 0xd0, 0x5c, 0x3d, 0xc,
    0xee, 0xfc, 0x0, 0x3d, 0x0,

    /* U+0035 "5" */
    0x3f, 0xff, 0x6, 0xed, 0x90, 0x13, 0xe, 0x32,
    0x40, 0xe3, 0x2d, 0xe8, 0x0,

    /* U+0036 "6" */
    0xa, 0xeb, 0x7, 0xee, 0x90, 0xaa, 0xd, 0x48,
    0xa0, 0xd4, 0x1d, 0xea, 0x0,

    /* U+0037 "7" */
    0xbf, 0xff, 0x30, 0x5, 0xc0, 0x0, 0xd4, 0x0,
    0x6c, 0x0, 0xe, 0x30, 0x0,

    /* U+0038 "8" */
    0x2d, 0xec, 0x16, 0xb0, 0xd4, 0x1e, 0xfd, 0x9,
    0x90, 0xb7, 0x4d, 0xed, 0x20,

    /* U+0039 "9" */
    0x4d, 0xe9, 0xa, 0x80, 0xe3, 0x3c, 0xde, 0x40,
    0x12, 0xe1, 0x4e, 0xe6, 0x0,

    /* U+003A ":" */
    0x4a, 0x1, 0x1, 0x4a,

    /* U+003B ";" */
    0x4a, 0x1, 0x0, 0x5a, 0x83,

    /* U+003C "<" */
    0x0, 0x0, 0x6c, 0xea, 0x15, 0xbc, 0x0, 0x20,

    /* U+003D "=" */
    0x1d, 0xdd, 0x10, 0x0, 0x0, 0x1d, 0xdd, 0x10,

    /* U+003E ">" */
    0x0, 0x0, 0xc6, 0x0, 0x1a, 0xf0, 0xbc, 0x50,
    0x20, 0x0,

    /* U+003F "?" */
    0x6e, 0xe5, 0x12, 0x7b, 0xb, 0xa2, 0x5, 0x20,
    0xa, 0x50,

    /* U+0040 "@" */
    0x2, 0xac, 0xbb, 0x30, 0x1c, 0x9e, 0xcd, 0xb2,
    0x84, 0xe1, 0x4c, 0x48, 0xb1, 0xe1, 0x4c, 0x39,
    0x84, 0x6e, 0x9c, 0xb2, 0x1c, 0x20, 0x0, 0x0,
    0x2, 0xab, 0xc6, 0x0,

    /* U+0041 "A" */
    0x0, 0xea, 0x0, 0x6, 0xcf, 0x10, 0xd, 0x49,
    0x80, 0x4f, 0xff, 0xe0, 0xb7, 0x0, 0xb6,

    /* U+0042 "B" */
    0x6f, 0xdc, 0x16, 0xb0, 0xd5, 0x6f, 0xef, 0x26,
    0xb0, 0xa8, 0x6f, 0xfd, 0x30,

    /* U+0043 "C" */
    0x9, 0xee, 0x90, 0x7c, 0x11, 0x93, 0xb7, 0x0,
    0x0, 0x7c, 0x11, 0x94, 0x9, 0xee, 0x90,

    /* U+0044 "D" */
    0x6f, 0xfd, 0x40, 0x6b, 0x6, 0xf1, 0x6b, 0x0,
    0xe4, 0x6b, 0x6, 0xf1, 0x6f, 0xfd, 0x40,

    /* U+0045 "E" */
    0x6f, 0xff, 0x26, 0xb0, 0x0, 0x6f, 0xfc, 0x6,
    0xb0, 0x0, 0x6f, 0xff, 0x20,

    /* U+0046 "F" */
    0x6f, 0xff, 0x36, 0xb0, 0x0, 0x6f, 0xfc, 0x6,
    0xb0, 0x0, 0x6b, 0x0, 0x0,

    /* U+0047 "G" */
    0x8, 0xef, 0xa1, 0x7d, 0x20, 0x51, 0xa7, 0x8,
    0xda, 0x7d, 0x20, 0xa9, 0x8, 0xef, 0xa0,

    /* U+0048 "H" */
    0x6b, 0x0, 0xb6, 0x6b, 0x0, 0xb6, 0x6f, 0xff,
    0xf6, 0x6b, 0x0, 0xb6, 0x6b, 0x0, 0xb6,

    /* U+0049 "I" */
    0x6b, 0x6b, 0x6b, 0x6b, 0x6b,

    /* U+004A "J" */
    0x0, 0x3e, 0x0, 0x3e, 0x0, 0x3e, 0x62, 0x6d,
    0x6e, 0xe5,

    /* U+004B "K" */
    0x6b, 0xb, 0xa0, 0x6b, 0xc9, 0x0, 0x6f, 0xf2,
    0x0, 0x6b, 0x6d, 0x10, 0x6b, 0x8, 0xc0,

    /* U+004C "L" */
    0x6b, 0x0, 0x6, 0xb0, 0x0, 0x6b, 0x0, 0x6,
    0xb0, 0x0, 0x6f, 0xff, 0x20,

    /* U+004D "M" */
    0x6f, 0x30, 0x5f, 0x46, 0xed, 0x3e, 0xf4, 0x6b,
    0x8f, 0x6d, 0x46, 0xb0, 0x70, 0xd4, 0x6b, 0x0,
    0xd, 0x40,

    /* U+004E "N" */
    0x6f, 0x30, 0x89, 0x6e, 0xe1, 0x89, 0x6b, 0x8c,
    0x89, 0x6b, 0xb, 0xf9, 0x6b, 0x1, 0xd9,

    /* U+004F "O" */
    0x8, 0xef, 0xb1, 0x7, 0xd2, 0x19, 0xc0, 0xb7,
    0x0, 0x2f, 0x7, 0xd2, 0x9, 0xc0, 0x8, 0xef,
    0xb1, 0x0,

    /* U+0050 "P" */
    0x6f, 0xfd, 0x36, 0xb0, 0x9a, 0x6f, 0xfd, 0x36,
    0xb0, 0x0, 0x6b, 0x0, 0x0,

    /* U+0051 "Q" */
    0x8, 0xef, 0xb1, 0x7, 0xd2, 0x9, 0xc0, 0xb7,
    0x0, 0x2f, 0x7, 0xd2, 0x8e, 0xb0, 0x8, 0xef,
    0xcb, 0x0, 0x0, 0x0, 0x0,

    /* U+0052 "R" */
    0x6f, 0xfd, 0x36, 0xb0, 0xaa, 0x6f, 0xfe, 0x36,
    0xb2, 0xe1, 0x6b, 0x9, 0xa0,

    /* U+0053 "S" */
    0x2d, 0xfc, 0x17, 0xb0, 0x51, 0x18, 0xc9, 0x3,
    0x40, 0xc7, 0x3c, 0xfd, 0x20,

    /* U+0054 "T" */
    0xdf, 0xff, 0x80, 0x3e, 0x0, 0x3, 0xe0, 0x0,
    0x3e, 0x0, 0x3, 0xe0, 0x0,

    /* U+0055 "U" */
    0x7a, 0x0, 0xf1, 0x7a, 0x0, 0xf1, 0x7a, 0x0,
    0xf1, 0x6c, 0x13, 0xf0, 0xa, 0xed, 0x50,

    /* U+0056 "V" */
    0xb7, 0x0, 0xa7, 0x4d, 0x1, 0xf1, 0xd, 0x48,
    0x90, 0x6, 0xce, 0x20, 0x0, 0xeb, 0x0,

    /* U+0057 "W" */
    0xc5, 0xc, 0xa0, 0x7a, 0x7a, 0x1e, 0xe0, 0xc4,
    0x1f, 0x6a, 0xc6, 0xf0, 0xc, 0xe5, 0x7e, 0xa0,
    0x7, 0xf0, 0x2f, 0x40,

    /* U+0058 "X" */
    0x7c, 0x6, 0xd0, 0xa, 0xbe, 0x20, 0x1, 0xf8,
    0x0, 0xb, 0xae, 0x30, 0x8c, 0x5, 0xe1,

    /* U+0059 "Y" */
    0x9a, 0x7, 0xc0, 0xd, 0x7e, 0x20, 0x3, 0xf6,
    0x0, 0x0, 0xf2, 0x0, 0x0, 0xf2, 0x0,

    /* U+005A "Z" */
    0xaf, 0xff, 0xc0, 0x3, 0xe4, 0x3, 0xe4, 0x3,
    0xe4, 0x0, 0xbf, 0xff, 0xb0,

    /* U+005B "[" */
    0x5e, 0x46, 0x80, 0x68, 0x6, 0x80, 0x68, 0x6,
    0x80, 0x6f, 0x40,

    /* U+005C "\\" */
    0x61, 0x0, 0x87, 0x0, 0x3c, 0x0, 0xd, 0x20,
    0x8, 0x70, 0x3, 0xd0, 0x0, 0xd2,

    /* U+005D "]" */
    0x7f, 0x20, 0xc2, 0xc, 0x20, 0xc2, 0xc, 0x20,
    0xc2, 0x8f, 0x20,

    /* U+005E "^" */
    0x2, 0x70, 0x0, 0xad, 0x40, 0x2c, 0x2c, 0x0,

    /* U+005F "_" */
    0xce, 0xe4,

    /* U+0060 "`" */
    0x88, 0x0, 0x20,

    /* U+0061 "a" */
    0x4d, 0xe5, 0x4c, 0xcd, 0xb6, 0x5d, 0x6e, 0xcd,

    /* U+0062 "b" */
    0x88, 0x0, 0x8, 0xce, 0xb1, 0x8b, 0xb, 0x68,
    0xa0, 0xb6, 0x8c, 0xec, 0x10,

    /* U+0063 "c" */
    0x2c, 0xf9, 0xa, 0x80, 0x50, 0xa8, 0x5, 0x2,
    0xcf, 0xa0,

    /* U+0064 "d" */
    0x0, 0xc, 0x43, 0xdd, 0xe4, 0xa8, 0xe, 0x4a,
    0x80, 0xe4, 0x3d, 0xdd, 0x40,

    /* U+0065 "e" */
    0x2c, 0xd8, 0xa, 0xdc, 0xd2, 0xa9, 0x6, 0x2,
    0xce, 0x90,

    /* U+0066 "f" */
    0x1d, 0xc0, 0xbf, 0xb0, 0x2e, 0x0, 0x2e, 0x0,
    0x2e, 0x0,

    /* U+0067 "g" */
    0x2d, 0xdd, 0x5a, 0x80, 0xe5, 0xa8, 0xe, 0x53,
    0xdc, 0xd5, 0x11, 0xd, 0x34, 0xee, 0x90,

    /* U+0068 "h" */
    0x88, 0x0, 0x8, 0xde, 0xb0, 0x8a, 0x1f, 0x28,
    0x80, 0xe2, 0x88, 0xe, 0x20,

    /* U+0069 "i" */
    0x68, 0x8a, 0x79, 0x79, 0x79,

    /* U+006A "j" */
    0x3, 0xb0, 0x4e, 0x4, 0xc0, 0x4c, 0x4, 0xc0,
    0x5c, 0x4f, 0x60,

    /* U+006B "k" */
    0x88, 0x0, 0x8, 0x89, 0xa0, 0x8e, 0xd0, 0x8,
    0xde, 0x30, 0x88, 0x5d, 0x0,

    /* U+006C "l" */
    0x88, 0x88, 0x88, 0x88, 0x88,

    /* U+006D "m" */
    0x8d, 0xea, 0xbe, 0x38, 0xa1, 0xf2, 0x88, 0x88,
    0xf, 0x7, 0x98, 0x80, 0xf0, 0x79,

    /* U+006E "n" */
    0x8d, 0xeb, 0x8, 0xa0, 0xf1, 0x88, 0xe, 0x28,
    0x80, 0xe2,

    /* U+006F "o" */
    0x2c, 0xe9, 0xa, 0x80, 0xd5, 0xa7, 0xd, 0x52,
    0xce, 0x90,

    /* U+0070 "p" */
    0x8d, 0xdc, 0x18, 0xa0, 0xa7, 0x8b, 0xb, 0x78,
    0xde, 0xc1, 0x88, 0x0, 0x8, 0x80, 0x0,

    /* U+0071 "q" */
    0x3d, 0xdd, 0x5a, 0x80, 0xe5, 0xa8, 0xe, 0x53,
    0xdd, 0xd5, 0x0, 0xc, 0x50, 0x0, 0xc5,

    /* U+0072 "r" */
    0x8c, 0xd8, 0xb0, 0x88, 0x8, 0x80,

    /* U+0073 "s" */
    0x3e, 0xe3, 0x6d, 0x60, 0x16, 0xc7, 0x5e, 0xe3,

    /* U+0074 "t" */
    0x3d, 0xc, 0xfb, 0x3d, 0x3, 0xd0, 0x1d, 0xc0,

    /* U+0075 "u" */
    0x97, 0xf, 0x19, 0x70, 0xf1, 0x99, 0x2f, 0x13,
    0xec, 0xf1,

    /* U+0076 "v" */
    0xb5, 0xe, 0x25, 0xb4, 0xc0, 0xe, 0xc5, 0x0,
    0x8e, 0x0,

    /* U+0077 "w" */
    0xc5, 0x5f, 0xa, 0x66, 0xaa, 0xd6, 0xe1, 0x1e,
    0xe4, 0xeb, 0x0, 0xba, 0xe, 0x60,

    /* U+0078 "x" */
    0x7c, 0x7b, 0x0, 0xbe, 0x10, 0xd, 0xe2, 0x9,
    0xa6, 0xd0,

    /* U+0079 "y" */
    0xc6, 0xc, 0x65, 0xc2, 0xf0, 0xe, 0xa9, 0x0,
    0x7f, 0x30, 0x4, 0xc0, 0x7, 0xe4, 0x0,

    /* U+007A "z" */
    0xad, 0xf8, 0x5, 0xd1, 0x2e, 0x20, 0xbf, 0xd7,

    /* U+007B "{" */
    0x5, 0x90, 0xc2, 0xd, 0x17, 0xc0, 0xd, 0x10,
    0xc3, 0x6, 0x90,

    /* U+007C "|" */
    0x56, 0x67, 0x67, 0x67, 0x67, 0x67, 0x67, 0x67,

    /* U+007D "}" */
    0x77, 0x0, 0xe0, 0xe, 0x0, 0xaa, 0xe, 0x0,
    0xe0, 0x77, 0x0,

    /* U+007E "~" */
    0x3e, 0xc2, 0x53, 0x38, 0xe7
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 30, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 0, .adv_w = 40, .box_w = 2, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 5, .adv_w = 53, .box_w = 3, .box_h = 2, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 8, .adv_w = 81, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 21, .adv_w = 71, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 39, .adv_w = 104, .box_w = 7, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 57, .adv_w = 83, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 72, .adv_w = 31, .box_w = 2, .box_h = 2, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 74, .adv_w = 49, .box_w = 3, .box_h = 8, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 86, .adv_w = 49, .box_w = 3, .box_h = 8, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 98, .adv_w = 62, .box_w = 4, .box_h = 3, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 104, .adv_w = 81, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 114, .adv_w = 33, .box_w = 2, .box_h = 2, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 116, .adv_w = 52, .box_w = 3, .box_h = 1, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 118, .adv_w = 32, .box_w = 2, .box_h = 2, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 120, .adv_w = 55, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 134, .adv_w = 83, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 147, .adv_w = 54, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 155, .adv_w = 74, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 168, .adv_w = 71, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 181, .adv_w = 81, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 194, .adv_w = 74, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 207, .adv_w = 74, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 220, .adv_w = 71, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 233, .adv_w = 78, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 246, .adv_w = 74, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 259, .adv_w = 39, .box_w = 2, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 263, .adv_w = 39, .box_w = 2, .box_h = 5, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 268, .adv_w = 81, .box_w = 3, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 276, .adv_w = 81, .box_w = 5, .box_h = 3, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 284, .adv_w = 81, .box_w = 4, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 294, .adv_w = 66, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 304, .adv_w = 127, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 332, .adv_w = 91, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 347, .adv_w = 78, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 360, .adv_w = 93, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 375, .adv_w = 90, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 390, .adv_w = 75, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 403, .adv_w = 71, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 416, .adv_w = 95, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 431, .adv_w = 96, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 446, .adv_w = 37, .box_w = 2, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 451, .adv_w = 71, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 461, .adv_w = 85, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 476, .adv_w = 69, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 489, .adv_w = 110, .box_w = 7, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 507, .adv_w = 99, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 522, .adv_w = 101, .box_w = 7, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 540, .adv_w = 77, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 553, .adv_w = 101, .box_w = 7, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 574, .adv_w = 83, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 587, .adv_w = 78, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 600, .adv_w = 75, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 613, .adv_w = 90, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 628, .adv_w = 92, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 643, .adv_w = 126, .box_w = 8, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 663, .adv_w = 87, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 678, .adv_w = 83, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 693, .adv_w = 81, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 706, .adv_w = 44, .box_w = 3, .box_h = 7, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 717, .adv_w = 55, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 731, .adv_w = 44, .box_w = 3, .box_h = 7, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 742, .adv_w = 74, .box_w = 5, .box_h = 3, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 750, .adv_w = 56, .box_w = 4, .box_h = 1, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 752, .adv_w = 34, .box_w = 3, .box_h = 2, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 755, .adv_w = 68, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 763, .adv_w = 76, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 776, .adv_w = 70, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 786, .adv_w = 76, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 799, .adv_w = 71, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 809, .adv_w = 50, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 819, .adv_w = 77, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 834, .adv_w = 73, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 847, .adv_w = 34, .box_w = 2, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 852, .adv_w = 37, .box_w = 3, .box_h = 7, .ofs_x = -1, .ofs_y = -2},
    {.bitmap_index = 863, .adv_w = 69, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 876, .adv_w = 31, .box_w = 2, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 881, .adv_w = 112, .box_w = 7, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 895, .adv_w = 73, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 905, .adv_w = 74, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 915, .adv_w = 77, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 930, .adv_w = 76, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 945, .adv_w = 49, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 951, .adv_w = 60, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 959, .adv_w = 51, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 967, .adv_w = 73, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 977, .adv_w = 71, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 987, .adv_w = 107, .box_w = 7, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1001, .adv_w = 69, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1011, .adv_w = 74, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 1026, .adv_w = 61, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1034, .adv_w = 50, .box_w = 3, .box_h = 7, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1045, .adv_w = 33, .box_w = 2, .box_h = 8, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 1053, .adv_w = 50, .box_w = 3, .box_h = 7, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1064, .adv_w = 85, .box_w = 5, .box_h = 2, .ofs_x = 0, .ofs_y = 1}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};

/*-----------------
 *    KERNING
 *----------------*/


/*Map glyph_ids to kern left classes*/
static const uint8_t kern_left_class_mapping[] =
{
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 2, 0, 0, 3, 0, 4,
    5, 0, 0, 0, 6, 7, 0, 0,
    8, 9, 0, 10, 0, 11, 3, 12,
    13, 14, 15, 0, 0, 0, 0, 0,
    0, 0, 16, 17, 17, 0, 17, 18,
    0, 16, 0, 0, 19, 0, 16, 20,
    17, 17, 0, 0, 21, 22, 0, 23,
    23, 24, 23, 25, 0, 0, 0, 0
};

/*Map glyph_ids to kern right classes*/
static const uint8_t kern_right_class_mapping[] =
{
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 2, 0, 3, 0, 0, 0,
    4, 0, 5, 0, 0, 0, 0, 0,
    6, 0, 0, 0, 0, 7, 0, 8,
    9, 0, 10, 0, 0, 0, 0, 0,
    0, 0, 11, 0, 11, 11, 11, 0,
    11, 0, 0, 12, 0, 0, 13, 13,
    14, 13, 11, 13, 15, 0, 13, 16,
    16, 17, 16, 18, 0, 0, 0, 0
};

/*Kern values between classes*/
static const int8_t kern_class_values[] =
{
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -2, 0, -1, -2, -1, 0,
    0, -1, 0, 0, -3, -5, 0, -3,
    -6, -10, -9, -5, -1, 0, 0, -1,
    0, -5, 0, 0, 0, 0, 0, 0,
    0, 0, -3, -3, -3, -3, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -1, -1, -1, -1, -1, -1, 0, -1,
    0, -3, 0, 0, -1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -4, 0, -1, -4,
    -1, -4, 0, -1, 0, 0, 0, 0,
    0, -2, -11, 0, 0, -6, 0, 0,
    0, 0, 0, -2, 0, 0, 0, 0,
    0, 0, 0, 0, -3, -3, -3, -3,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -1, 0, 0, -1, 0, 0,
    0, 0, 0, 1, 0, 0, 0, 0,
    -2, 0, 0, 0, 0, 0, 0, 0,
    0, -2, 0, 0, 0, -8, 0, 0,
    0, 0, 0, 0, 0, 0, -8, 0,
    -5, -8, -5, -5, 0, -4, 0, -10,
    0, 0, 0, -2, 0, 0, 0, 0,
    -5, 0, -2, -5, -2, 0, 0, -2,
    0, -9, 0, 0, 0, 0, 0, 0,
    0, 0, -5, 0, -2, -5, -2, 0,
    0, -2, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -4, 0, 0, -4,
    0, -4, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -11, 0,
    -6, -11, -6, -5, 0, -7, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -2, 0, 0,
    -2, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, -3,
    -2, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -2, 0, 0, -2,
    0, 0, 0, 0, -1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -3, 0,
    0, -3, 0, 0, 0, 0, -1, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -2, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -1, 0, 0, -1,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -2, 0,
    0, -2, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -2, 0, 0, -2, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0
};


/*Collect the kern class' data in one place*/
static const lv_font_fmt_txt_kern_classes_t kern_classes =
{
    .class_pair_values   = kern_class_values,
    .left_class_mapping  = kern_left_class_mapping,
    .right_class_mapping = kern_right_class_mapping,
    .left_class_cnt      = 25,
    .right_class_cnt     = 18,
};

/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = &kern_classes,
    .kern_scale = 16,
    .cmap_num = 1,
    .bpp = 4,
    .kern_classes = 1,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t font_b8 = {
#else
lv_font_t font_b8 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 9,          /*The maximum line height required by the font*/
    .base_line = 2,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 0,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if FONT_B8*/

