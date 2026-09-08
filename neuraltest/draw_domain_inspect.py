"""Bound the next complete-draw capture from real scene topology, not address guesses."""
import argparse
import json
from pathlib import Path
from transform_store_inspect import require


def inspect_scene(scene,ordinal=1):
    draws=[d for d in scene['draws'] if d['list']==0 and d['ordinal']==ordinal]
    require(len(draws)==1,'draw identity')
    draw=draws[0];first,count=draw['first'],draw['count']
    require(draw['range_space']=='indices' and first>=0 and 0<count<=4096
            and first+count<=len(scene['indices']),'index range')
    strip=scene['indices'][first:first+count];vertices=set();triangles=[];run=[];restarts=0
    for index,v in enumerate(strip):
        if v==0xffffffff:
            run=[];restarts+=1;continue
        require(isinstance(v,int) and 0<=v<len(scene['vertices']),'vertex index')
        require(all(isinstance(w,int) and 0<=w<=0xffffffff for w in scene['vertices'][v][:3]),'position words')
        vertices.add(v);require(len(vertices)<=256,'vertex budget')
        run.append(v)
        if len(run)<3 or len(set(run[-3:]))<3:continue
        t=run[-3:]
        if (len(run)-1)&1:t=[t[1],t[0],t[2]]
        triangles.append(t)
    require(triangles,'empty draw')
    return dict(frame_id=scene['frame_id'],game_id=scene['game_id'],draw_ordinal=ordinal,
                vertices=sorted(vertices),index_stream=strip,triangles=triangles,restarts=restarts,
                unique_vertices=len(vertices),index_count=count,triangle_count=len(triangles),
                original_draw=draw,original_transform_coverage_proven=False)


def inspect_capture(path):
    frames=[]
    for number in (1782,1783,1784):
        p=path/f'frame-{number:06d}'/'pvr-scene.json'
        require(p.stat().st_size<=64*1024*1024,'scene byte budget')
        scene=json.loads(p.read_text());require(scene['frame_id']==number,'frame identity')
        frames.append(inspect_scene(scene))
    for frame in frames[1:]:
        require(all(frame[key]==frames[0][key] for key in ('game_id','vertices','index_stream','triangles','original_draw')),'draw domain changed')
    return frames


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('capture',type=Path)
    args=parser.parse_args()
    print(json.dumps([{k:v for k,v in frame.items() if k not in ('vertices','index_stream','triangles','original_draw')}
                      for frame in inspect_capture(args.capture)],indent=2))
