/*
 * Configurazione LVGL 9.5 per onda (incluso nello sketch in src/lvgl).
 * Solo le voci diverse dai default: il resto lo completa lv_conf_internal.h.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

/* Colore: RGB565. Lo scambio dei byte per l'ILI9341 lo fa display.cpp. */
#define LV_COLOR_DEPTH 16

/* Memoria: malloc della libc. Con heap_caps_malloc_extmem_enable(64) gli oggetti
 * finiscono in PSRAM e la RAM interna resta per DMA e stack. */
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING    LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_BUILTIN

#define LV_DEF_REFR_PERIOD  20
#define LV_DPI_DEF          130

#define LV_USE_OS   LV_OS_NONE

/* Disegno software */
#define LV_USE_DRAW_SW 1
#define LV_DRAW_SW_SUPPORT_RGB565       1
#define LV_DRAW_SW_SUPPORT_RGB565_SWAPPED 0
#define LV_DRAW_SW_SUPPORT_RGB565A8     1
#define LV_DRAW_SW_SUPPORT_RGB888       0
#define LV_DRAW_SW_SUPPORT_XRGB8888     0
#define LV_DRAW_SW_SUPPORT_ARGB8888     1
#define LV_DRAW_SW_SUPPORT_ARGB8888_PREMULTIPLIED 0
#define LV_DRAW_SW_SUPPORT_L8           0
#define LV_DRAW_SW_SUPPORT_AL88         0
#define LV_DRAW_SW_SUPPORT_A8           1
#define LV_DRAW_SW_SUPPORT_I1           1   /* codici QR (trasferimento file) */
#define LV_DRAW_SW_DRAW_UNIT_CNT        1
#define LV_USE_DRAW_ARM2D_SYNC          0
#define LV_USE_NATIVE_HELIUM_ASM        0
#define LV_DRAW_SW_COMPLEX              1
#define LV_DRAW_SW_SHADOW_CACHE_SIZE    0
#define LV_DRAW_SW_CIRCLE_CACHE_SIZE    8
#define LV_USE_DRAW_SW_ASM              LV_DRAW_SW_ASM_NONE
#define LV_USE_DRAW_SW_COMPLEX_GRADIENTS 0
#define LV_DRAW_LAYER_SIMPLE_BUF_SIZE   (24 * 1024)

/* Niente log/strumenti di debug */
#define LV_USE_LOG          0
#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0
#define LV_USE_SYSMON       0
#define LV_USE_PROFILER     0
#define LV_USE_REFR_DEBUG   0

/* Le copertine sono già decodificate in RAM (artwork.cpp): nessuna cache immagini */
#define LV_CACHE_DEF_SIZE               0
#define LV_IMAGE_HEADER_CACHE_DEF_CNT   0
#define LV_GRADIENT_MAX_STOPS           2

#define LV_USE_FLOAT        0
#define LV_USE_MATRIX       0
#define LV_USE_VECTOR_GRAPHIC 0
#define LV_USE_OBJ_ID       0
#define LV_USE_OBJ_NAME     0
#define LV_USE_OBSERVER     1

/* Font: quelli dell'interfaccia sono in src/fonts; Montserrat 14 solo per i
 * simboli della tastiera e come predefinito. */
#define LV_FONT_MONTSERRAT_14       1
#define LV_FONT_DEFAULT             &lv_font_montserrat_14
#define LV_USE_FONT_COMPRESSED      0
#define LV_USE_FONT_PLACEHOLDER     1

#define LV_TXT_ENC          LV_TXT_ENC_UTF8
#define LV_USE_BIDI         0
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/* Widget: solo quelli usati (la tastiera richiede buttonmatrix e textarea). */
#define LV_WIDGETS_HAS_DEFAULT_VALUE 0
#define LV_USE_ANIMIMG      0
#define LV_USE_ARC          1   // anello della batteria (centro di controllo)
#define LV_USE_ARCLABEL     0
#define LV_USE_BAR          1
#define LV_USE_BUTTON       1
#define LV_USE_BUTTONMATRIX 1
#define LV_USE_CALENDAR     0
#define LV_USE_CANVAS       1   /* richiesto dal codice QR */
#define LV_USE_CHART        0
#define LV_USE_CHECKBOX     0
#define LV_USE_DROPDOWN     0
#define LV_USE_IMAGE        1
#define LV_USE_IMAGEBUTTON  0
#define LV_USE_KEYBOARD     1
#define LV_USE_LABEL        1
#define LV_LABEL_TEXT_SELECTION 1
#define LV_LABEL_LONG_TXT_HINT  1
#define LV_USE_LED          0
#define LV_USE_LINE         0
#define LV_USE_LIST         0
#define LV_USE_LOTTIE       0
#define LV_USE_MENU         0
#define LV_USE_MSGBOX       0
#define LV_USE_ROLLER       1   // rotelle ore / min / sec del timer
#define LV_USE_SCALE        0
#define LV_USE_SLIDER       1
#define LV_USE_SPAN         0
#define LV_USE_SPINBOX      0
#define LV_USE_SPINNER      0
#define LV_USE_SWITCH       1
#define LV_USE_TABLE        0
#define LV_USE_TABVIEW      0
#define LV_USE_TEXTAREA     1
#define LV_USE_TILEVIEW     0
#define LV_USE_WIN          0
#define LV_USE_3DTEXTURE    0

/* Tema */
#define LV_USE_THEME_DEFAULT        1
#define LV_THEME_DEFAULT_DARK       1
#define LV_THEME_DEFAULT_GROW       0
#define LV_THEME_DEFAULT_TRANSITION_TIME 80
#define LV_USE_THEME_SIMPLE         0
#define LV_USE_THEME_MONO           0

#define LV_USE_FLEX 1
#define LV_USE_GRID 0

/* Niente file system, decoder, driver esterni: li gestisce il firmware */
#define LV_USE_FS_STDIO     0
#define LV_USE_FS_POSIX     0
#define LV_USE_FS_WIN32     0
#define LV_USE_FS_FATFS     0
#define LV_USE_FS_MEMFS     0
#define LV_USE_LODEPNG      0
#define LV_USE_LIBPNG       0
#define LV_USE_BMP          0
#define LV_USE_TJPGD        0
#define LV_USE_LIBJPEG_TURBO 0
#define LV_USE_GIF          0
#define LV_USE_QRCODE       1
#define LV_USE_BARCODE      0
#define LV_USE_FREETYPE     0
#define LV_USE_TINY_TTF     0
#define LV_USE_RLOTTIE      0
#define LV_USE_THORVG_INTERNAL 0
#define LV_USE_THORVG_EXTERNAL 0
#define LV_USE_LZ4_INTERNAL 0
#define LV_USE_FFMPEG       0
#define LV_USE_SNAPSHOT     0
#define LV_USE_MONKEY       0
#define LV_USE_GRIDNAV      0
#define LV_USE_FRAGMENT     0
#define LV_USE_IMGFONT      0
#define LV_USE_IME_PINYIN   0
#define LV_USE_FILE_EXPLORER 0
#define LV_USE_FONT_MANAGER 0
#define LV_USE_TEST         0
#define LV_USE_XML          0
#define LV_USE_TRANSLATION  0
#define LV_USE_GLTF         0

#define LV_BUILD_EXAMPLES   0
#define LV_BUILD_DEMOS      0

#endif /* LV_CONF_H */
