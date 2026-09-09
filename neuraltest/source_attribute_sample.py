"""Bounded mip-zero RGBA sampling for diagnostic triangle-attribute baking.

Not full PVR shading: fog, offset color, mip selection and external material
color management are not reproduced. No image or source asset is rewritten.
"""
import math
import struct


def address(index, size, clamp, mirror):
    if clamp:
        return min(size-1, max(0, index))
    if mirror:
        index %= 2*size
        return index if index < size else 2*size-1-index
    return index % size


def sample(rgba, width, height, u, v, tsp):
    if (type(width) is not int or type(height) is not int or
            not 0 < width <= 4096 or not 0 < height <= 4096 or
            len(rgba) != width*height*4 or not math.isfinite(u) or not math.isfinite(v) or
            type(tsp) is not int or not 0 <= tsp <= 0xffffffff):
        raise ValueError('invalid bounded sample')
    filtering = (tsp >> 13) & 3
    if filtering > 1:
        raise ValueError('trilinear mip selection unsupported')
    def texel(x, y):
        x = address(x, width, tsp & (1 << 16), tsp & (1 << 18))
        y = address(y, height, tsp & (1 << 15), tsp & (1 << 17))
        start = (y*width+x)*4
        return rgba[start:start+4]
    if not filtering:
        return tuple(texel(math.floor(u*width), math.floor(v*height)))
    x, y = u*width-.5, v*height-.5
    ix, iy = math.floor(x), math.floor(y)
    fx, fy = x-ix, y-iy
    taps = (texel(ix, iy), texel(ix+1, iy), texel(ix, iy+1), texel(ix+1, iy+1))
    return tuple((1-fy)*((1-fx)*taps[0][c]+fx*taps[1][c])+
                 fy*((1-fx)*taps[2][c]+fx*taps[3][c]) for c in range(4))


def triangle_sample(vertices, weights, rgba, width, height, tsp):
    if (len(vertices) != 3 or len(weights) != 3 or
            any(not math.isfinite(w) or w < 0 or w > 1 for w in weights) or
            abs(sum(weights)-1) > 1e-10):
        raise ValueError('triangle barycentrics required')
    uv, colors = [], []
    for vertex in vertices:
        raw = vertex['original_vertex']
        if len(raw) != 11 or any(type(b) is not int or not 0 <= b <= 255 for b in raw[5]) or len(raw[5]) != 4:
            raise ValueError('source vertex layout')
        uv.append(struct.unpack('<2f', struct.pack('<2I', *raw[3:5])))
        colors.append((raw[5][2], raw[5][1], raw[5][0], raw[5][3]))
    u, v = [sum(weights[i]*uv[i][axis] for i in range(3)) for axis in range(2)]
    texture = sample(rgba, width, height, u, v, tsp)
    return tuple(texture[c]*sum(weights[i]*colors[i][c] for i in range(3))/255 for c in range(4))


if __name__ == '__main__':
    import argparse
    import hashlib
    import json
    from pathlib import Path
    from retained_topology_inspect import inspect as topology_inspect
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('first',type=Path)
    parser.add_argument('second',type=Path)
    args=parser.parse_args()
    scenes=[json.loads((root/'scene.json').read_text()) for root in (args.first,args.second)]
    if not all(row['topology_equal'] for row in topology_inspect(*scenes)['meshes']):
        raise ValueError('source topology mismatch')
    def centroids(scene,root):
        assets={}
        for key,metadata in scene['source_assets'].items():
            path=root/'assets'/metadata['file']
            if path.resolve().parent != (root/'assets').resolve() or path.stat().st_size > 64*1024*1024:
                raise ValueError('asset path/size')
            data=path.read_bytes()
            if hashlib.sha256(data).hexdigest()!=metadata['sha256'] or len(data)<148 or data[:4]!=b'DDS ':
                raise ValueError('asset hash/format')
            if struct.unpack_from('<I',data,128)[0]!=28:
                raise ValueError('linear RGBA required')
            height,width=struct.unpack_from('<2I',data,12)
            assets[int(key)]=(data[148:148+width*height*4],width,height)
        values={}
        for mesh in scene['meshes']:
            binding=mesh['source_bindings'][0]
            pixels,width,height=assets[binding['asset']]
            for offset in range(0,len(mesh['indices']),3):
                vertices=[mesh['vertices'][i] for i in mesh['indices'][offset:offset+3]]
                values[mesh['source_draw'],offset//3]=triangle_sample(vertices,[1/3]*3,pixels,width,height,binding['tsp'])
        return values
    old,new=[centroids(scene,root) for scene,root in zip(scenes,(args.first,args.second))]
    deltas=[max(abs(a-b) for a,b in zip(old[key],new[key])) for key in old]
    print(json.dumps(dict(diagnostic_only=True,mip_level=0,full_pvr_shading=False,
                          triangles=len(deltas),changed_centroids=sum(d>1e-6 for d in deltas),
                          max_channel_delta=max(deltas),mean_max_channel_delta=sum(deltas)/len(deltas))))
