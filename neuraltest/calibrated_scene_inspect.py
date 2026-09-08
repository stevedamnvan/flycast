"""Partial evidence mesh, never a complete/renderable game-scene claim."""
import argparse
import json
import math
from pathlib import Path
from binary32_oracle import fraction
from camera_transform_inspect import inspect_capture as transforms
from camera_calibration_inspect import inspect_capture as calibration
from transform_store_inspect import require


def build_frame(scene,samples,contract,full_draw=False):
    require(scene['schema']=='flycast-pvr-scene-v2','scene schema')
    count=142 if full_draw else 5
    require(len(samples)==count and len({s['vertex'] for s in samples})==count,'supported sample count')
    if full_draw:
        require({s['vertex'] for s in samples}==set(range(4,146))
                and all(s['draw']==1 for s in samples),'full draw domain')
    sx,sy=contract['normalized_calibration'];cx,cy=contract['screen_center']
    require(all(math.isfinite(v) and v>0 for v in (sx,sy)),'invalid calibration')
    points={};max_error=0
    for sample in samples:
        require(sample['producer']['ordinal']+1==scene['frame_id'],'frame association')
        vertex=sample['vertex'];require(0<=vertex<len(scene['vertices']),'vertex bounds')
        original=scene['vertices'][vertex]
        require(original[:3]==sample['final_words'],'source vertex disagreement')
        x,y,z=[float(fraction(w)) for w in sample['preprojection_record_words']]
        require(all(math.isfinite(v) for v in (x,y,z)) and z>0,'unsupported view depth')
        view=[x/sx,-y/sy,z]
        projected=[cx+sx*view[0]/view[2],cy-sy*view[1]/view[2]]
        expected=[float(fraction(w)) for w in original[:2]]
        error=max(abs(a-b) for a,b in zip(projected,expected));max_error=max(max_error,error)
        require(error<.001,'calibrated reprojection mismatch')
        points[vertex]=dict(source_vertex=vertex,source_draw=sample['draw'],position=view,
                            original_vertex=original,normal=None,normal_provenance='unknown')
    triangles=[];total=0
    require(len(scene['vertices'])<=65536 and len(scene['indices'])<=262144 and len(scene['draws'])<=4096,'scene bounds')
    for draw in scene['draws']:
        if draw['list']!=0:continue
        first,count=draw['first'],draw['count']
        require(first>=0 and count>=0 and first+count<=len(scene['indices']),'draw range')
        strip=scene['indices'][first:first+count]
        run=[]
        for position,vertex in enumerate(strip):
            if vertex==0xffffffff:
                run=[];continue
            require(isinstance(vertex,int) and 0<=vertex<len(scene['vertices']),'index bounds')
            run.append(vertex)
            if len(run)<3:continue
            i=len(run)-1;ids=run[-3:]
            if len(set(ids))<3:continue
            total+=1
            if not all(v in points and points[v]['source_draw']==draw['ordinal'] for v in ids):continue
            if i&1:ids=[ids[1],ids[0],ids[2]]
            triangles.append(dict(source_draw=draw['ordinal'],source_strip_offset=position-2,vertices=ids,original_draw=draw))
    require(triangles,'no fully supported primitive')
    return dict(schema='flycast-calibrated-evidence-scene-v1',frame_id=scene['frame_id'],game_id=scene['game_id'],
                coordinate_space='calibrated-camera-relative',calibration=contract,vertices=list(points.values()),triangles=triangles,
                total_opaque_index_triangles=total,supported_triangles=len(triangles),
                omitted_opaque_triangles=total-len(triangles),maximum_reprojection_error_pixels=max_error,
                complete_scene=False,renderable_by_remix_adapter=False,
                omissions=['unverified vertices and primitives','nonopaque geometry','game normals','world camera and scale','runtime material conversion'])


def inspect_capture(path,full_draw=False):
    verified=transforms(path,full_draw);contract=calibration(path,full_draw)
    frames=[]
    for ordinal in (1781,1782,1783):
        scene_path=path/f'frame-{ordinal+1:06d}'/'pvr-scene.json'
        require(scene_path.stat().st_size<=64*1024*1024,'scene file bound')
        scene=json.loads(scene_path.read_text())
        frames.append(build_frame(scene,[s for s in verified['samples'] if s['producer']['ordinal']==ordinal],contract,full_draw))
    return dict(frames=frames,production_enabled=False,combined_pipeline_proven=False)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('capture',type=Path)
    parser.add_argument('--full-draw',action='store_true')
    args=parser.parse_args();result=inspect_capture(args.capture,args.full_draw)
    print(json.dumps([{k:v for k,v in f.items() if k not in ('vertices','triangles','calibration')} for f in result['frames']],indent=2))
