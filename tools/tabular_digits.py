#!/usr/bin/env python3
"""Cifre a larghezza fissa in un font generato da lv_font_conv (come la
"tnum" di OpenType): tutte le cifre prendono l'avanzamento della più larga e
il disegno viene centrato. Serve ai numeri che cambiano (timer), che
altrimenti ballano a ogni secondo.

  python3 tabular_digits.py ../src/fonts/font_d52.c
"""
import re, sys

path = sys.argv[1]
src = open(path, encoding='utf-8').read()
GLYPH = re.compile(r'\{\.bitmap_index = (\d+), \.adv_w = (\d+), \.box_w = (\d+), \.box_h = (\d+), '
                   r'\.ofs_x = (-?\d+), \.ofs_y = (-?\d+)\}')
glyphs = list(GLYPH.finditer(src))
# mappa codepoint -> id del glifo dai range del cmap
cmap = re.findall(r'\.range_start = (\d+), \.range_length = (\d+), \.glyph_id_start = (\d+)', src)
gid = {}
for start, length, first in cmap:
    for i in range(int(length)):
        gid[int(start) + i] = int(first) + i
digits = [gid[c] for c in range(0x30, 0x3A)]
adv = {g: int(glyphs[g].group(2)) for g in digits}
widest = max(adv.values())
out, last = [], 0
for g in digits:
    m = glyphs[g]
    shift = round((widest - adv[g]) / 32)   # metà della differenza, in pixel (adv_w è in 1/16 px)
    new = (f'{{.bitmap_index = {m.group(1)}, .adv_w = {widest}, .box_w = {m.group(3)}, .box_h = {m.group(4)}, '
           f'.ofs_x = {int(m.group(5)) + shift}, .ofs_y = {m.group(6)}}}')
    out.append(src[last:m.start()]); out.append(new); last = m.end()
out.append(src[last:])
open(path, 'w', encoding='utf-8').write(''.join(out))
print(f'{path}: cifre a {widest / 16:.1f} px')
