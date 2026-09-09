"""Compose explicitly mixed diagnostic coordinates with per-group provenance."""
import argparse
import copy
import json
from pathlib import Path
from prepare_remake_endpoint import bounded_json
from remake_batch_join_inspect import combine, publish_verified


def compose(base, additions):
    if base['coordinate_space']!='reflected-selected-source-anchor' or not 1<=len(additions)<=3:
        raise ValueError('bounded base and additions required')
    result=copy.deepcopy(base)
    groups=[dict(source_git_sha=base['git_sha'],coordinate_space=base['coordinate_space'],
                 draws=[m['source_draw'] for m in base['meshes']])]
    digest=None
    for scene in additions:
        e=scene['embedding_provenance'];eq=scene['capture_equivalence']
        draws=[m['source_draw'] for m in scene['meshes']]
        if (scene['coordinate_space']!='diagnostic-camera-embedded-anchor'
            or e['recovered_world_transform'] is not False
            or e.get('source_coordinate_space')!='calibrated-camera-relative'
            or e['source_git_sha']!=scene['git_sha'] or e['reference_git_sha']!=base['git_sha']
            or eq['source_git_sha']!=scene['git_sha'] or eq['reference_git_sha']!=base['git_sha']
            or eq['frame_id']!=base['frame_id'] or eq['game_id']!=base['game_id']
            or eq['diagnostic_content_equivalence'] is not True or sorted(eq['draws'])!=sorted(draws)):
            raise ValueError('group provenance mismatch')
        if digest is not None and digest!=eq['scene_content_sha256']:
            raise ValueError('different reference content')
        digest=eq['scene_content_sha256']
        if not isinstance(digest,str) or len(digest)!=64 or any(c not in '0123456789abcdef' for c in digest):
            raise ValueError('scene content digest')
        if set(eq['asset_sha256'])!=set(scene['source_assets']):
            raise ValueError('equivalent asset coverage')
        for key,value in eq['asset_sha256'].items():
            if scene['source_assets'][key]['sha256']!=value:
                raise ValueError('equivalent asset mismatch')
        groups.append(dict(source_git_sha=scene['git_sha'],coordinate_space=scene['coordinate_space'],
                           draws=draws,embedding_provenance=e,capture_equivalence=eq))
        # Reuse geometry/camera/asset conflict checks in one reference coordinate
        # system; retain the original identities in the explicit mixed result.
        normalized=copy.deepcopy(scene)
        normalized['git_sha']=base['git_sha'];normalized['coordinate_space']=base['coordinate_space']
        result=combine(result,normalized)
    result['coordinate_space']='mixed-diagnostic-anchor'
    result['source_groups']=groups
    result['omissions'].append('mixed diagnostic embedding is not recovered world geometry')
    return result


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('base',type=Path);p.add_argument('output',type=Path)
    p.add_argument('additions',nargs='+',type=Path)
    a=p.parse_args();roots=[a.base,*a.additions]
    scenes=[bounded_json(root/'scene.json',32*1024*1024) for root in roots]
    result=compose(scenes[0],scenes[1:])
    publish_verified(result,[(scene,root/'assets') for scene,root in zip(scenes,roots)],a.output)
    print(json.dumps(dict(meshes=len(result['meshes']),groups=len(result['source_groups']),complete_scene=False)))
