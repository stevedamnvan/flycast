"""Bounded expression-derived geometry; strict failures remain in the artifact."""
import math
from transform_store_inspect import require
from draw_domain_inspect import inspect_scene


def build(scene, rows, matrix_contract):
    require(scene['schema']=='flycast-pvr-scene-v2','scene schema')
    require(0<len(rows)<=4096 and len(scene['vertices'])<=65536,'mesh vertex bounds')
    require('rejected' not in matrix_contract and matrix_contract.get('doubled_calibration_rejected') is True,
            'matrix witnesses required')
    vertices={}
    for row in rows:
        vertex=row['vertex'];d=row['diagnostic'];position=d['position']
        require(vertex not in vertices and 0<=vertex<len(scene['vertices']),'mesh vertex identity')
        require(len(position)==3 and all(math.isfinite(v) for v in position) and position[2]>0,'mesh position')
        require(row['error']==d['maximum_reprojection_error_pixels'],'mesh residual identity')
        vertices[vertex]=dict(source_vertex=vertex,source_draw=row['draw'],position=position,
            original_vertex=scene['vertices'][vertex],record_identity=row['record'],
            normal=None,normal_provenance='unknown',reprojection_error_pixels=row['error'])
    triangles=[]
    for draw in sorted({r['draw'] for r in rows}):
        for tri in inspect_scene(scene,draw,4096,8192)['triangles']:
            if all(v in vertices and vertices[v]['source_draw']==draw for v in tri):
                triangles.append(dict(source_draw=draw,vertices=tri))
    require(triangles,'no supported triangles')
    failures=[v for v in vertices if vertices[v]['reprojection_error_pixels']>=.001]
    return dict(schema='flycast-expression-evidence-mesh-v1',frame_id=scene['frame_id'],game_id=scene['game_id'],
        coordinate_space='calibrated-camera-relative',vertices=list(vertices.values()),triangles=triangles,
        calibration=matrix_contract,strict_reprojection_failures=failures,strict_tolerance_pixels=.001,
        strict_reprojection_pass=not failures,world_camera_recovered=False,complete_scene=False,
        renderable_by_remix_adapter=False,production_enabled=False,
        omissions=['world camera and physical scale','background','nonopaque geometry','normals and material conversion',
                   'expression ancestry outside this single frame','offscreen culled game geometry'])
