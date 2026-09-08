"""Version-bound two-draw reconstruction; no world-camera or Remix claim."""
import argparse
import json
from pathlib import Path
import numpy as np
from binary32_oracle import fraction
from camera_calibration_inspect import analyze
from calibrated_scene_inspect import build_frame
from compact_consumer_inspect import decode as decode_tape
from compact_draw_inspect import inspect_capture as scene_binding, NEXT_DOMAIN, FOUR_DOMAIN, domains
from large_transform_targets_inspect import group
from large_transform_arithmetic_inspect import inspect as arithmetic
from transform_incarnation_inspect import inspect as lifetimes
from transform_ledger_inspect import decode
from transform_span_inspect import records
from transform_store_inspect import require


def inspect_capture(capture,tape,ledger,domain=NEXT_DOMAIN,target_count=1122,affine_contributions=False,max_versions=4,dense=False):
    binding=scene_binding(capture,tape,domain)
    rows=decode_tape(tape.read_bytes())['records'];targets=group(rows,domain,target_count)
    life=lifetimes(decode(ledger.read_bytes()),targets['bases'],rows,True,max_versions,dense)
    text=life.pop('text');selected=life.pop('selected');selected_slots=life.pop('selected_slots')
    verified=arithmetic(text,True,4096 if dense else max_versions*target_count);details=verified.pop('record_details')
    require(len(details)==life['lifetimes'],'lifetime arithmetic coverage')
    require(all(r['screen_center_words']==[0x43a00000,0x43700000] for r in details),'screen center')
    words=[r['xf_matrix_words'] for r in details]
    for part in text.split('FC067_EXTRA_FILTER ')[1:]:
        section=part.split('FC067_EXTRA_EXIT ')[0]
        live={int(r['reg']):int(r['value'],16) for r in records(section,'FC067_EXTRA_INPUT')}
        words.append([live[r] for r in range(32,48)])
    require(len(words)==verified['records']+verified['accumulations'],'matrix coverage')
    matrices=[np.array([float(fraction(w)) for w in row]).reshape(4,4).T for row in words]
    from affine_calibration_inspect import inspect as affine_calibration
    calibrate=(lambda m,k=None:affine_calibration(m,k)) if affine_contributions else (lambda m,k=None:analyze(m,k,matrix_limit=19341))
    contract=calibrate(matrices)
    wrong=contract['normalized_calibration'][:];wrong[0]*=2
    try:calibrate(matrices,wrong)
    except ValueError:pass
    else:raise ValueError('wrong shared calibration accepted')
    contract.update(screen_center=[320,240],wrong_scale_rejected=True)
    lookup={(r['generation'],r['slot']):r for r in details};frames=[]
    for ordinal in (1781,1782,1783):
        path=capture/f'frame-{ordinal+1:06d}'/'pvr-scene.json'
        require(path.stat().st_size<=64*1024*1024,'scene byte budget')
        scene=json.loads(path.read_text());samples=[]
        for row in rows:
            if row['ordinal']!=ordinal:continue
            gen,slot,version=selected[row['sample']]
            record=lookup[selected_slots[row['sample']]]
            require(record['base']==row['ram_x'] and record['final_words']==row['after'][1:4],'versioned reconstruction binding')
            samples.append(dict(record,producer=dict(ordinal=ordinal),draw=row['draw'],vertex=row['vertex']))
        results=[]
        for draw,vertices in domains(domain):
            own=[s for s in samples if s['draw']==draw]
            result=build_frame(scene,own,contract,domain=dict(vertices=list(vertices),draw=draw))
            results.append({k:result[k] for k in ('supported_triangles','omitted_opaque_triangles','maximum_reprojection_error_pixels')})
        frames.append(dict(frame_id=ordinal+1,draws=results,supported_triangles=sum(r['supported_triangles'] for r in results)))
    return dict(lifetimes=life,arithmetic=verified,consumer_binding=binding,calibration=contract,
                frames=frames,complete_scene=False,remix_gpu_verified=False)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('capture','tape','ledger'):parser.add_argument(name,type=Path)
    parser.add_argument('--four-draw-batch',action='store_true')
    parser.add_argument('--affine-contributions',action='store_true')
    args=parser.parse_args()
    domain,count=(FOUR_DOMAIN,1005) if args.four_draw_batch else (NEXT_DOMAIN,1122)
    print(json.dumps(inspect_capture(args.capture,args.tape,args.ledger,domain,count,args.affine_contributions),sort_keys=True))
