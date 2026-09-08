"""Verify chronological address lifetimes before invoking arithmetic oracles."""
from transform_store_inspect import require


SLOT_TAGS={'FC067_CT_BLOCK','FC067_CT_WRITE','FC067_CT_GATHER',
           'FC067_X_LOAD','FC067_X_EDGE','FC067_EXTRA_SELECTION',
           'FC067_X_PREFETCH_LOAD','FC067_X_PREFETCH_EDGE'}


def inspect(text, bases, rows, include_text=False, max_versions=4, dense=False):
    require(type(dense) is bool,'dense contract')
    require(type(max_versions) is int and max_versions in (4,6),'version contract')
    require(dense or max_versions==4 or len(bases)*max_versions<=4096,'expanded lifetime record budget')
    require(1<=len(bases)<=1122 and len(set(bases))==len(bases),'address budget')
    require(0<len(rows)<=12288,'consumer budget')
    current={}; lifetimes={}; reads={}; bindings={}; prefixes={}; begins={}; output=[]
    prefetch=None;prefetches=0;frame_counts={}
    def complete(state):
        require(state['seams']==15 and set(state['words'])=={0,1,2}
                and state['reads']%3==0,'incomplete retired lifetime')
    for position,line in enumerate(text.splitlines()):
        require('REJECT' not in line,'observer rejection')
        parts=line.split();tag=parts[0]
        fields=dict(p.split('=',1) for p in parts[1:])
        if tag=='FC067_CT_BEGIN':
            gen=int(fields['generation']);require(gen not in begins and len(begins)<3,'generation begin')
            begins[gen]=int(fields['cycle'])
        if tag=='FC067_CT_PREFIX':
            gen=int(fields['generation']);require(gen in begins and gen not in prefixes,'generation end')
            prefixes[gen]=int(fields['cycle']);require(prefixes[gen]>=begins[gen],'frame clock')
        if tag=='FC067_CT_INCARNATION':
            gen,slot,version=(int(fields[k]) for k in ('generation','slot','version'))
            require(gen in begins and gen not in prefixes and 0<=slot<len(bases),'lifetime frame/slot')
            key=gen,slot;previous=current.get(key)
            expected=1 if previous is None else previous['version']+1
            require(version==expected and (dense or version<=max_versions),'lifetime sequence/budget')
            virtual=frame_counts.get(gen,0)
            require(not dense or virtual<4096,'dense lifetime record budget')
            require(not dense or int(fields.get('dense_id','-1'))==virtual,'dense lifetime identity')
            frame_counts[gen]=virtual+1
            require(int(fields['previous_seams'])==(previous['seams'] if previous else 0)
                    and int(fields['previous_reads'])==(previous['reads'] if previous else 0),'retirement evidence')
            if previous:complete(previous)
            cycle=int(fields['cycle']);require(cycle>=begins[gen] and (not previous or cycle>=previous['cycle']),'lifetime clock')
            state=dict(version=version,virtual=virtual if dense else (version-1)*len(bases)+slot,
                       seams=0,reads=0,words={},cycle=cycle,position=position,writes=0)
            current[key]=state;lifetimes[gen,slot,version]=state
        if tag in SLOT_TAGS:
            gen,slot=(int(fields[k]) for k in ('generation','slot'));key=gen,slot
            require(key in current and gen not in prefixes,'event outside lifetime')
            state=current[key];identity=gen,slot,state['version']
            if tag=='FC067_X_PREFETCH_LOAD':
                require(prefetch is None and state['seams']==15 and state['words'].get(0)==int(fields['value'],16)
                        and fields['value']==fields['expected'] and fields['pc']=='8c03c9d6'
                        and int(fields['address'],16)==bases[slot],'prefetch source')
                prefetch=identity,int(fields['value'],16)
            elif tag=='FC067_X_PREFETCH_EDGE':
                require(prefetch==(identity,int(fields['value'],16)) and fields['value']==fields['expected']
                        and fields['block']=='8c03c9d8' and fields['exact']=='1'
                        and int(fields['pointer'],16)==bases[slot]+4,'prefetch edge')
                prefetch=None;prefetches+=1
            if 'cycle' in fields:
                cycle=int(fields['cycle']);require(cycle>=state['cycle'],'event clock');state['cycle']=cycle
            if tag=='FC067_CT_BLOCK':
                kind=int(fields['kind']);require(0<=kind<4 and not state['seams']&(1<<kind),'duplicate seam')
                require(int(fields['base'],16)==bases[slot],'seam address')
                state['seams']|=1<<kind
            elif tag=='FC067_CT_WRITE':
                require(state['reads']==0,'write after consumption without new lifetime')
                address=int(fields['address'],16);component=(address-bases[slot])//4
                require(address==bases[slot]+4*component and 0<=component<3,'write address')
                state['writes']+=1;require(int(fields['event'])==state['writes']<=64,'write sequence')
                require(fields['source']==fields['actual'],'write bytes')
                state['words'][component]=int(fields['actual'],16)
            elif tag=='FC067_CT_GATHER':
                sample=int(fields['sample']);component=int(fields['component']);copy=gen,sample
                own=reads.setdefault(copy,[])
                require(state['seams']==15 and component==len(own)<3,'copy read sequence')
                require(int(fields['address'],16)==bases[slot]+4*component
                        and int(fields['value'],16)==int(fields['expected'],16)==state['words'][component],'read bytes')
                require(not own or own[0][0]==identity,'copy crosses incarnations')
                own.append((identity,int(fields['value'],16),position));state['reads']+=1
            fields['slot']=str(state['virtual'])
            line=tag+' '+' '.join(k+'='+v for k,v in fields.items())
        if tag=='FC067_LEDGER_BIND':
            sample=int(fields['sample']);require(sample not in bindings,'duplicate consumer binding')
            bindings[sample]=fields,position
        if include_text:output.append(line)
    require(set(prefixes)==set(begins) and prefetch is None,'unclosed frames/prefetch')
    for state in current.values():complete(state)
    require(set(bindings)=={r['sample'] for r in rows},'consumer coverage')
    selected={};selected_copies=set()
    for row in rows:
        bind,pos=bindings[row['sample']];gen=row['generation'];copy=gen,int(bind['copy_id'])
        require(copy not in selected_copies,'reused selected copy');selected_copies.add(copy)
        require(int(bind['generation'])==gen and bind['exact']=='1'
                and int(bind['vertex'])==row['vertex'] and int(bind['base'],16)==row['ram_x']
                and int(bind['pointer'],16)==row['decoder_pointer'],'consumer identity')
        own=reads.get(copy,[]);require(len(own)==3,'selected read coverage')
        identity=own[0][0];state=lifetimes[identity]
        require(all(r[0]==identity and r[2]<pos for r in own)
                and [r[1] for r in own]==row['after'][1:4],'selected incarnation bytes')
        require(begins[gen]<=row['cycle']<=prefixes[gen],'consumer frame clock')
        selected[row['sample']]=identity
    result=dict(lifetimes=len(lifetimes),bound_consumers=len(selected),completed_prefetches=prefetches,
                selected_lifetimes=len(set(selected.values())),
                unused_lifetimes=sum(s['reads']==0 for s in lifetimes.values()),
                lifecycle_verified=True,arithmetic_verified=False,world_camera_recovered=False)
    if include_text:result.update(text='\n'.join(output)+'\n',selected=selected,
                                 selected_slots={sample:(identity[0],lifetimes[identity]['virtual']) for sample,identity in selected.items()})
    return result
