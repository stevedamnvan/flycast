"""Prove one executed reciprocal-record-depth edge, not camera-space semantics."""
import argparse
import json
from pathlib import Path
import re

from binary32_oracle import evaluate, fraction
from record_calc_inspect import inspect as inspect_calculation
from transform_span_inspect import records
from transform_store_inspect import load_session, require
from xyz_operand_inspect import operand, reg_key


def inspect_edge(session):
    require('FC067_PRED_REJECT' not in session, 'live predecessor rejected')
    entries, exits, edges = (records(session, 'FC067_PRED_'+tag) for tag in ('ENTRY','EXIT','EDGE'))
    require(len(entries) == len(exits) == len(edges) == 1, 'not one predecessor edge')
    entry, finish, edge = entries[0], exits[0], edges[0]
    require(entry['block'] == '8c03c9a4' and entry['cycle'] == '7602640640'
            and entry['ops'] == '7' and entry['inputs'] == '2', 'wrong predecessor')
    require(finish['next'] == edge['block'] == '8c03c9c0'
            and finish['pointer'] == edge['pointer'] == '8ce7425c'
            and finish['cycle'] == edge['cycle'] == entry['cycle']
            and finish['descriptor'] == entry['descriptor'], 'wrong executed edge identity')
    require(finish['factor'] == edge['factor'] == edge['expected'] and edge['exact'] == '1', 'factor changed across edge')
    require('fpscr' in entry and 'mxcsr' in entry and 'mxcsr' in finish, 'missing mode')
    mxcsr = int(entry['mxcsr'],16)
    require(mxcsr & 0xe040 == int(finish['mxcsr'],16) & 0xe040, 'mode changed')
    mode = (mxcsr >> 13) & 3
    inputs, ops = records(session,'FC067_PRED_INPUT'), records(session,'FC067_PRED_OP')
    require(len(inputs) == 2 and len(ops) == 7, 'descriptor coverage')
    state = {}
    for row in inputs:
        key = (int(row['reg']),0)
        require(key not in state and row['value'] == row['expected'] and row['exact'] == '1', 'input mismatch')
        state[key] = int(row['value'],16)
    require(set(state) == {(4,0),(23,0)} and state[4,0] == 0x8ce74254, 'wrong input identity')
    names = ['readm','add','readm','add','mov32','fsetgt','fdiv']
    pcs = [0x8c03c9a4,0x8c03c9a4,0x8c03c9a6,0x8c03c9a6,0x8c03c9a8,0x8c03c9aa,0x8c03c9ac]
    require([op['op'] for op in ops] == names and [int(op['pc'],16) for op in ops] == pcs, 'wrong predecessor program')
    dynamic=[]
    for line in session.splitlines():
        match=re.search(r'FC067_PRED_(READ|VALUE|STORE) ',line)
        if match:
            dynamic.append((match[1],records(line,'FC067_PRED_'+match[1])[0]))
    require(len(dynamic) == int(finish['events']) == 9, 'wrong event count')
    cursor, loads = 0, []
    def take(kind,index):
        nonlocal cursor
        require(cursor < len(dynamic) and dynamic[cursor][0] == kind, 'event order')
        row=dynamic[cursor][1]; cursor+=1
        require(int(row['event']) == cursor and int(row['index']) == index, 'event identity')
        return row
    for index,op in enumerate(ops):
        require(int(op['index']) == index and op['rd2'] == op['rs3'] == '-', 'unsupported shape')
        a,b=(operand(op[key],state) for key in ('rs1','rs2'))
        expected=None; address=None
        if op['op']=='readm':
            read=take('READ',index)
            require(op['size'] == read['size'] == '4' and op['rs2'] == '-'
                    and int(read['address'],16) == a, 'wrong actual load address')
            address=a
        else:
            require(op['size'] == '0', 'arithmetic size')
            if op['op']=='add': expected=(a+b)&0xffffffff
            elif op['op']=='mov32':
                require(a==0x3f800000 and op['rs2']=='-', 'not observed unit numerator')
                expected=a
            elif op['op']=='fsetgt': expected=int(fraction(a)>fraction(b))
            elif op['op']=='fdiv': expected=evaluate('div',a,b,0,mode)
        row=take('VALUE',index); key=reg_key(op['rd'])
        require(row['slot'] == row['part'] == '0' and row['count'] == '1'
                and (int(row['reg']),int(row['version'])) == key and key not in state, 'wrong/stale SSA result')
        value=int(row['value'],16)
        require(0<=value<=0xffffffff and (expected is None or value==expected), 'arithmetic mismatch')
        state[key]=value
        if address is not None: loads.append((address,value))
    require(cursor==len(dynamic) and [address for address,_ in loads]==[0x8ce74254,0x8ce74258], 'load coverage')
    factor=state[reg_key(ops[-1]['rd'])]
    require(factor==int(edge['factor'],16) and state[4,2]==int(edge['pointer'],16), 'exit differs from executed results')
    # Emitted immediately at the predecessor exit, then at the very next JIT
    # block entry. No silent intervening block is allowed by the live pending guard.
    require(session.index('FC067_PRED_ENTRY ') < session.index('FC067_PRED_EXIT ')
            < session.index('FC067_PRED_EDGE '), 'buffer/edge chronology')
    return {'factor_word':factor,'source_loads':loads,'predecessor_ops':7,'predecessor_events':9,
            'reciprocal_record_depth_proven':True,'world_camera_recovered':False,
            'record_z_coordinate_system':'unknown','mxcsr_rounding_mode':mode}


