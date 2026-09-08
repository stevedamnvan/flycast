"""Verify initial record store operands; report coordinate calculation outside this block."""
import argparse
import json
from pathlib import Path
import re

from factor_edge_inspect import inspect as inspect_factor
from record_calc_inspect import calculate
from transform_span_inspect import records
from transform_store_inspect import load_session, require
from xyz_operand_inspect import operand, reg_key


def inspect_block(session):
    require('FC067_INITIAL_REJECT' not in session,'live initial-store rejection')
    entries,exits=(records(session,'FC067_INITIAL_'+tag) for tag in ('ENTRY','EXIT'))
    require(len(entries)==len(exits)==1,'not one initial-store invocation')
    entry,finish=entries[0],exits[0]
    require(entry['block']=='8c03c94c' and entry['cycle']=='7602512960'
            and entry['ops']=='10' and entry['inputs']=='5','wrong initial-store block')
    require(entry['descriptor']==finish['descriptor'] and finish['events']=='10'
            and finish['selected']=='1','wrong terminal witness')
    inputs,ops=records(session,'FC067_INITIAL_INPUT'),records(session,'FC067_INITIAL_OP')
    require(len(inputs)==5 and len(ops)==10,'descriptor coverage')
    state,live={},{}
    for row in inputs:
        reg,value=int(row['reg']),int(row['value'],16)
        require(reg not in live and 0<=value<=0xffffffff and row['value']==row['expected']
                and row['exact']=='1','live operand mismatch')
        state[reg,0]=live[reg]=value
    require(set(live)=={0,6,16,17,18} and live[0]==0x8ce7425c,'wrong source registers')
    names=['sub','seteq','sub','writem','sub','writem','sub','writem','jcond','add']
    pcs=[0x8c03c94c,0x8c03c94c,0x8c03c94e,0x8c03c94e,0x8c03c950,
         0x8c03c950,0x8c03c952,0x8c03c952,0x8c03c954,0x8c03c956]
    require([op['op'] for op in ops]==names and [int(op['pc'],16) for op in ops]==pcs,
            'not observed store-only program')
    dynamic=[]
    for line in session.splitlines():
        match=re.search(r'FC067_INITIAL_(VALUE|READ|STORE) ',line)
        if match: dynamic.append((match[1],records(line,'FC067_INITIAL_'+match[1])[0]))
    require(len(dynamic)==10,'wrong event coverage')
    stores=[]
    for index,op in enumerate(ops):
        require(int(op['index'])==index and op['rd2']==op['rs3']=='-','unsupported shape')
        kind,row=dynamic[index]
        require(int(row['event'])==index+1,'event order')
        a,b=(operand(op[key],state) for key in ('rs1','rs2'))
        if op['op']=='writem':
            require(kind=='STORE' and op['rd']=='-' and op['size']==row['size']=='4'
                    and row['pc']==op['pc'] and int(row['address'],16)==a
                    and int(row['value'],16)==b,'store differs from live operands')
            stores.append(row)
        else:
            require(kind=='VALUE' and op['size']=='0' and int(row['index'])==index,'result identity')
            require((op['rs2']=='-')==(op['op']=='jcond'),'unary/binary shape')
            key=reg_key(op['rd'])
            require(row['slot']==row['part']=='0' and row['count']=='1'
                    and (int(row['reg']),int(row['version']))==key and key not in state,'stale result')
            value=int(row['value'],16)
            require(value==calculate(op['op'],a,b,0),'address/count arithmetic mismatch')
            state[key]=value
    require([int(row['address'],16) for row in stores]==[0x8ce74258,0x8ce74254,0x8ce74250],
            'wrong record destination')
    require(session.index('FC067_INITIAL_ENTRY ')<session.index('FC067_INITIAL_EXIT '),'buffer chronology')
    return {'initial_xyz_words':[live[16],live[17],live[18]],'initial_store_block_proven':True,
            'initial_block_ops':10,'initial_block_events':10,'initial_coordinate_calculation_proven':False,
            'initial_block_classification':'precomputed-register-stores-only','world_camera_recovered':False}


def inspect(session,scene,manifest):
    result=inspect_factor(session,scene,manifest)
    initial=inspect_block(session)
    prefix=session[:session.index('FC067_INITIAL_ENTRY ')]
    values=[None]*12
    for row in records(prefix,'FC067_RAM_BYTE'): values[int(row['byte'])]=int(row['actual'],16)
    require(all(value is not None for value in values),'missing initial byte generation')
    words=[int.from_bytes(bytes(values[i:i+4]),'little') for i in (0,4,8)]
    require(initial['initial_xyz_words']==words,'initial stores differ from observed RAM generation')
    positions=[session.index(tag) for tag in ('FC067_RAM_WRITE event=3 ','FC067_INITIAL_ENTRY ',
               'FC067_INITIAL_EXIT ','FC067_PRED_ENTRY ','FC067_RAM_WRITE event=4 ')]
    require(positions==sorted(positions),'initial/store/consume chronology mismatch')
    result.update(initial)
    return result


def check_controls(session):
    row=next(row for row in records(session,'FC067_INITIAL_INPUT') if row['reg']=='18')
    old=f"reg=18 value={row['value']} expected={row['expected']}"
    value=int(row['value'],16)^1
    def mutate(old,new):
        lines=session.splitlines()
        for i,line in enumerate(lines):
            if 'FC067_INITIAL_' in line and old in line:
                lines[i]=line.replace(old,new,1); return '\n'.join(lines)+'\n'
        raise ValueError('control target missing')
    changes=[mutate(old,f'reg=18 value={value:x} expected={value:x}'),
             mutate('rs2=r18:1:0','rs2=r17:1:0'),
             mutate('pc=8c03c950 address=8ce74254','pc=8c03c950 address=8ce74250'),
             mutate('event=10 index=9','event=9 index=9')]
    for changed in changes:
        require(changed!=session,'control did not mutate')
        try: inspect_block(changed)
        except ValueError: continue
        raise ValueError('wrong initial-store control accepted')
    return len(changes)


def inspect_capture(capture):
    frame=capture/'frame-001782'
    def read(name):
        path=frame/name
        require(path.stat().st_size<=8*1024*1024,'JSON bound')
        return json.loads(path.read_text(encoding='utf-8'))
    session=load_session(capture)
    result=inspect(session,read('pvr-scene.json'),read('manifest.json'))
    result['initial_store_controls_rejected']=check_controls(session)
    return result


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture',required=True,type=Path)
    args=parser.parse_args()
    try: print(json.dumps(inspect_capture(args.capture),sort_keys=True))
    except (ValueError,OSError,KeyError,IndexError,TypeError) as error:
        parser.exit(1,f'Initial record witness rejected: {error}\n')
