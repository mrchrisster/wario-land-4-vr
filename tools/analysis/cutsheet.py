import sys, numpy as np, bgrender, ioreg
from PIL import Image, ImageDraw
COL={0:(255,90,90),1:(255,200,80),2:(90,220,90),3:(90,140,255),'bd':(160,160,160)}
def explode(L,bd,S=2):
    # back to front: backdrop, BG3..BG0 by index (cutscene priorities tie -> BG index order)
    order=['bd']+sorted(L,reverse=True); n=len(order); W,H=240*S,160*S; dx,dy=60,34; sw=int(H*0.25)
    out=Image.new('RGBA',(int(W*0.8)+sw+dx*(n-1)+30,H+dy*(n-1)+30),(24,24,28,255)); d=ImageDraw.Draw(out)
    for k,i in enumerate(order):
        im=Image.new('RGBA',(240,160),tuple(bd)+(255,)) if i=='bd' else Image.fromarray(L[i])
        im=im.resize((W,H),Image.NEAREST).transform((int(W*0.8)+sw,H),Image.AFFINE,(1/0.8,-0.25/0.8,0,0,1,0),Image.NEAREST)
        ox=15+dx*(n-1-k); oy=15+dy*k; out.alpha_composite(im,(ox,oy))
        d.polygon([(ox,oy),(ox+int(W*0.8),oy),(ox+int(W*0.8)+sw,oy+H),(ox+sw,oy+H)],outline=COL[i]); d.text((ox+4,oy+4),'backdrop' if i=='bd' else f'BG{i}',fill=COL[i])
    return out
rows=[]
for p in sys.argv[2:]:
    f=int(p.split('/f')[-1].split('/')[0]); st=f'{p}/s.state'
    L,r,bd=bgrender.layers(st)
    shot=Image.open(f'{p}/shot.png').convert('RGBA').resize((480,320),Image.NEAREST)
    e=explode(L,bd)
    row=Image.new('RGBA',(shot.width+e.width+30,max(shot.height,e.height)+30),(16,16,18,255))
    row.alpha_composite(shot,(10,24)); row.alpha_composite(e,(shot.width+20,24))
    ImageDraw.Draw(row).text((10,6),ioreg.line(f,r),fill=(230,230,230)); rows.append(row)
s=Image.new('RGB',(max(r.width for r in rows),sum(r.height for r in rows)),(16,16,18)); y=0
for r in rows: s.paste(r,(0,y)); y+=r.height
s.save(sys.argv[1])