def source_record(session,loads):
    prefix=session[:session.index('FC067_PRED_ENTRY ')]
    state,writers=[None]*12,[0]*12
    for row in records(prefix,'FC067_RAM_BYTE'):
        byte=int(row['byte']); state[byte]=int(row['actual'],16); writers[byte]=int(row['event'])
    require(all(value is not None for value in state), 'source generation missing')
    source_writers=[]
    for address,value in loads:
        base=address-0x8ce74250
        require(base in (4,8),'source read outside selected Y/Z')
        require(value==int.from_bytes(bytes(state[base:base+4]),'little'), 'load differs from prior record writers')
        source_writers.append(writers[base:base+4])
    return source_writers


def inspect(session,scene,manifest):
    result=inspect_calculation(session,scene,manifest)
    edge=inspect_edge(session)
    source_writers=source_record(session,edge['source_loads'])
    require(edge['factor_word']==result['live_input_words'][19], 'factor not consumed by final block')
    require(edge['source_loads'][0][1]==result['live_input_words'][17], 'loaded Y not retained into final block')
    positions=[session.index(tag) for tag in ('FC067_RAM_WRITE event=3 ', 'FC067_PRED_ENTRY ',
               'FC067_PRED_EXIT ','FC067_PRED_EDGE ','FC067_RAM_WRITE event=4 ','FC067_CALC_ENTRY ')]
    require(positions==sorted(positions), 'source/overwrite/consumer chronology mismatch')
    require(edge['mxcsr_rounding_mode']==result['mxcsr_rounding_mode'], 'consumer rounding mismatch')
    result.update(edge); result['source_byte_writers_yz']=source_writers
    result['depth_factor_origin_proven']=True
    return result


def check_controls(session):
    def mutate(old,new):
        lines=session.splitlines()
        for i,line in enumerate(lines):
            if 'FC067_PRED_' in line and old in line:
                lines[i]=line.replace(old,new,1); return '\n'.join(lines)+'\n'
        raise ValueError('control target missing')
    changes=[mutate('next=8c03c9c0','next=8c03c9c2'),
             mutate('rs1=i3f800000','rs1=i40000000'),
             mutate('rs2=r18:1:1','rs2=r23:1:0'),
             mutate('index=2 address=8ce74258','index=2 address=8ce74254'),
             mutate('event=9 index=6','event=8 index=6')]
    for changed in changes:
        require(changed!=session,'control did not mutate')
        try: inspect_edge(changed)
        except ValueError: continue
        raise ValueError('wrong predecessor control accepted')
    return len(changes)


def inspect_capture(capture):
    frame=capture/'frame-001782'
    def read(name):
        path=frame/name
        require(path.stat().st_size<=8*1024*1024,'JSON bound')
        return json.loads(path.read_text(encoding='utf-8'))
    session=load_session(capture)
    scene,manifest=read('pvr-scene.json'),read('manifest.json')
    result=inspect(session,scene,manifest)
    result['predecessor_controls_rejected']=check_controls(session)
    # Mutate the overwritten denominator generation consistently, so the prior
    # final-record replay alone still accepts. The actual source load must reject.
    lines=session.splitlines()
    for i,line in enumerate(lines):
        if 'FC067_RAM_BYTE event=1 byte=8 ' in line:
            row=records(line,'FC067_RAM_BYTE')[0]
            value=int(row['actual'],16)^1
            lines[i]=re.sub(r'\b(source|expected|actual)=[0-9a-f]+',lambda m:f'{m[1]}={value:02x}',line)
            break
    else: raise ValueError('source control target missing')
    mutated='\n'.join(lines)+'\n'
    inspect_calculation(mutated,scene,manifest)
    try: inspect(mutated,scene,manifest)
    except ValueError: result['source_generation_control_rejected']=True
    else: raise ValueError('wrong source generation accepted')
    return result


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture',required=True,type=Path)
    args=parser.parse_args()
    try: print(json.dumps(inspect_capture(args.capture),sort_keys=True))
    except (ValueError,OSError,KeyError,IndexError,TypeError) as error:
        parser.exit(1,f'Factor predecessor rejected: {error}\n')
