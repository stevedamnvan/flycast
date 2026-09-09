"""Bounded diagnostic triangle tile error; no runtime or full PVR acceptance."""
import argparse
import hashlib
import json
import struct
from pathlib import Path
from source_attribute_sample import sample, triangle_sample


def weights(u, v):
    # Extend boundary texels to the simplex; do not wrap another triangle in.
    values=[max(0,1-u-v),max(0,u),max(0,v)]
    total=sum(values)
    return [x/total for x in values]


def tile(vertices, rgba, width, height, tsp, size):
    if type(size) is not int or size not in (8,16,32,64):
        raise ValueError('bounded tile size')
    pixels=bytearray()
    for y in range(size):
        for x in range(size):
            value=triangle_sample(vertices,weights((x+.5)/size,(y+.5)/size),rgba,width,height,tsp)
            pixels.extend(min(255,max(0,round(c))) for c in value)
    return bytes(pixels)


def error(vertices, rgba, width, height, tsp, size):
    pixels=tile(vertices,rgba,width,height,tsp,size)
    errors=[]
    # Subtexel/off-lattice probes, including near all three edges.
    for y in range(12):
        for x in range(12-y):
            u,v=(x+.17)/12,(y+.31)/12
            truth=triangle_sample(vertices,[1-u-v,u,v],rgba,width,height,tsp)
            approximated=sample(pixels,size,size,u,v,8192|(1<<15)|(1<<16))
            errors.extend(abs(a-b) for a,b in zip(truth[:3],approximated[:3]))
    return dict(mae=sum(errors)/len(errors),maximum=max(errors),samples=len(errors),bytes=len(pixels))


def inspect(root, draws, count=8):
    if not 1<=count<=16 or not 1<=len(draws)<=8:
        raise ValueError('inspection work bound')
    scene=json.loads((root/'scene.json').read_text())
    result=[]
    for draw in draws:
        matches=[m for m in scene['meshes'] if m['source_draw']==draw]
        if len(matches)!=1:raise ValueError('unique draw required')
        mesh=matches[0];binding=mesh['source_bindings'][0]
        metadata=scene['source_assets'][str(binding['asset'])]
        path=root/'assets'/metadata['file']
        if path.resolve().parent!=(root/'assets').resolve() or path.stat().st_size>64*1024*1024:
            raise ValueError('asset path/size')
        data=path.read_bytes()
        if len(data)<148 or data[:4]!=b'DDS ' or hashlib.sha256(data).hexdigest()!=metadata['sha256'] or struct.unpack_from('<I',data,128)[0]!=28:
            raise ValueError('linear source DDS required')
        height,width=struct.unpack_from('<2I',data,12)
        rgba=data[148:148+width*height*4]
        total=len(mesh['indices'])//3
        selected=sorted(set(round(i*(total-1)/max(1,count-1)) for i in range(min(count,total))))
        for size in (8,16,32,64):
            rows=[]
            for index in selected:
                vertices=[mesh['vertices'][i] for i in mesh['indices'][index*3:index*3+3]]
                rows.append(error(vertices,rgba,width,height,binding['tsp'],size))
            result.append(dict(draw=draw,size=size,triangles=selected,
                               mean_mae=sum(r['mae'] for r in rows)/len(rows),
                               maximum=max(r['maximum'] for r in rows),
                               full_draw_bytes=total*size*size*4))
    return dict(diagnostic_only=True,mip_zero_only=True,gpu_proven=False,rows=result)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root',type=Path)
    parser.add_argument('--draw',type=int,action='append',required=True)
    args=parser.parse_args()
    print(json.dumps(inspect(args.root,args.draw),sort_keys=True))
