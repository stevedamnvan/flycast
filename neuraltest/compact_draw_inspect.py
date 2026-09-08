"""Bind compact observed consumers to captured geometry, not original transforms."""
from collections import defaultdict
from compact_consumer_inspect import decode,HEADER,RECORD
from draw_domain_inspect import inspect_scene
from transform_store_inspect import require,load_session


DEFAULT_DOMAIN=((277,1420,4165),)
NEXT_DOMAIN=((1503,7272,9118),(1887,9118,10664))
FOUR_DOMAIN=((161,823,1420),(854,4165,5196),(1222,5929,7272),(2266,11172,11732))


def selection(domain):
    require(isinstance(domain,tuple) and 1<=len(domain)<=8,'draw batch bound')
    mapping={}
    for draw,first,end in domain:
        require(all(type(v) is int for v in (draw,first,end)) and draw>=0 and 0<=first<end<=65536,'draw batch shape')
        require(end-first<=4096,'per-draw vertex bound')
        for vertex in range(first,end):
            require(vertex not in mapping,'overlapping selected domains');mapping[vertex]=draw
    require(1<=len(mapping)<=4096,'selected vertex budget')
    return mapping


def inspect(tape,frames,domain=DEFAULT_DOMAIN):
    mapping=selection(domain);vertices=sorted(mapping);count=len(vertices)
    rows=decode(tape)['records']
    require(len(rows)==count*3 and len(frames)==3,'large draw coverage')
    results=[];epochs=set();generations=set()
    for frame,(scene,manifest) in enumerate(frames):
        domains=[inspect_scene(scene,draw,4096,8192) for draw,_,_ in domain]
        require(all(d['vertices']==list(range(first,end)) for d,(_,first,end) in zip(domains,domain)),'selected vertex domain')
        producer=manifest['producer_identity']
        require(producer['available'] is True and producer['clock']=='sh4-scheduler-cycles'
                and producer['ordinal']==1781+frame,'producer identity')
        require(scene['frame_id']==manifest['frame_id']==1782+frame
                and scene['git_sha']==manifest['git_sha'] and scene['game_id']==manifest['game_id']=='T1401N','scene identity')
        selected=rows[frame*count:(frame+1)*count]
        epochs.add(selected[0]['epoch']);generations.add(selected[0]['generation'])
        pointers=set();offsets=set();values=defaultdict(set);last=0
        for index,row in enumerate(selected):
            require(row['epoch']==producer['epoch'] and row['ordinal']==producer['ordinal']
                    and row['generation']==selected[0]['generation'],'consumer frame ownership')
            require(row['vertex']==vertices[index] and row['draw']==mapping[vertices[index]],'consumer vertex/draw')
            require(last<=row['cycle']<=producer['cycle'],'consumer clock')
            last=row['cycle']
            require(row['after'][1:4]==scene['vertices'][row['vertex']][:3],'consumer scene bytes')
            require(row['decoder_pointer'] not in pointers and row['ta_offset'] not in offsets,'duplicate packet association')
            pointers.add(row['decoder_pointer']);offsets.add(row['ta_offset'])
            values[row['ram_x']].add(tuple(row['after'][1:4]))
        results.append(dict(frame_id=scene['frame_id'],vertices=count,triangles=sum(d['triangle_count'] for d in domains),
                            source_addresses=len(values),addresses_with_multiple_values=sum(len(v)>1 for v in values.values())))
    require(len(epochs)==1 and len(generations)==3,'generation/epoch reuse')
    return dict(frames=results,consumer_scene_binding=True,original_transform_proven=False,
                source_value_reuse_is_identity=False,world_camera_recovered=False,production_enabled=False)


def inspect_capture(capture,tape,domain=DEFAULT_DOMAIN):
    import json
    count=len(selection(domain))*3;byte_count=HEADER.size+count*RECORD.size
    require(tape.stat().st_size==byte_count,'selected tape byte count')
    session=load_session(capture)
    require(f'FC067_COMPACT_COMPLETE bytes={byte_count} records={count} transport_only=1' in session,'live completion')
    require(not any('FC067_' in line and 'REJECT' in line for line in session.splitlines()),'live observer rejected')
    frames=[]
    for number in (1782,1783,1784):
        folder=capture/f'frame-{number:06d}'
        def read(name):
            path=folder/name;require(path.stat().st_size<=64*1024*1024,'JSON byte bound')
            return json.loads(path.read_text())
        frames.append((read('pvr-scene.json'),read('manifest.json')))
    return inspect(tape.read_bytes(),frames,domain)


if __name__=='__main__':
    import argparse,json
    from pathlib import Path
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture',type=Path);parser.add_argument('tape',type=Path)
    batch=parser.add_mutually_exclusive_group()
    batch.add_argument('--next-batch',action='store_true')
    batch.add_argument('--four-draw-batch',action='store_true')
    args=parser.parse_args()
    domain=FOUR_DOMAIN if args.four_draw_batch else NEXT_DOMAIN if args.next_batch else DEFAULT_DOMAIN
    print(json.dumps(inspect_capture(args.capture,args.tape,domain),sort_keys=True))
