"""Bind compact observed consumers to captured geometry, not original transforms."""
from collections import defaultdict
from compact_consumer_inspect import decode
from draw_domain_inspect import inspect_scene
from transform_store_inspect import require,load_session


def inspect(tape,frames):
    rows=decode(tape)['records']
    require(len(rows)==8235 and len(frames)==3,'large draw coverage')
    results=[];epochs=set();generations=set()
    for frame,(scene,manifest) in enumerate(frames):
        domain=inspect_scene(scene,277,4096,8192)
        require(domain['vertices']==list(range(1420,4165)),'selected vertex domain')
        producer=manifest['producer_identity']
        require(producer['available'] is True and producer['clock']=='sh4-scheduler-cycles'
                and producer['ordinal']==1781+frame,'producer identity')
        require(scene['frame_id']==manifest['frame_id']==1782+frame
                and scene['git_sha']==manifest['git_sha'] and scene['game_id']==manifest['game_id']=='T1401N','scene identity')
        selected=rows[frame*2745:(frame+1)*2745]
        epochs.add(selected[0]['epoch']);generations.add(selected[0]['generation'])
        pointers=set();offsets=set();values=defaultdict(set);last=0
        for index,row in enumerate(selected):
            require(row['epoch']==producer['epoch'] and row['ordinal']==producer['ordinal']
                    and row['generation']==selected[0]['generation'],'consumer frame ownership')
            require(row['vertex']==1420+index and row['draw']==277,'consumer vertex/draw')
            require(last<=row['cycle']<=producer['cycle'],'consumer clock')
            last=row['cycle']
            require(row['after'][1:4]==scene['vertices'][row['vertex']][:3],'consumer scene bytes')
            require(row['decoder_pointer'] not in pointers and row['ta_offset'] not in offsets,'duplicate packet association')
            pointers.add(row['decoder_pointer']);offsets.add(row['ta_offset'])
            values[row['ram_x']].add(tuple(row['after'][1:4]))
        results.append(dict(frame_id=scene['frame_id'],vertices=2745,triangles=domain['triangle_count'],
                            source_addresses=len(values),addresses_with_multiple_values=sum(len(v)>1 for v in values.values())))
    require(len(epochs)==1 and len(generations)==3,'generation/epoch reuse')
    return dict(frames=results,consumer_scene_binding=True,original_transform_proven=False,
                source_value_reuse_is_identity=False,world_camera_recovered=False,production_enabled=False)


def inspect_capture(capture,tape):
    import json
    require(tape.stat().st_size==1317616,'selected tape byte count')
    session=load_session(capture)
    require('FC067_COMPACT_COMPLETE bytes=1317616 records=8235 transport_only=1' in session,'live completion')
    require(not any('FC067_' in line and 'REJECT' in line for line in session.splitlines()),'live observer rejected')
    frames=[]
    for number in (1782,1783,1784):
        folder=capture/f'frame-{number:06d}'
        def read(name):
            path=folder/name;require(path.stat().st_size<=64*1024*1024,'JSON byte bound')
            return json.loads(path.read_text())
        frames.append((read('pvr-scene.json'),read('manifest.json')))
    return inspect(tape.read_bytes(),frames)


if __name__=='__main__':
    import argparse,json
    from pathlib import Path
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture',type=Path);parser.add_argument('tape',type=Path)
    args=parser.parse_args();print(json.dumps(inspect_capture(args.capture,args.tape),sort_keys=True))
