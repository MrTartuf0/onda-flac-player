#!/bin/sh
# Rigenera i font LVGL in ../src/fonts  (richiede: npm i -g lv_font_conv)
# Testo: Figtree (SIL OFL 1.1)  -  Icone: Font Awesome Free 6 (SIL OFL 1.1 / CC BY 4.0)
set -e
cd "$(dirname "$0")"
OUT=../src/fonts
F=fonts
TXT="0x20-0x7E,0xA0-0x17F,0x2013-0x2014,0x2018-0x201E,0x2022,0x2026,0x20AC"
# home search library play pause prev next shuffle repeat heart headphones vol-high vol-low
# ellipsis chev-down chev-left chev-right list music disc user gear rotate xmark check sort plus
# sd-card sliders clock arrow-left bars-staggered sun circle-check volume-xmark
ICONS="0xf015,0xf002,0xe51e,0xf04b,0xf04c,0xf048,0xf051,0xf074,0xf363,0xf004,0xf025,0xf028,0xf027,0xf141,0xf078,0xf053,0xf054,0xf03a,0xf001,0xf51f,0xf007,0xf013,0xf2f1,0xf00d,0xf00c,0xf160,0xf067,0xf7c2,0xf1de,0xf017,0xf060,0xf550,0xf185,0xf058,0xf6a9"
gen_txt() { # nome file dimensione
  lv_font_conv --font "$F/$2" -r "$TXT" --size $3 --bpp 4 --format lvgl --no-compress \
    --lv-include ../lvgl/lvgl.h --lv-font-name "$1" -o "$OUT/$1.c"
}
gen_ico() { # nome dimensione
  lv_font_conv --font "$F/fa-solid-900.ttf" -r "$ICONS" \
    --font "$F/fa-regular-400.ttf" -r "0xf004=>0xe004" \
    --size $2 --bpp 4 --format lvgl --no-compress --lv-include ../lvgl/lvgl.h --lv-font-name "$1" -o "$OUT/$1.c"
}
mkdir -p $OUT
gen_txt font_r10 Figtree-Regular.ttf 10
gen_txt font_r12 Figtree-Regular.ttf 12
gen_txt font_r14 Figtree-Regular.ttf 14
gen_txt font_b10 Figtree-Bold.ttf 10
gen_txt font_b12 Figtree-Bold.ttf 12
gen_txt font_b14 Figtree-Bold.ttf 14
gen_txt font_b16 Figtree-Bold.ttf 16
gen_txt font_b20 Figtree-ExtraBold.ttf 20
gen_txt font_b26 Figtree-ExtraBold.ttf 26
gen_ico icons_14 14
gen_ico icons_18 18
gen_ico icons_22 22
gen_ico icons_28 28
echo "Font generati in $OUT"
