"""Exact remaining opaque topology batches, not source-transform evidence."""
import json
from compact_draw_inspect import inspect_capture,selection
from draw_domain_inspect import inspect_scene
from transform_store_inspect import require

BATCHES=((1114,70,2180,2462,2771,1071,3014,2410,2715,1,2660,44,2246,2923,2378,26,2990),
         (2946,2838,2966,2890,2570,2874,2697,2858,2758,2522,2650,2371,2915,2648,2647,0,2914))


def domain(capture,batch):
    require(type(batch) is int and batch in (1,2),'remaining batch')
    result=[]
    for n in (1782,1783,1784):
        path=capture/f'frame-{n:06d}'/'pvr-scene.json'
        require(path.stat().st_size<=64*1024*1024,'scene byte budget')
        scene=json.loads(path.read_text());require(scene['frame_id']==n and scene['game_id']=='T1401N','source scene')
        result.append(tuple((d,tuple(inspect_scene(scene,d,4096,8192)['vertices'])) for d in BATCHES[batch-1]))
    require(result[0]==result[1]==result[2],'remaining topology changed')
    require(len(selection(result[0]))==(3682 if batch==1 else 924),'remaining vertex count')
    return result[0]


def packet_domain(full):
    background=[vertices for draw,vertices in full if draw==0]
    require(background==[(0,1,2,3)],'explicit background domain')
    result=tuple((draw,vertices) for draw,vertices in full if draw!=0)
    require(len(result)==16 and len(selection(result))==920,'remaining packet domain')
    return result


if __name__=='__main__':
    import argparse
    from pathlib import Path
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('topology',type=Path);p.add_argument('capture',type=Path);p.add_argument('tape',type=Path)
    p.add_argument('--batch',type=int,choices=(1,2),required=True)
    p.add_argument('--packet-only',action='store_true',help='Batch2 only; background provenance remains separately required')
    a=p.parse_args();selected=domain(a.topology,a.batch)
    if a.packet_only:
        require(a.batch==2,'packet-only batch2');selected=packet_domain(selected)
    result=inspect_capture(a.capture,a.tape,selected)
    result['omitted_background_vertices']=4 if a.packet_only else 0
    result['background_provenance_proven']=False
    print(json.dumps(result,sort_keys=True))
