"""Derive bounded observation targets, never infer transform generations."""
from compact_consumer_inspect import decode
from compact_draw_inspect import inspect_capture as inspect_consumers
from transform_store_inspect import require


def group(rows):
    require(len(rows)==8235,'consumer count')
    frames=[]
    for frame in range(3):
        part=rows[frame*2745:(frame+1)*2745]
        require([r['vertex'] for r in part]==list(range(1420,4165)),'consumer domain')
        generation=part[0]['generation'];ordinal=1781+frame
        require(all(r['generation']==generation and r['ordinal']==ordinal and r['draw']==277 for r in part),'frame ownership')
        bases=sorted({r['ram_x'] for r in part});require(len(bases)==921,'target address coverage')
        require(all(base%4==0 and 0<=base<=0xfffffff7 for base in bases),'target address shape')
        require(all(a+12<=b for a,b in zip(bases,bases[1:])),'overlapping targets')
        lookup={base:i for i,base in enumerate(bases)}
        mapping=[lookup[r['ram_x']] for r in part]
        consumers=[[] for _ in bases]
        for vertex,target in enumerate(mapping,1420):consumers[target].append(vertex)
        frames.append(dict(generation=generation,bases=bases,mapping=mapping,consumers=consumers))
    require(len({f['generation'] for f in frames})==3,'context generation reuse')
    require(all(f['bases']==frames[0]['bases'] and f['mapping']==frames[0]['mapping'] for f in frames[1:]),'target domain changed')
    return dict(bases=frames[0]['bases'],mapping=frames[0]['mapping'],consumers=frames[0]['consumers'],
                context_generations=[f['generation'] for f in frames],target_records=2763,
                original_transform_generation_proven=False,world_camera_recovered=False)


def inspect_capture(capture,tape):
    inspect_consumers(capture,tape)
    return group(decode(tape.read_bytes())['records'])
