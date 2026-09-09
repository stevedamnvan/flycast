"""Partial original-transform calibrated scene; not a recovered world camera."""
import argparse
import json
from pathlib import Path
import numpy as np
from binary32_oracle import fraction
from camera_calibration_inspect import analyze
from calibrated_scene_inspect import build_frame
from compact_consumer_inspect import decode as decode_tape
from large_ledger_binding_inspect import inspect_capture as bindings
from large_transform_arithmetic_inspect import inspect as arithmetic
from transform_ledger_inspect import decode
from transform_span_inspect import records
from transform_store_inspect import require


def inspect_capture(capture,tape,ledger,source_details=False):
    binding=bindings(capture,tape,ledger);text=decode(ledger.read_bytes())
    verified=arithmetic(text,True);details=verified.pop('record_details')
    require(len(details)==binding['traced_address_instances'],'arithmetic/binding coverage')
    require(all(r['screen_center_words']==[0x43a00000,0x43700000] for r in details),'screen center')
    words=[r['xf_matrix_words'] for r in details]
    for part in text.split('FC067_EXTRA_FILTER ')[1:]:
        section=part.split('FC067_EXTRA_EXIT ')[0]
        live={int(r['reg']):int(r['value'],16) for r in records(section,'FC067_EXTRA_INPUT')}
        words.append([live[r] for r in range(32,48)])
    require(len(words)==verified['records']+verified['accumulations'],'matrix contribution coverage')
    matrices=[np.array([float(fraction(w)) for w in row]).reshape(4,4).T for row in words]
    contract=analyze(matrices,matrix_limit=19341)
    wrong=contract['normalized_calibration'][:];wrong[0]*=2
    try:analyze(matrices,wrong,matrix_limit=19341)
    except ValueError:pass
    else:raise ValueError('wrong shared calibration accepted')
    contract.update(screen_center=[320,240],wrong_scale_rejected=True,
                    source_axis_convention='record-positive-Y-projects-down',depth_contract='verified-unit-reciprocal-record-Z')
    by_address={(r['generation'],r['base']):r for r in details}
    rows=decode_tape(tape.read_bytes())['records'];frames=[]
    for ordinal in (1781,1782,1783):
        scene_path=capture/f'frame-{ordinal+1:06d}'/'pvr-scene.json'
        require(scene_path.stat().st_size<=64*1024*1024,'scene byte budget')
        scene=json.loads(scene_path.read_text());samples=[]
        for row in rows:
            if row['ordinal']!=ordinal:continue
            record=by_address.get((row['generation'],row['ram_x']))
            if record is None:continue
            require(record['final_words']==row['after'][1:4],'reconstruction source binding')
            samples.append(dict(record,producer=dict(ordinal=ordinal),draw=row['draw'],vertex=row['vertex']))
        domain=dict(vertices=[s['vertex'] for s in samples],draw=277)
        frame=build_frame(scene,samples,contract,domain=domain)
        frames.append(frame if source_details else {k:frame[k] for k in ('frame_id','supported_triangles','omitted_opaque_triangles','maximum_reprojection_error_pixels')})
    return dict(arithmetic=verified,consumer_binding=binding,calibration=contract,frames=frames,
                complete_scene=False,remix_gpu_verified=False)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('capture','tape','ledger'):parser.add_argument(name,type=Path)
    args=parser.parse_args();print(json.dumps(inspect_capture(args.capture,args.tape,args.ledger),sort_keys=True))
