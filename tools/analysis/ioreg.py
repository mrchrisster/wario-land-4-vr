import struct, sys, gbas
def regs(path):
    io=gbas.sections(path)['IO_0'][:0x400]
    h=lambda a: struct.unpack_from('<H',io,a)[0]
    d=h(0); r={'mode':d&7,'bg':[(d>>(8+i))&1 for i in range(4)],'obj':(d>>12)&1,'win':(d>>13)&7,'blank':(d>>7)&1}
    r['cnt']=[h(8+2*i) for i in range(4)]
    r['prio']=[c&3 for c in r['cnt']]; r['bpp8']=[(c>>7)&1 for c in r['cnt']]; r['size']=[(c>>14)&3 for c in r['cnt']]
    r['ofs']=[(h(0x10+4*i)&0x1ff,h(0x12+4*i)&0x1ff) for i in range(4)]
    r['bld']=h(0x50); r['alpha']=h(0x52); r['bldy']=h(0x54)&31; r['mosaic']=h(0x4c)
    r['bg2pa']=h(0x20)
    return r
def line(f,r):
    bgs=' '.join(f"BG{i}:p{r['prio'][i]}{'/8bpp' if r['bpp8'][i] else ''}@{r['ofs'][i][0]},{r['ofs'][i][1]}" for i in range(4) if r['bg'][i])
    bm=r['bld']; mode={0:'',1:'alpha',2:'bright+',3:'bright-'}[(bm>>6)&3]
    tgt=lambda m:''.join(n for i,n in enumerate(['0','1','2','3','O','B']) if m>>i&1)
    bl=f" blend={mode} A[{tgt(bm&63)}] B[{tgt(bm>>8&63)}] eva/evb={r['alpha']&31}/{r['alpha']>>8&31} y={r['bldy']}" if mode else ''
    return f"f{f:5d} mode{r['mode']} {bgs} obj={r['obj']} win={r['win']}{bl}{' MOSAIC' if r['mosaic'] else ''}{' FBLANK' if r['blank'] else ''}"
if __name__=='__main__':
    for p in sys.argv[1:]:
        f=int(p.split('/f')[-1].split('/')[0]); print(line(f,regs(p)))
