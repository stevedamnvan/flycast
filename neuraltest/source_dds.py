"""DDS DX10 serialization of already-decoded source RGBA; no color conversion."""
import struct


def encode(levels, *, srgb):
    if type(srgb) is not bool or not 1<=len(levels)<=13:
        raise ValueError('explicit color format and bounded levels required')
    w,h=levels[0]['width'],levels[0]['height']
    if type(w) is not int or type(h) is not int or not (0<w<=4096 and 0<h<=4096):
        raise ValueError('DDS dimensions')
    payload=[];total=0
    for i,level in enumerate(levels):
        mw,mh=max(1,w>>i),max(1,h>>i)
        raw=level['rgba']
        if level['width']!=mw or level['height']!=mh or len(raw)!=mw*mh*4:
            raise ValueError('DDS mip chain/size')
        if i and max(w,h)>>(i-1)==1:
            raise ValueError('DDS excess mip')
        total+=len(raw)
        if total>64*1024*1024:raise ValueError('DDS byte bound')
        payload.append(raw)
    flags=0x100f | (0x20000 if len(levels)>1 else 0)
    caps=0x1000 | (0x400008 if len(levels)>1 else 0)
    header=[124,flags,h,w,w*4,0,len(levels)]+[0]*11
    header += [32,4,int.from_bytes(b'DX10','little'),0,0,0,0,0]
    header += [caps,0,0,0,0]
    # DXGI R8G8B8A8 UNORM(28)/SRGB(29), TEXTURE2D(3), one array item.
    return b'DDS '+struct.pack('<31I',*header)+struct.pack('<5I',29 if srgb else 28,3,0,1,0)+b''.join(payload)
