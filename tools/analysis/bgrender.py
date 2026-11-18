import struct, numpy as np, gbas, ioreg
def mem(path):
    s=gbas.sections(path); b=s['BUS0']; o=0x40000+0x8000
    pal=np.frombuffer(b[o:o+0x400],'<u2'); vram=b[o+0x400:o+0x400+0x18000]
    io=s['IO_0'][:0x400]
    return pal,vram,io
def rgb(c): return np.stack([(c&31)<<3,((c>>5)&31)<<3,((c>>10)&31)<<3],-1).astype(np.uint8)
def text_bg(pal,vram,cnt,hofs,vofs,W=240,H=160):
    cb=((cnt>>2)&3)*0x4000; sb=((cnt>>8)&31)*0x800; b8=(cnt>>7)&1; sz=(cnt>>14)&3
    mw=256*(2 if sz in(1,3) else 1); mh=256*(2 if sz in(2,3) else 1)
    out=np.zeros((H,W,4),np.uint8)
    prgb=rgb(pal.astype(np.uint32))
    for y in range(H):
        for x in range(W):
            px=(x+hofs)%mw; py=(y+vofs)%mh
            blk=(px//256)+(py//256)*(2 if mw==512 else 1)
            tx=(px%256)//8; ty=(py%256)//8
            e=struct.unpack_from('<H',vram,sb+blk*0x800+(ty*32+tx)*2)[0]
            t=e&0x3ff; fx=(px%8) if not e&0x400 else 7-(px%8); fy=(py%8) if not e&0x800 else 7-(py%8)
            if b8:
                a=cb+t*64+fy*8+fx
                if a>=0x10000: continue
                ci=vram[a]
                if ci: out[y,x,:3]=prgb[ci]; out[y,x,3]=255
            else:
                a=cb+t*32+fy*4+fx//2
                if a>=0x10000: continue
                v=vram[a]; ci=(v>>4) if fx&1 else (v&15)
                if ci: out[y,x,:3]=prgb[(e>>12)*16+ci]; out[y,x,3]=255
    return out
def affine_bg(pal,vram,io,cnt,W=240,H=160):
    h=lambda a: struct.unpack_from('<h',io,a)[0]
    pa,pb,pc,pd=h(0x20),h(0x22),h(0x24),h(0x26)
    x0=struct.unpack_from('<i',io,0x28)[0]<<4>>4; y0=struct.unpack_from('<i',io,0x2c)[0]<<4>>4
    cb=((cnt>>2)&3)*0x4000; sb=((cnt>>8)&31)*0x800; size=128<<((cnt>>14)&3); wrap=(cnt>>13)&1
    prgb=rgb(pal.astype(np.uint32)); out=np.zeros((H,W,4),np.uint8); n=size//8
    for y in range(H):
        for x in range(W):
            tx=(x0+pa*x+pb*y)>>8; ty=(y0+pc*x+pd*y)>>8
            if wrap: tx%=size; ty%=size
            elif not(0<=tx<size and 0<=ty<size): continue
            t=vram[sb+(ty//8)*n+tx//8]; ci=vram[cb+t*64+(ty%8)*8+tx%8]
            if ci: out[y,x,:3]=prgb[ci]; out[y,x,3]=255
    return out
def layers(path):
    pal,vram,io=mem(path); r=ioreg.regs(path); L={}
    for i in range(4):
        if not r['bg'][i]: continue
        if r['mode']==0 or (r['mode']==1 and i<2):
            L[i]=text_bg(pal,vram,r['cnt'][i],*r['ofs'][i])
        elif r['mode'] in (1,2) and i>=2:
            if i==2: L[i]=affine_bg(pal,vram,io,r['cnt'][i])
    return L,r,rgb(np.array([pal[0]],np.uint32))[0]
