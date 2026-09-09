"""Content equivalence across diagnostic revisions; never erase source revisions."""
import hashlib
import json
from source_dds import capture_bundle
from transform_store_inspect import require


def scene_equivalence(a,b):
    require(a.get('schema')==b.get('schema')=='flycast-pvr-scene-v2','scene schema')
    require(all(isinstance(s.get('git_sha'),str) and s['git_sha'] for s in (a,b)), 'source revisions')
    left={k:v for k,v in a.items() if k!='git_sha'}
    right={k:v for k,v in b.items() if k!='git_sha'}
    require(left==right,'scene content differs')
    return dict(source_git_sha=a['git_sha'],reference_git_sha=b['git_sha'],
                scene_content_sha256=hashlib.sha256(json.dumps(left,sort_keys=True).encode()).hexdigest(),
                frame_id=a['frame_id'],game_id=a['game_id'],complete_pipeline=False)


def verify(a,am,aroot,b,bm,broot,draws):
    result=scene_equivalence(a,b)
    for scene,materials in ((a,am),(b,bm)):
        require(materials['git_sha']==materials['scene_sha']==scene['git_sha'],'material revision')
    clean=lambda m:{k:v for k,v in m.items() if k not in ('git_sha','scene_sha')}
    require(clean(am)==clean(bm),'material metadata differs')
    first=capture_bundle(a,am,draws,aroot)
    second=capture_bundle(b,bm,draws,broot)
    require({k:v for k,v in first.items() if k!='git_sha'}==
            {k:v for k,v in second.items() if k!='git_sha'},'verified source texture bytes differ')
    return dict(result,draws=list(draws),asset_sha256={str(k):v['sha256'] for k,v in first['assets'].items()},
                diagnostic_content_equivalence=True)
