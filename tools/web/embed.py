#!/usr/bin/env python3
# Incorpora tools/web/index.html nel firmware (src/net/web_index.h).
# Da rilanciare dopo ogni modifica alla pagina:  python3 tools/web/embed.py
import os, re
here = os.path.dirname(os.path.abspath(__file__))
html = open(os.path.join(here, 'index.html'), encoding='utf-8').read()
# alleggerisce un po': niente indentazione iniziale e righe vuote
html = '\n'.join(l.strip() for l in html.splitlines() if l.strip())
assert ')ONDAWEB"' not in html
out = os.path.join(here, '..', '..', 'src', 'net', 'web_index.h')
with open(out, 'w', encoding='utf-8') as f:
    f.write('// Generato da tools/web/embed.py a partire da tools/web/index.html: non modificare a mano.\n')
    f.write('#pragma once\n\n')
    f.write('static const char kWebIndex[] = R"ONDAWEB(' + html + ')ONDAWEB";\n')
print(out, len(html.encode()), 'byte')
