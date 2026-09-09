"""Bounded prepared scene artifact; never upgrades sampled camera acceptance."""
import math
import subprocess
from transform_store_inspect import require


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
