"""Join witnessed geometry to verified source files; not a renderable game scene."""
from source_dds import verify_published
from transform_store_inspect import require
from draw_domain_inspect import inspect_scene


def join(mesh, scene, materials, directory, output):
    require(mesh.get('schema') in ('flycast-expression-evidence-mesh-v1','flycast-calibrated-evidence-scene-v1'),'mesh schema')
    for key in ('frame_id','game_id','git_sha'):
        require(mesh.get(key)==scene[key],'mesh source '+key)
    vertices=mesh['vertices'];triangles=mesh['triangles']
    require(0<len(vertices)<=65536 and 0<len(triangles)<=262144//3,'mesh bounds')
    by_id={v['source_vertex']:v for v in vertices}
    require(len(by_id)==len(vertices),'duplicate mesh vertex')
    draws=sorted({v['source_draw'] for v in vertices})
    require(0<len(draws)<=128,'mesh draw bound')
    for vertex in vertices:
        index=vertex['source_vertex']
        require(type(index) is int and 0<=index<len(scene['vertices']),'mesh source vertex')
        require(vertex['original_vertex']==scene['vertices'][index],'mesh source attributes')
    source_triangles={d:{tuple(t) for t in inspect_scene(scene,d,4096,8192)['triangles']} for d in draws}
    grouped={d:[] for d in draws}
    for triangle in triangles:
        d=triangle['source_draw'];indices=triangle['vertices']
        require(d in grouped and len(indices)==3 and all(i in by_id and by_id[i]['source_draw']==d for i in indices),
                'mesh triangle ownership')
        # Orientation conversion explicitly reverses winding, not arbitrary topology.
        original=tuple([indices[0],indices[2],indices[1]] if mesh.get('winding_reversed') is True else indices)
        require(original in source_triangles[d],'mesh source topology')
        grouped[d].append(dict(triangle))
    published=verify_published(scene,materials,draws,directory,output)
    packets=[]
    for d in draws:
        bindings=[b for b in published['bindings'] if b['draw']==d]
        packets.append(dict(source_draw=d,vertices=[dict(v) for v in vertices if v['source_draw']==d],
                            triangles=grouped[d],source_bindings=bindings))
    return dict(mesh,draw_packets=packets,source_assets=published['assets'],
                source_publication_verified=True,renderable_by_remix_adapter=False,
                material_semantic='source-color-not-physical-albedo')
