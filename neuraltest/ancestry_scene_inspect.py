"""Same-capture ordered-expression acceptance for selected alternate geometry."""
import argparse
import json
import re
from pathlib import Path
from producer_ancestry_inspect import inspect as ancestry
from producer_record_inspect import inspect as records
from producer_ownership_inspect import bind_records, bind_tape, verify_memory, verify_copies
from compact_consumer_inspect import decode as decode_tape
from compact_draw_inspect import inspect_capture
from remaining_draw_inspect import domain, packet_domain
from draw_domain_inspect import inspect_scene
from transform_ledger_inspect import decode
from transform_store_inspect import require
from expression_camera_inspect import recover, outside_common_plane


def inspect(capture,tape_path,ledger,topology,calibration=None,batch=1,ordinal=1781,source_details=False):
    require(ordinal in (1781,1782,1783),'producer ordinal bounds')
    selected_domain=domain(topology,batch)
    if batch==2:selected_domain=packet_domain(selected_domain)
    scene_binding=inspect_capture(capture,tape_path,selected_domain)
    text=decode(ledger.read_bytes());tape=decode_tape(tape_path.read_bytes())['records']
    result=ancestry(text,True);exact=set(result.pop('exact_expression_ids'))
    expressions=dict(result.pop('exact_expression_nodes'))
    transform_inputs=dict(result.pop('transform_inputs'))
    contributions=dict(result.pop('record_contributions'));result.pop('unproven_records')
    result.update(bind_tape(text,tape,ordinal));result.update(verify_copies(text,ordinal))
    memory=[dict(re.findall(r'(\w+)=([^ ]+)',l)) for l in text.splitlines() if 'FC067_PRODUCER_MEMORY ' in l]
    result.update(verify_memory(memory))
    owners=dict(bind_records(text,records(text,True)[1],True)['copy_record_ids'])
    copies={}
    for line in text.splitlines():
        if 'FC067_PRODUCER_COPY ' not in line:continue
        r=dict(re.findall(r'(\w+)=([^ ]+)',line))
        copies[tuple(int(r[k]) for k in ('ordinal','generation','offset'))]=int(r['id'])
    selected=[];camera_rows=[];selected_transforms=set();divided_draws={};source_rows=[]
    for r in tape:
        identity=owners.get(copies.get((r['ordinal'],r['generation'],r['ta_offset'])))
        if identity in exact and identity in contributions:
            selected.append(r)
            selected_transforms.update(contributions[identity])
            tags=contributions[identity]
            if len(tags)==1 and tags[0][1]=='8c03a9ea':
                matrix=tuple(transform_inputs[tuple(tags[0])]['matrix_words'])
                divided_draws.setdefault(matrix,{}).setdefault(r['draw'],set()).add(r['vertex'])
                if source_details:
                    source_rows.append(dict(draw=r['draw'],vertex=r['vertex'],ordinal=ordinal,
                        **transform_inputs[tuple(tags[0])]))
            if calibration is not None:
                try:
                    recovered=recover(expressions[identity],calibration)
                    camera_rows.append(dict(draw=r['draw'],vertex=r['vertex'],record=list(identity),
                                            error=recovered['maximum_reprojection_error_pixels'],
                                            diagnostic=recovered))
                except ValueError as error:
                    camera_rows.append(dict(draw=r['draw'],vertex=r['vertex'],record=list(identity),rejected=str(error)))
    require(selected and all(r['ordinal']==ordinal for r in selected),'selected expression frame')
    scene_path=capture/f'frame-{ordinal+1:06d}'/'pvr-scene.json'
    require(scene_path.stat().st_size<=64*1024*1024,'scene bound')
    scene=json.loads(scene_path.read_text());vertices={r['vertex'] for r in selected}
    coverage=[]
    for draw in sorted({r['draw'] for r in selected}):
        d=inspect_scene(scene,draw,4096,8192)
        coverage.append(dict(draw=draw,supported=sum(all(v in vertices for v in tri) for tri in d['triangles']),total=d['triangle_count']))
    result.update(batch=batch,producer_ordinal=ordinal,frame_id=ordinal+1,omitted_background_vertices=4 if batch==2 else 0,
                  divided_candidate_draws=[dict(matrix_words=list(matrix),draw_vertices={str(draw):len(v) for draw,v in draws.items()})
                                          for matrix,draws in sorted(divided_draws.items())],
                  selected_exact_expression_vertices=len(selected),selected_frame_coverage=coverage,
                  selected_frame_supported_triangles=sum(d['supported'] for d in coverage),
                  accepted_scene_binding=scene_binding,world_camera_recovered=False)
    if source_details:result['selected_divided_sources']=source_rows
    if calibration is not None:
        import numpy as np
        from binary32_oracle import fraction
        from affine_calibration_inspect import inspect as calibrate
        matrices=[];raw_rows={}
        for identity in sorted(selected_transforms):
            words=transform_inputs[identity]['matrix_words']
            require(len(words)==16,'selected matrix words')
            matrix=np.array([float(fraction(w)) for w in words]).reshape(4,4).T
            row=tuple(float(v) for v in matrix[3]);raw_rows[str(row)]=raw_rows.get(str(row),0)+1
            require(row in ((0.,0.,0.,0.),(0.,0.,0.,1.)),'unsupported fourth matrix row')
            # Analyze the captured XYZ affine map, not an invented captured W.
            analysis=matrix.copy();analysis[3]=[0,0,0,1];matrices.append(analysis)
        try:
            matrix_contract=calibrate(matrices,calibration)
            wrong=[calibration[0]*2,calibration[1]]
            try:calibrate(matrices,wrong)
            except ValueError as error:
                require(str(error)=='incompatible normalized calibration','wrong calibration control reason')
            else:raise ValueError('wrong calibration control accepted')
            matrix_contract['doubled_calibration_rejected']=True
        except ValueError as error:matrix_contract=dict(rejected=str(error))
        result['selected_matrix_diagnostic']=dict(contributions=len(matrices),raw_fourth_rows=raw_rows,
            analysis='captured top-three-row affine maps; homogeneous analysis row only',contract=matrix_contract)
        recovered=[r for r in camera_rows if 'error' in r]
        by_vertex={r['vertex']:r for r in recovered};affected=[]
        failing={r['vertex'] for r in recovered if r['error']>=.001}
        for draw in coverage:
            for tri in inspect_scene(scene,draw['draw'],4096,8192)['triangles']:
                if not failing.intersection(tri):continue
                require(all(v in by_vertex for v in tri),'outlier triangle missing reconstruction')
                affected.append(dict(draw=draw['draw'],vertices=tri,
                    observed_outside_common_plane=outside_common_plane([by_vertex[v]['diagnostic']['observed_screen_position'] for v in tri]),
                    mathematical_outside_common_plane=outside_common_plane([by_vertex[v]['diagnostic']['mathematical_screen_position'] for v in tri])))
        result['conditional_camera_diagnostic']=dict(
            calibration=list(calibration),calibration_independently_proven=False,
            recovered_vertices=len(recovered),rejected=[r for r in camera_rows if 'rejected' in r],
            maximum_error_pixels=max((r['error'] for r in recovered),default=None),
            tolerance_pixels=.001,failures=[r for r in recovered if r['error']>=.001],
            outlier_triangles=affected,rectangle=[0,0,640,480])
        from expression_mesh_inspect import build as build_mesh
        mesh=build_mesh(scene,recovered,matrix_contract)
        if source_details:result['expression_mesh']=mesh
        result['expression_mesh_summary']={k:v for k,v in mesh.items() if k not in ('vertices','triangles','calibration')}
        result['expression_mesh_summary'].update(vertices=len(mesh['vertices']),triangles=len(mesh['triangles']))
    return result


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('capture','tape','ledger','topology'):p.add_argument(name,type=Path)
    p.add_argument('--calibration',nargs=2,type=float)
    p.add_argument('--batch',type=int,choices=(1,2),default=1)
    p.add_argument('--ordinal',type=int,choices=(1781,1782,1783),default=1781)
    a=p.parse_args();print(json.dumps(inspect(a.capture,a.tape,a.ledger,a.topology,a.calibration,a.batch,a.ordinal),sort_keys=True))
