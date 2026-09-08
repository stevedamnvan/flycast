"""Check four executed transform seams and RAM generations; never infer camera semantics."""
import argparse
import json
from pathlib import Path
import re

from camera_source_inspect import inspect_capture as inspect_sources
from initial_edge_inspect import inspect_edge as inspect_supply
from initial_record_inspect import inspect_block as inspect_initial
from factor_edge_inspect import inspect_edge as inspect_pred
from record_calc_inspect import inspect_block as inspect_calc
from extra_writer_inspect import inspect as inspect_extra
from transform_span_inspect import records
from transform_store_inspect import load_session, require

TAGS=('SUPPLY','INITIAL','PRED','CALC')


def inspect(session, sources):
    require('FC067_CT_REJECT' not in session,'live transform observer rejected')
    begins=records(session,'FC067_CT_BEGIN'); ends=records(session,'FC067_CT_FRAME')
    require(len(begins)==len(ends)==3,'transform frame coverage')
    headers=records(session,'FC067_CT_BLOCK')
    starts=[m.start() for m in re.finditer('FC067_CT_BLOCK ',session)]
    require(len(headers)==len(starts)==60,'five-record four-seam coverage')
    extras={}
    for match in re.finditer(r'FC067_EXTRA_FILTER ',session):
        section=session[match.start():]
        finish=section.index('\n',section.index('FC067_EXTRA_EXIT '))
        section=section[:finish]
        selected=records(section,'FC067_EXTRA_SELECTION')[0]
        key=int(selected['generation']),int(selected['slot'])
        require(key not in extras,'duplicate accumulation')
        extras[key]=inspect_extra(section,int(selected['target'],16),key[0],False)
    require(len(extras)==6,'accumulation coverage')
    groups={}
    for i,header in enumerate(headers):
        key=tuple(int(header[k]) for k in ('generation','slot','kind'))
        require(key not in groups and 0<=key[1]<6 and 0<=key[2]<4,'duplicate or invalid seam')
        section=session[starts[i]:starts[i+1] if i+1<len(starts) else len(session)]
        entry=records(section,'FC067_'+TAGS[key[2]]+'_ENTRY')
        require(len(entry)==1 and entry[0]['cycle']==header['cycle'],'seam entry identity')
        groups[key]=(header,section)
    output=[]
    xloads=records(session,'FC067_X_LOAD'); xedges=records(session,'FC067_X_EDGE')
    require(len(xloads)==len(xedges)==15,'initial X load coverage')
    require([int(r['count']) for r in xloads]==list(range(1,16)),'X load event count')
    require(len({(r['generation'],r['slot']) for r in xloads})==15,'duplicate X load')
    for i,sample in enumerate(sources['samples']):
        generation=sample['generation']; slot=sample['slot']; producer=sample['producer']
        if slot==3:
            require(not any((generation,slot,k) in groups for k in range(4)), 'unsupported record unexpectedly traced')
            continue
        begin=begins[i//6]; end=ends[i//6]
        require(int(begin['generation'])==int(end['generation'])==generation
                and int(begin['ordinal_hint'])==int(end['ordinal'])==producer['ordinal']
                and end['complete']=='55' and end['counts']==','.join([str((i//6+1)*5)]*4),
                'transform actual frame coverage')
        parts=[groups[generation,slot,kind] for kind in range(4)]
        base=int(sample['position_ram_addresses'][0],16)
        for header,section in parts:
            require(int(header['base'],16)==base and int(header['ordinal_hint'])==producer['ordinal']
                    and int(begin['cycle'])<=int(header['cycle'])<=producer['cycle'],'record target identity')
        supply=inspect_supply(parts[0][1],base,parts[0][0]['cycle'],str(generation))
        initial=inspect_initial(parts[1][1],base,parts[1][0]['cycle'])
        pred=inspect_pred(parts[2][1],base,parts[2][0]['cycle'])
        calc=inspect_calc(parts[3][1],base,parts[3][0]['cycle'])
        require(supply['transformed_words'][:3]==initial['initial_xyz_words'],'FTRV initial store binding')
        projected_input=initial['initial_xyz_words']
        extra=extras.get((generation,slot))
        if extra:
            require([v for a,v in extra['loads'][5:]]==projected_input,'accumulation prior record binding')
            projected_input=[v for a,v in extra['stores']][::-1]
        load=[r for r in xloads if int(r['generation'])==generation and int(r['slot'])==slot]
        edge=[r for r in xedges if int(r['generation'])==generation and int(r['slot'])==slot]
        require(len(load)==len(edge)==1,'X generation coverage')
        load,edge=load[0],edge[0]
        require(load['pc']=='8c03c9d6' and int(load['address'],16)==base
                and int(load['value'],16)==int(load['expected'],16)==projected_input[0]
                and int(load['writer'])==(6 if extra else 3),'X load ledger binding')
        require(edge['block']=='8c03c9a4' and int(edge['pointer'],16)==base+4
                and int(edge['value'],16)==int(edge['expected'],16)==projected_input[0]
                and edge['exact']=='1' and 16 not in pred['written_registers'],'X entry/register preservation')
        pred_edges=records(parts[2][1],'FC067_PRED_EDGE')
        require(int(pred_edges[0]['x'],16)==projected_input[0],'X exit register continuity')
        load_pos=session.index(f'FC067_X_LOAD slot={slot} generation={generation} ')
        edge_pos=session.index(f'FC067_X_EDGE slot={slot} generation={generation} ')
        pred_pos=session.index(f'FC067_CT_BLOCK generation={generation} ordinal_hint={producer["ordinal"]} slot={slot} kind=2 ')
        calc_pos=session.index(f'FC067_CT_BLOCK generation={generation} ordinal_hint={producer["ordinal"]} slot={slot} kind=3 ')
        writer_pos=session.index(f'FC067_CT_WRITE generation={generation} slot={slot} event={6 if extra else 3} ')
        require(writer_pos<load_pos<edge_pos<pred_pos<calc_pos,'X writer/load/entry chronology')
        require(calc['final_words_xyz']==sample['position_words'],'projection gather binding')
        require(calc['live_input_words'][16]==projected_input[0]
                and calc['live_input_words'][17]==projected_input[1]
                and pred['factor_word']==calc['live_input_words'][19], 'projection input binding')
        require([v for _,v in pred['source_loads']]==projected_input[1:3], 'reciprocal record binding')
        writes=[row for row in records(session,'FC067_CT_WRITE')
                if int(row['generation'])==generation and int(row['slot'])==slot]
        n=9 if extra else 6
        require(len(writes)==n and [int(r['event']) for r in writes]==list(range(1,n+1)), 'record write coverage')
        if extra:
            for row,(address,value) in zip(writes[3:6],extra['stores']):
                require(row['kind']=='4' and int(row['address'],16)==address
                        and int(row['source'],16)==int(row['actual'],16)==value, 'accumulation write binding')
        for kind,values,pc_base in ((1,initial['initial_xyz_words'],0x8c03c94e),(3,calc['final_words_xyz'],0x8c03c9ca)):
            block=parts[kind][1]
            observed=records(block,'FC067_'+TAGS[kind]+'_STORE')
            own=writes[:3] if kind==1 else writes[-3:]
            require(len(observed)==3,'operand store coverage')
            for j,(row,store) in enumerate(zip(own,observed)):
                require(int(row['kind'])==kind and int(row['address'],16)==base+8-4*j
                        and int(row['pc'],16)==pc_base+2*j
                        and row['source']==row['actual'] and int(row['actual'],16)==values[2-j]
                        and row['address']==store['address'] and row['pc']==store['pc']
                        and int(row['actual'],16)==int(store['value'],16),'actual RAM store differs from operand')
                require(int(parts[kind][0]['cycle'])<=int(row['cycle'])<=producer['cycle'],'RAM store clock')
        gather=[row for row in records(session,'FC067_CT_GATHER') if int(row['sample'])==sample['sample']]
        require(len(gather)==3 and [int(r['component']) for r in gather]==[0,1,2],'RAM consumption coverage')
        for j,row in enumerate(gather):
            require(int(row['generation'])==generation and int(row['slot'])==slot
                    and int(row['address'],16)==base+4*j and row['value']==row['expected']
                    and int(row['value'],16)==sample['position_words'][j] and int(row['events'])==n
                    and row['writers']==','.join([str(n-j)]*4),'RAM last-writer association')
        output.append(dict(sample=sample['sample'],producer=producer,draw=sample['draw'],vertex=sample['vertex'],
                           source_point_words=supply['source_point_words'],xf_matrix_words=supply['xf_matrix_words'],
                           source_loads=supply['source_loads'],transformed_words=supply['transformed_words'],
                           final_words=calc['final_words_xyz']))
    require(len(records(session,'FC067_CT_WRITE'))==108 and len(records(session,'FC067_CT_GATHER'))==45,
            'aggregate record bounds')
    return dict(samples=output,observations=15,unsupported_slots=[3],executed_seams=60,accumulations=6,mathematical_projection_verified=True,
                initial_x_reload_proven=True,source_coordinate_space='unknown',world_camera_recovered=False,
                production_enabled=False)


def inspect_capture(path):
    return inspect(load_session(path),inspect_sources(path))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture',required=True,type=Path)
    args=parser.parse_args()
    try: print(json.dumps(inspect_capture(args.capture),sort_keys=True))
    except (ValueError,OSError,KeyError,IndexError,TypeError) as error:
        parser.exit(1,f'Camera transform rejected: {error}\n')
