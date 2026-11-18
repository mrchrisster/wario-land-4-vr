#!/usr/bin/env python3
"""plane_sheet.py out.png dumpdir... : flat composite + exploded STOCK 3D planes from GBARECOMP_PLANE_DUMP dirs."""
import sys, numpy as np
from PIL import Image, ImageDraw
W, H = 240, 160
COL = [(160,160,160),(90,140,255),(90,220,90),(255,200,80),(255,90,90)]
def sheet_row(d):
    comp = Image.frombytes('RGB', (W, H), open(f'{d}/composite.rgb','rb').read()).resize((W*2,H*2), Image.NEAREST)
    planes = [Image.frombytes('RGBA',(W,H),open(f'{d}/stock_plane_{i}.rgba','rb').read()) for i in range(5)]
    S = 2; dx, dy = 60, 34; n = 5
    w, h = W*S, H*S; sw = int(h*0.25)
    out = Image.new('RGBA', (int(w*0.8)+sw+dx*(n-1)+30, h+dy*(n-1)+30), (24,24,28,255)); dr = ImageDraw.Draw(out)
    for i in range(n):  # 0 = back
        depth = i
        im = planes[i].resize((w,h), Image.NEAREST)
        im = im.transform((int(w*0.8)+sw, h), Image.AFFINE, (1/0.8, -0.25/0.8, 0, 0, 1, 0), Image.NEAREST)
        ox = 15 + dx*(n-1-depth); oy = 15 + dy*depth
        out.alpha_composite(im, (ox, oy))
        dr.polygon([(ox,oy),(ox+int(w*0.8),oy),(ox+int(w*0.8)+sw,oy+h),(ox+sw,oy+h)], outline=COL[i])
        dr.text((ox+4, oy+4), f'P{i}', fill=COL[i])
    row = Image.new('RGBA', (comp.width+out.width+30, max(comp.height, out.height)+20), (16,16,18,255))
    row.paste(comp, (10, 10)); row.alpha_composite(out, (comp.width+20, 10))
    return row
rows = [sheet_row(d) for d in sys.argv[2:]]
s = Image.new('RGB', (max(r.width for r in rows), sum(r.height for r in rows)), (16,16,18))
y = 0
for r in rows: s.paste(r, (0, y)); y += r.height
s.save(sys.argv[1])
