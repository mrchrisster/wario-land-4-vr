import struct
def sections(path):
    st=open(path,'rb').read(); o=8+40; n=struct.unpack_from('<I',st,o)[0]; o+=4; out={}
    for _ in range(n):
        tag=st[o:o+4].decode('latin1'); ln=struct.unpack_from('<I',st,o+4)[0]; out[tag]=st[o+8:o+8+ln]; o+=8+ln
    return out
