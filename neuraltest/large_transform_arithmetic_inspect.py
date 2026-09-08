"""Indexed large-domain arithmetic checks using established instruction oracles."""
import argparse
import json
from pathlib import Path
from collections import defaultdict
from camera_transform_inspect import TAGS,inspect_supply,inspect_initial,inspect_pred,inspect_calc,inspect_extra
from compact_consumer_inspect import decode as decode_tape
from large_ledger_binding_inspect import inspect_capture as inspect_bindings
from transform_ledger_inspect import decode
from transform_span_inspect import records
from transform_store_inspect import require


def inspect(text,include_records=False):
    require('REJECT' not in text,'observer rejection')
    sections=[]; extra_sections=[]; current=None; extra=None
    indexed=defaultdict(list); descriptors={}
    for position,line in enumerate(text.splitlines()):
        tag=line.split(' ',1)[0]
        row=dict(part.split('=',1) for part in line.split()[1:]);row['_position']=position
        if tag=='FC067_CT_BLOCK':
            current=[row,[]];sections.append(current)
        if current is not None:current[1].append(line)
        if tag=='FC067_EXTRA_FILTER':
            require(extra is None,'nested accumulation');extra=[]
        if extra is not None:
            extra.append(line)
            if tag=='FC067_EXTRA_EXIT':extra_sections.append(extra);extra=None
        if tag in ('FC067_X_LOAD','FC067_X_EDGE','FC067_CT_WRITE','FC067_CT_GATHER','FC067_CT_BEGIN','FC067_CT_PREFIX'):
            indexed[tag].append(row)
    require(extra is None and 0<len(sections)<=3*921*4,'section bounds')
    def restore(lines,tag):
        source='\n'.join(lines)+'\n';entry=records(source,'FC067_'+tag+'_ENTRY')
        require(len(entry)==1,'unique descriptor entry')
        identity=tag,entry[0]['descriptor'];op='FC067_'+tag+'_OP '
        definition=[line for line in lines if line.startswith(op)]
        if definition:
            require(identity not in descriptors,'duplicate descriptor definition');descriptors[identity]=definition
            require(len(descriptors)<=32,'descriptor budget')
        else:
            require(identity in descriptors,'missing descriptor definition');source+='\n'.join(descriptors[identity])+'\n'
        return source
    groups={}
    for header,lines in sections:
        key=tuple(int(header[k]) for k in ('generation','slot','kind'))
        require(key not in groups and 0<=key[1]<921 and 0<=key[2]<4,'seam identity')
        groups[key]=header,restore(lines,TAGS[key[2]])
    extras=defaultdict(list)
    for lines in extra_sections:
        source=restore(lines,'EXTRA');selected=records(source,'FC067_EXTRA_SELECTION')[0]
        key=int(selected['generation']),int(selected['slot'])
        result=inspect_extra(source,int(selected['target'],16),key[0],False,16578)
        extras[key].append(result)
    writes=defaultdict(list);loads={};edges={}
    for row in indexed['FC067_CT_WRITE']:writes[int(row['generation']),int(row['slot'])].append(row)
    for tag,target in (('FC067_X_LOAD',loads),('FC067_X_EDGE',edges)):
        for row in indexed[tag]:
            key=int(row['generation']),int(row['slot']);require(key not in target,'duplicate X event');target[key]=row
    begins={int(r['generation']):r for r in indexed['FC067_CT_BEGIN']}
    prefixes={int(r['generation']):r for r in indexed['FC067_CT_PREFIX']}
    require(len(begins)==len(indexed['FC067_CT_BEGIN']) and len(prefixes)==len(indexed['FC067_CT_PREFIX']),'duplicate frame boundary')
    output={};direct_x=set()
    for generation,slot in sorted({key[:2] for key in groups}):
        key=generation,slot
        require(all((*key,k) in groups for k in range(4)),'incomplete transform')
        parts=[groups[*key,k] for k in range(4)];base=int(parts[0][0]['base'],16)
        require(all(int(p[0]['base'],16)==base for p in parts),'changed target')
        require(generation in begins and generation in prefixes,'missing frame boundary')
        require(all(int(begins[generation]['cycle'])<=int(p[0]['cycle'])<=int(prefixes[generation]['cycle'])
                    and p[0]['ordinal_hint']==prefixes[generation]['ordinal'] for p in parts),'seam frame chronology')
        first_supply=records(parts[0][1],'FC067_SUPPLY_ENTRY')[0]['block']=='8c03c932'
        first_pred=records(parts[2][1],'FC067_PRED_ENTRY')[0]['block']=='8c03c998'
        supply=inspect_supply(parts[0][1],base,parts[0][0]['cycle'],str(generation),parts[1][0]['cycle'],first_supply)
        initial=inspect_initial(parts[1][1],base,parts[1][0]['cycle'])
        pred=inspect_pred(parts[2][1],base,parts[2][0]['cycle'],parts[3][0]['cycle'],first_pred)
        calc=inspect_calc(parts[3][1],base,parts[3][0]['cycle'])
        require(supply['transformed_words'][:3]==initial['initial_xyz_words'],'initial arithmetic binding')
        projected=initial['initial_xyz_words'];expected_writes=[]
        for j,value in enumerate(reversed(projected)):expected_writes.append((1,base+8-4*j,value))
        for contribution in extras.get(key,[]):
            require([v for a,v in contribution['loads'][5:]]==projected,'accumulation prior binding')
            expected_writes.extend((4,a,v) for a,v in contribution['stores'])
            projected=[v for a,v in contribution['stores']][::-1]
        writer=len(expected_writes)
        if first_pred:
            require(key not in loads and key not in edges and [v for _,v in pred['source_loads']]==projected,'direct XYZ record binding')
            direct_x.add(key)
        else:
            require(key in loads and key in edges,'missing X continuity')
            load,edge=loads[key],edges[key]
            require(load['pc']=='8c03c9d6' and int(load['address'],16)==base and int(load['writer'])==writer
                    and int(load['value'],16)==int(load['expected'],16)==projected[0],'X load binding')
            require(edge['block']=='8c03c9a4' and int(edge['pointer'],16)==base+4 and edge['exact']=='1'
                    and int(edge['value'],16)==int(edge['expected'],16)==projected[0]
                    and 16 not in pred['written_registers'],'X register continuity')
        require(calc['live_input_words'][16]==projected[0] and calc['live_input_words'][17]==projected[1]
                and [v for _,v in pred['source_loads'][-2:]]==projected[1:3]
                and pred['factor_word']==calc['live_input_words'][19],'projection input binding')
        require(int(records(parts[2][1],'FC067_PRED_EDGE')[0]['x'],16)==projected[0],'X predecessor exit')
        for j,value in enumerate(reversed(calc['final_words_xyz'])):expected_writes.append((3,base+8-4*j,value))
        own=writes[key];require(len(own)==len(expected_writes)<=64,'write coverage')
        for event,(row,(kind,address,value)) in enumerate(zip(own,expected_writes),1):
            require(int(row['event'])==event and int(row['kind'])==kind and int(row['address'],16)==address
                    and int(row['source'],16)==int(row['actual'],16)==value,'actual write arithmetic binding')
            component=(base+8-address)//4
            pc=([0x8c03c97c,0x8c03c97e,0x8c03c982][component] if kind==4
                else (0x8c03c94e if kind==1 else 0x8c03c9ca)+component*2)
            require(int(row['pc'],16)==pc and int(begins[generation]['cycle'])<=int(row['cycle'])<=int(prefixes[generation]['cycle']),'writer PC/clock')
        if first_pred:require(own[writer-1]['_position']<parts[2][0]['_position']<parts[3][0]['_position'],'direct X chronology')
        else:require(own[writer-1]['_position']<load['_position']<edge['_position']
                     <parts[2][0]['_position']<parts[3][0]['_position'],'X chronology')
        output[key]=dict(base=base,final_words=calc['final_words_xyz'],writes=len(own),
                         generation=generation,slot=slot,preprojection_record_words=projected,
                         source_point_words=supply['source_point_words'],xf_matrix_words=supply['xf_matrix_words'],
                         screen_center_words=[calc['live_input_words'].get(r) for r in (20,21)])
    require(set(extras)<=set(output) and set(loads)==set(edges)==set(output)-direct_x,'orphan arithmetic')
    for row in indexed['FC067_CT_GATHER']:
        key=int(row['generation']),int(row['slot']);record=output[key];component=int(row['component'])
        require(0<=component<3 and int(row['address'],16)==record['base']+4*component
                and int(row['value'],16)==int(row['expected'],16)==record['final_words'][component]
                and int(row['events'])==record['writes']
                and row['writers']==','.join([str(record['writes']-component)]*4),'final read arithmetic binding')
    result=dict(records=len(output),seams=len(groups),accumulations=len(extra_sections),
                writes=sum(len(v) for v in writes.values()),reads=len(indexed['FC067_CT_GATHER']),
                arithmetic_verified=True,world_camera_recovered=False)
    if include_records:result['record_details']=list(output.values())
    return result


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('capture','tape','ledger'):parser.add_argument(name,type=Path)
    args=parser.parse_args();binding=inspect_bindings(args.capture,args.tape,args.ledger)
    result=inspect(decode(args.ledger.read_bytes()));result['consumer_binding']=binding
    print(json.dumps(result,sort_keys=True))
