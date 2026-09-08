"""Join bounded producer continuity to selected accepted scene records."""
import argparse
import json
import re
from pathlib import Path
from compact_consumer_inspect import decode as decode_tape
from compact_draw_inspect import inspect_capture
from remaining_draw_inspect import domain
from producer_continuity_inspect import inspect as continuity
from producer_record_inspect import inspect as records
from producer_ownership_inspect import inspect as ownership, bind_records, bind_tape
from transform_ledger_inspect import decode
from transform_store_inspect import require, load_session
from producer_projection_inspect import inspect as projection
from draw_domain_inspect import inspect_scene


def writer_inventory(text, targets):
    memory, reads, counts, seen = {}, {}, {}, set()
    for line in text.splitlines():
        match=re.search(r'FC067_PRODUCER_(MEMORY|COPY) (.*)',line)
        if not match: continue
        r=dict(re.findall(r'(\w+)=([^ ]+)',match[2]))
        if match[1]=='MEMORY':
            address,size=int(r['address'],16),int(r['size'])
            if r['kind']=='write':
                for i in range(size): memory[address+i]=r['pc']
            else: reads[address]=tuple(memory.get(address+i) for i in range(size))
        else:
            key=tuple(int(r[k]) for k in ('ordinal','generation','offset'))
            if key not in targets: continue
            require(key not in seen,'duplicate inventory copy');seen.add(key)
            base=int(r['base'],16)
            signature=[]
            for i in range(3):
                writers=reads.get(base+4*i,())
                require(len(writers)==4 and None not in writers and len(set(writers))==1,'mixed/missing inventory writer')
                signature.append(writers[0])
            label=str(targets[key])+':'+','.join(signature)
            counts[label]=counts.get(label,0)+1
    require(seen==set(targets),'inventory target coverage')
    return counts


def join(text, tape, scene=None):
    result = ownership(text)
    result.update(bind_tape(text, tape))
    chain = continuity(text, True)
    result['projection'] = projection(chain['record_transforms'])
    linked = set(chain['linked_record_ids'])
    unlinked = set(chain['unlinked_record_ids'])
    bound = bind_records(text, records(text)[1], True)['copy_record_ids']
    owner_by_copy = dict(bound)
    require(len(owner_by_copy) == len(bound), 'duplicate owned copy')
    copies = {}
    for line in text.splitlines():
        if 'FC067_PRODUCER_COPY ' not in line: continue
        r = dict(re.findall(r'(\w+)=([^ ]+)', line))
        copies[int(r['ordinal']),int(r['generation']),int(r['offset'])] = int(r['id'])
    selected = []
    selected_vertices = set()
    unmatched_draws = {}
    unmatched_targets = {}
    for r in tape:
        copy = copies.get((r['ordinal'],r['generation'],r['ta_offset']))
        owner = owner_by_copy.get(copy)
        if owner in linked:
            selected.append((r['sample'],r['draw'],owner))
            selected_vertices.add(r['vertex'])
        elif copy is not None:
            unmatched_draws[r['draw']] = unmatched_draws.get(r['draw'],0)+1
            unmatched_targets[r['ordinal'],r['generation'],r['ta_offset']]=r['draw']
    result.update(selected_matrix_record_consumers=len(selected),
                  selected_matrix_record_executions=len({owner for sample,draw,owner in selected}),
                  selected_draws=sorted({draw for sample,draw,owner in selected}),
                  unlinked_record_ids=sorted(unlinked),
                  copies_consuming_unlinked_records=sum(owner in unlinked for owner in owner_by_copy.values()),
                  camera_coordinate_meaning_verified=False)
    result['alternate_consumers_without_new_family_chain_by_draw']=unmatched_draws
    result['remaining_actual_writer_inventory']=writer_inventory(text,unmatched_targets)
    if scene is not None:
        require(scene['frame_id']==1782, 'projection coverage frame')
        coverage=[]
        for draw in result['selected_draws']:
            d=inspect_scene(scene,draw,4096,8192)
            supported=sum(all(v in selected_vertices for v in t) for t in d['triangles'])
            coverage.append(dict(draw=draw,supported=supported,total=len(d['triangles'])))
        result['first_frame_triangle_coverage']=coverage
        result['first_frame_provenance_supported_triangles']=sum(d['supported'] for d in coverage)
        result['partial_triangles_excluded']=sum(d['total']-d['supported'] for d in coverage)
    return result


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture',type=Path)
    parser.add_argument('tape',type=Path)
    parser.add_argument('ledger',type=Path)
    parser.add_argument('topology',type=Path)
    args=parser.parse_args()
    session=load_session(args.capture)
    require(not any('FC067_' in line and 'REJECT' in line for line in session.splitlines()),
            'capture observer rejected; ledger-only arithmetic cannot promote this run')
    require(args.tape.is_file(), 'accepted source tape missing')
    scene=inspect_capture(args.capture,args.tape,domain(args.topology,1))
    frame_path=args.capture/'frame-001782'/'pvr-scene.json'
    require(frame_path.stat().st_size<=64*1024*1024,'scene byte bound')
    result=join(decode(args.ledger.read_bytes()),decode_tape(args.tape.read_bytes())['records'],json.loads(frame_path.read_text()))
    result['accepted_scene_binding']=scene
    print(json.dumps(result,sort_keys=True))
