"""Join actual compact consumers to bounded transform reads; not arithmetic proof."""
import argparse
import json
from pathlib import Path
from compact_consumer_inspect import decode as decode_tape
from compact_draw_inspect import inspect_capture as inspect_scene
from large_transform_targets_inspect import group
from transform_ledger_inspect import decode
from transform_store_inspect import require


def inspect(text, rows):
    targets=group(rows)
    bases=targets['bases']; generations=targets['context_generations']
    bindings={}; gathers={}; unsupported=set(); blocks={}; prefixes={}
    for position,line in enumerate(text.splitlines()):
        require('REJECT' not in line,'live observer rejection')
        fields=line.split(); tag=fields[0]
        if tag not in ('FC067_LEDGER_BIND','FC067_CT_GATHER','FC067_CT_UNSUPPORTED',
                       'FC067_CT_BLOCK','FC067_CT_PREFIX'):continue
        values=dict(field.split('=',1) for field in fields[1:])
        generation=int(values['generation'])
        require(generation in generations,'foreign generation')
        values['_position']=position
        if tag=='FC067_LEDGER_BIND':
            sample=int(values['sample']);require(sample not in bindings,'duplicate binding')
            bindings[sample]=values
        elif tag=='FC067_CT_PREFIX':
            require(generation not in prefixes,'duplicate prefix')
            require(int(values['copies'])==5069 and int(values['ordinal'])==1781+generations.index(generation),'prefix identity')
            prefixes[generation]=values
        else:
            slot=int(values['slot']);require(0<=slot<921,'slot range')
            key=generation,slot
            if tag=='FC067_CT_UNSUPPORTED':
                require(key not in unsupported and int(values['base'],16)==bases[slot],'unsupported identity')
                unsupported.add(key)
            elif tag=='FC067_CT_BLOCK':
                kind=int(values['kind']);require(0<=kind<4,'seam kind')
                require((*key,kind) not in blocks and int(values['base'],16)==bases[slot],'seam identity')
                blocks[*key,kind]=values
            else:gathers.setdefault((generation,int(values['sample'])),[]).append(values)
    require(set(bindings)==set(range(1,8236)) and set(prefixes)==set(generations),'complete binding/prefix coverage')
    supported=0; omitted=0; seen_copy=set()
    for row in rows:
        binding=bindings[row['sample']]; generation=row['generation']
        slot=targets['mapping'][row['vertex']-1420];key=generation,slot
        copy_id=int(binding['copy_id']); copy_key=generation,copy_id
        require(copy_key not in seen_copy,'reused consumer copy');seen_copy.add(copy_key)
        require(binding['exact']=='1' and int(binding['generation'])==generation
                and int(binding['vertex'])==row['vertex'] and int(binding['base'],16)==row['ram_x']
                and int(binding['pointer'],16)==row['decoder_pointer'],'consumer binding identity')
        frame=generations.index(generation)
        require(frame*5069<copy_id<=(frame+1)*5069 and row['cycle']<=int(prefixes[generation]['cycle']),'copy prefix boundary')
        own=gathers.get(copy_key,[])
        if key in unsupported:
            require(not own and not any((*key,k) in blocks for k in range(4)),'unsupported has inferred transform')
            omitted+=1;continue
        require(all((*key,k) in blocks for k in range(4)),'missing producer seam')
        require(len(own)==3 and [int(v['component']) for v in own]==[0,1,2],'gather coverage')
        for component,value in enumerate(own):
            require(int(value['slot'])==slot and int(value['address'],16)==row['ram_x']+component*4
                    and int(value['value'],16)==int(value['expected'],16)==row['after'][component+1], 'gather bytes/identity')
            require(blocks[*key,3]['_position']<value['_position']<binding['_position']
                    and value['_position']<prefixes[generation]['_position'],'consumer chronology')
        supported+=1
    require(len(blocks)+4*len(unsupported)==3*921*4,'target partition')
    return dict(bound_consumers=len(rows),supported_consumers=supported,unsupported_consumers=omitted,
                traced_address_instances=len(blocks)//4,unsupported_address_instances=len(unsupported),
                original_transform_arithmetic_proven=False,world_camera_recovered=False)


def inspect_capture(capture,tape,ledger):
    inspect_scene(capture,tape)
    return inspect(decode(ledger.read_bytes()),decode_tape(tape.read_bytes())['records'])


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture',type=Path);parser.add_argument('tape',type=Path);parser.add_argument('ledger',type=Path)
    args=parser.parse_args()
    print(json.dumps(inspect_capture(args.capture,args.tape,args.ledger),sort_keys=True))
