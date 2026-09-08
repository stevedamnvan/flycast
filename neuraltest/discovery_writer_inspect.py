"""Observed last-writer byte binding for missing targets; not transform proof."""
import argparse
from collections import Counter,defaultdict
import json
from pathlib import Path
from compact_consumer_inspect import decode as decode_tape
from compact_draw_inspect import inspect_capture as inspect_scene
from transform_ledger_inspect import decode
from transform_span_inspect import records
from transform_store_inspect import require


def inspect(text,rows):
    require('REJECT' not in text,'observer rejected')
    observed=records(text,'FC067_CT_DISCOVERY_READ');require(0<len(observed)<=3*15*64*4,'read budget')
    groups=defaultdict(list)
    for r in observed:
        generation,sample,component,byte=(int(r[k]) for k in ('generation','sample','component','byte'))
        require(0<=component<3 and 0<=byte<4 and r['exact']=='1','byte identity')
        address=int(r['address'],16);start=int(r['write_address'],16);size=int(r['size']);offset=(address+byte-start)&0xffffff
        require(size in (1,2,4,8) and 0<=offset<size and int(r['writer'])>0,'writer extent')
        require((int(r['value'],16)>>(8*byte))&255==(int(r['word'],16)>>(8*offset))&255,'writer byte mismatch')
        require(int(r['write_cycle'])<=int(r['read_cycle']),'writer clock')
        groups[generation,sample].append(r)
    for own in groups.values():
        require([(int(r['component']),int(r['byte'])) for r in own]==[(c,b) for c in range(3) for b in range(4)],'XYZ byte coverage')
        require(len({r['slot'] for r in own})==1,'changed target slot')
    binds=records(text,'FC067_LEDGER_BIND');require(len(binds)==len(rows)==8235,'consumer coverage')
    selected=0;instances=set();families=Counter()
    for row,b in zip(rows,binds):
        require(int(b['sample'])==row['sample'] and int(b['generation'])==row['generation']
                and int(b['vertex'])==row['vertex'] and int(b['base'],16)==row['ram_x']
                and int(b['pointer'],16)==row['decoder_pointer'] and b['exact']=='1','actual decoder binding')
        own=groups.get((row['generation'],int(b['copy_id'])))
        if own is None:continue
        for r in own:
            component=int(r['component'])
            require(int(r['address'],16)==row['ram_x']+4*component and int(r['value'],16)==row['after'][component+1]
                    and int(r['read_cycle'])<=row['cycle'],'consumed byte binding')
        selected+=1;instances.add((row['generation'],row['ram_x']))
        families[own[0]['blocks']]+=1
    require(selected==138 and len(instances)==45,'missing-target selected coverage')
    return dict(selected_consumers=selected,address_instances=len(instances),byte_reads=len(observed),
                executed_block_histories=dict(families),original_transform_proven=False,
                unobserved_writers_excluded=False,world_camera_recovered=False)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('capture','tape','ledger'):parser.add_argument(name,type=Path)
    args=parser.parse_args();inspect_scene(args.capture,args.tape)
    print(json.dumps(inspect(decode(args.ledger.read_bytes()),decode_tape(args.tape.read_bytes())['records']),sort_keys=True))
