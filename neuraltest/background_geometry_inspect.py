"""Validate FillBGP geometry construction; no world-camera or material claim."""
import json,re,struct
import numpy as np
from transform_store_inspect import load_session,require


def f(word):return np.float32(struct.unpack('<f',struct.pack('<I',word))[0])
def bits(value):return struct.unpack('<I',struct.pack('<f',value))[0]


def construct(raw,depth,textured,hscale):
    require(len(raw)==9 and textured in (0,1) and hscale in (0,1),'background shape')
    v=np.array([f(w) for w in raw],dtype=np.float32).reshape(3,3)
    require(np.isfinite(v).all() and np.isfinite(f(depth)),'background finite')
    z=max(np.float32(f(depth)-np.float32(1e-6)),np.float32(1e-11))
    scale=np.float32(2 if hscale else 1)
    if not textured:
        v=np.array([[-256*scale,0,z],[896*scale,0,z],[-256*scale,480,z]],dtype=np.float32)
    else:
        if v[2,0]==v[1,0]:v[2,0]=v[0,0]
        v[0,0]=np.float32(v[0,0]-256)*scale
        v[1,0]=np.float32(v[1,0]+256)*scale
        v[2,0]=np.float32(v[2,0]-256)*scale
        v[:,2]=z
    fourth=v[2].copy();fourth[0]=v[1,0]
    return [[bits(x) for x in row] for row in [*v,fourth]]


def inspect(text,scenes):
    require(len(text.encode('utf-8'))<=16*1024*1024,'background log bound')
    require('REJECT' not in text,'background capture rejected')
    inputs={};outputs={};bindings={}
    for line in text.splitlines():
        if 'FC067_BGP_FRAME_' not in line:continue
        r=dict(re.findall(r'(\w+)=([^ ]+)',line));n=int(r['ordinal'])
        require(1781<=n<=1783,'background ordinal')
        if 'BGP_FRAME_BIND' in line:
            require(n not in bindings,'duplicate background binding');bindings[n]=r;continue
        r['xyz']=[int(x,16) for x in r['xyz'].split(',')]
        if 'BGP_FRAME_INPUT' in line:
            require(n not in inputs,'duplicate background input');inputs[n]=r
        elif 'BGP_FRAME_OUTPUT' in line:
            key=(n,int(r['vertex']));require(key not in outputs,'duplicate background output');outputs[key]=r['xyz']
    require(len(inputs)==3 and len(outputs)==12 and len(bindings)==3,'complete background records')
    for n,r in inputs.items():
        require(all(r[k]==bindings[n][k] for k in ('generation','context')),'background queue binding')
        expected=construct(r['xyz'],int(r['depth'],16),int(r['texture']),int(r['hscale']))
        scene=scenes[n];require(scene['frame_id']==n+1 and scene['game_id']=='T1401N','scene identity')
        for i in range(4):
            require(outputs.get((n,i))==expected[i],'background arithmetic')
            require(scene['vertices'][i][:3]==expected[i],'background scene binding')
    return dict(frames=3,vertices_per_frame=4,geometry_construction_proven=True,
                material_provenance_proven=False,world_camera_recovered=False)


def inspect_capture(capture):
    scenes={}
    for n in (1781,1782,1783):
        path=capture/f'frame-{n+1:06d}'/'pvr-scene.json'
        require(path.stat().st_size<=64*1024*1024,'scene bound')
        scenes[n]=json.loads(path.read_text())
    return inspect(load_session(capture),scenes)


if __name__=='__main__':
    import argparse
    from pathlib import Path
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('capture',type=Path)
    print(json.dumps(inspect_capture(p.parse_args().capture),sort_keys=True))
