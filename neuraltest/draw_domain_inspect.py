"""Bound the next complete-draw capture from real scene topology, not address guesses."""
import argparse
import json
from pathlib import Path
from transform_store_inspect import require


def inspect_scene(scene,ordinal=1,vertex_limit=256,index_limit=4096):
    require(type(vertex_limit) is int and 1<=vertex_limit<=4096
            and type(index_limit) is int and 3<=index_limit<=8192,'invalid domain budget')
    draws=[d for d in scene['draws'] if d['list']==0 and d['ordinal']==ordinal]
    require(len(draws)==1,'draw identity')
    draw=draws[0];first,count=draw['first'],draw['count']
    require(draw['range_space']=='indices' and first>=0 and 0<count<=index_limit
            and first+count<=len(scene['indices']),'index range')
    strip=scene['indices'][first:first+count];vertices=set();triangles=[];run=[];restarts=0
    for index,v in enumerate(strip):
        if v==0xffffffff:
            run=[];restarts+=1;continue
        require(isinstance(v,int) and 0<=v<len(scene['vertices']),'vertex index')
        require(len(scene['vertices'][v])>=3 and all(type(w) is int and 0<=w<=0xffffffff for w in scene['vertices'][v][:3]),'position words')
        vertices.add(v);require(len(vertices)<=vertex_limit,'vertex budget')
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


def inspect_capture(path,ordinal=1,vertex_limit=256,index_limit=4096):
    frames=[]
    for number in (1782,1783,1784):
        p=path/f'frame-{number:06d}'/'pvr-scene.json'
        require(p.stat().st_size<=64*1024*1024,'scene byte budget')
        scene=json.loads(p.read_text());require(scene['frame_id']==number,'frame identity')
        frames.append(inspect_scene(scene,ordinal,vertex_limit,index_limit))
    for frame in frames[1:]:
        require(all(frame[key]==frames[0][key] for key in ('game_id','vertices','index_stream','triangles','original_draw')),'draw domain changed')
    return frames


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('capture',type=Path)
    parser.add_argument('--draw',type=int,default=1)
    parser.add_argument('--vertex-limit',type=int,default=256)
    parser.add_argument('--index-limit',type=int,default=4096)
    args=parser.parse_args()
    print(json.dumps([{k:v for k,v in frame.items() if k not in ('vertices','index_stream','triangles','original_draw')}
                      for frame in inspect_capture(args.capture,args.draw,args.vertex_limit,args.index_limit)],indent=2))
