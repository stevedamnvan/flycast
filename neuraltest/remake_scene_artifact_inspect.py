"""Bounded prepared scene artifact; never upgrades sampled camera acceptance."""
import math
import subprocess
from transform_store_inspect import require


def diagnostic_clips(artifact, near, far):
    """Check a declared diagnostic interval; never assign recovered game clips."""
    require(artifact.get('schema')=='flycast-prepared-remake-scene-v1','prepared schema')
    require(all(type(x) in (int,float) and math.isfinite(x) for x in (near,far))
            and 0<near<far,'diagnostic clip interval')
    camera=artifact['camera'];origin=camera['position'];forward=camera['forward']
    require(len(origin)==len(forward)==3 and all(math.isfinite(x) for x in [*origin,*forward])
            and abs(sum(x*x for x in forward)-1)<1e-5,'diagnostic camera basis')
    meshes=artifact['meshes'];require(0<len(meshes)<=128,'diagnostic mesh bound')
    count=sum(len(m['vertices']) for m in meshes);require(0<count<=65536,'diagnostic vertex bound')
    depths=[];excluded=[]
    for mesh in meshes:
        for index,vertex in enumerate(mesh['vertices']):
            p=vertex['position'];require(len(p)==3 and all(math.isfinite(x) for x in p),'diagnostic position')
            z=sum((p[i]-origin[i])*forward[i] for i in range(3))
            require(math.isfinite(z),'diagnostic depth')
            depths.append(z)
            if not near<=z<=far:excluded.append(dict(draw=mesh['source_draw'],vertex=index,depth=z))
    return dict(provenance='caller-supplied-diagnostic-only',near=near,far=far,
                minimum=min(depths),maximum=max(depths),vertices=count,excluded=excluded,
                encloses_submitted_vertices=not excluded,game_clips_recovered=False,
                renderable_by_remix_adapter=False)


def prepare(joined, normal_executable):
    require(joined.get('source_publication_verified') is True,'verified publication required')
    require(joined.get('coordinate_space')=='reflected-selected-source-anchor','converted anchor required')
    require(joined.get('winding_reversed') is True,'converted winding required')
    require('harness_camera' in joined and 'fixed_origin' in joined,'fixed sequence camera required')
    packets=joined['draw_packets'];require(0<len(packets)<=128,'draw count')
    total=sum(len(p['triangles'])*3 for p in packets)
    require(0<total<=65536,'split vertex bound')
    meshes=[]
    for packet in packets:
        vertices=packet['vertices'];mapping={v['source_vertex']:i for i,v in enumerate(vertices)}
        require(len(mapping)==len(vertices) and 0<len(vertices)<=65536,'vertex mapping')
        indices=[mapping[i] for t in packet['triangles'] for i in t['vertices']]
        positions=[x for v in vertices for x in v['position']]
        require(len(positions)==3*len(vertices) and all(math.isfinite(x) for x in positions),'finite positions')
        values=[len(vertices),len(indices),*positions,*indices]
        result=subprocess.run([str(normal_executable),'--flat-normals'],input=' '.join(map(str,values)),
                              text=True,capture_output=True,timeout=30,check=True)
        lines=result.stdout.splitlines();count,omitted=map(int,lines[0].split())
        # Until omission-to-source mapping exists, never silently drop a face.
        require(omitted==0 and count==len(indices),'normal omissions unsupported')
        normals=[list(map(float,line.split())) for line in lines[1:]]
        require(len(normals)==len(indices)//3 and all(len(n)==3 and all(math.isfinite(x) for x in n)
                and abs(sum(x*x for x in n)-1)<1e-5 for n in normals),'normal output')
        split=[]
        for slot,index in enumerate(indices):
            vertex=vertices[index]
            split.append(dict(vertex,normal=normals[slot//3],normal_provenance='geometry-derived-flat'))
        meshes.append(dict(source_draw=packet['source_draw'],vertices=split,indices=list(range(len(split))),
                           source_bindings=packet['source_bindings']))
    return dict(schema='flycast-prepared-remake-scene-v1',frame_id=joined['frame_id'],
        game_id=joined['game_id'],git_sha=joined['git_sha'],coordinate_space=joined['coordinate_space'],
        camera=dict(joined['harness_camera'],nearPlane=None,farPlane=None,accepted_game_camera=False),
        fixed_origin=joined['fixed_origin'],meshes=meshes,source_assets=joined['source_assets'],
        omissions=list(joined['omissions']),strict_reprojection_pass=joined['strict_reprojection_pass'],
        renderable_by_remix_adapter=False,material_semantic='source-color-not-physical-albedo')
