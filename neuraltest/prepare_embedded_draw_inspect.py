"""Prepare the witnessed large draw with explicit diagnostic coordinate provenance."""
import argparse
import json
from pathlib import Path
from prepare_remake_endpoint import bounded_json
from large_calibrated_scene_inspect import inspect_capture
from incarnated_scene_inspect import inspect_capture as incarnated
from compact_draw_inspect import FOUR_DOMAIN
from camera_embedding_inspect import embed
from capture_equivalence_inspect import verify
from source_dds import publish_capture
from remake_asset_join_inspect import join
from remake_scene_artifact_inspect import prepare


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('capture','tape','ledger','reference_capture','reference','normal_executable','output'):
        p.add_argument(key,type=Path)
    p.add_argument('--group',choices=('large','two','four'),default='large')
    p.add_argument('--frame',type=int,choices=(1782,1783,1784),default=1782)
    a=p.parse_args()
    if a.output.exists():p.error('new output required')
    if a.group=='large':
        evidence=inspect_capture(a.capture,a.tape,a.ledger,source_details=True)
        frame=evidence['frames'][a.frame-1782];draws=[277]
    else:
        extra=dict(domain=FOUR_DOMAIN,target_count=1005,affine_contributions=True) if a.group=='four' else {}
        evidence=incarnated(a.capture,a.tape,a.ledger,source_details=True,**extra)
        parts=evidence['frames'][a.frame-1782]['draws'];frame=dict(parts[0])
        frame['vertices']=[v for part in parts for v in part['vertices']]
        frame['triangles']=[t for part in parts for t in part['triangles']]
        frame['supported_triangles']=sum(part['supported_triangles'] for part in parts)
        frame['omitted_opaque_triangles']=frame['total_opaque_index_triangles']-frame['supported_triangles']
        draws=sorted({v['source_draw'] for v in frame['vertices']})
    source=a.capture/f'frame-{a.frame:06d}';target=a.reference_capture/f'frame-{a.frame:06d}'
    scene=bounded_json(source/'pvr-scene.json',64*1024*1024)
    materials=bounded_json(source/'materials/manifest.json',16*1024*1024)
    reference=bounded_json(a.reference,32*1024*1024)
    equivalence=verify(scene,materials,source/'materials',
        bounded_json(target/'pvr-scene.json',64*1024*1024),
        bounded_json(target/'materials/manifest.json',16*1024*1024),target/'materials',draws)
    if reference['git_sha']!=equivalence['reference_git_sha']:
        raise ValueError('reference capture revision mismatch')
    mesh=embed(frame,scene,reference)
    a.output.mkdir(exist_ok=False)
    publish_capture(scene,materials,draws,source/'materials',a.output/'assets')
    result=prepare(join(mesh,scene,materials,source/'materials',a.output/'assets'),a.normal_executable)
    result['capture_equivalence']=equivalence
    with (a.output/'scene.json').open('x',encoding='utf-8') as stream:
        json.dump(result,stream,sort_keys=True)
    print(json.dumps(dict(meshes=len(result['meshes']),vertices=sum(len(m['vertices']) for m in result['meshes']),
                         provenance=result['embedding_provenance'],runtime_loaded=False)))


if __name__=='__main__':main()
