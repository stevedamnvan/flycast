"""Verify one executed FTRV predecessor of the selected opaque record stores.

The matrix is an observed XF-register value, not a recovered world camera.
Dot checks use an exact-rational mathematical reference, not an instruction-level
binary64 emulator. The actual four-component input, including W, is preserved.
"""
import argparse
import json
from pathlib import Path
import re

from binary32_oracle import fraction, rounded_bits
from initial_record_inspect import inspect as inspect_initial
from transform_span_inspect import records
from transform_store_inspect import load_session, require
from xyz_operand_inspect import operand, reg_key


def vector_keys(token, count):
    match = re.fullmatch(r'r(\d+):(\d+):([\d,]+)', token)
    require(match is not None and int(match[2]) == count, 'vector shape')
    versions = [int(v) for v in match[3].split(',')]
    require(len(versions) == count, 'vector version coverage')
    return [(int(match[1])+i, version) for i, version in enumerate(versions)]


def inspect_edge(session, base=0x8ce74250, cycle='7602512960', generation='1', next_cycle=None, first_record=False):
    prefix=4 if first_record else 0;op_count=11+prefix;event_count=23 if first_record else 18
    require('FC067_SUPPLY_REJECT' not in session, 'live predecessor rejection')
    rows = [records(session, 'FC067_SUPPLY_'+tag) for tag in ('ENTRY', 'EXIT', 'EDGE')]
    require(all(len(row) == 1 for row in rows), 'missing/duplicate predecessor edge')
    entry, finish, edge = [row[0] for row in rows]
    require(entry['block'] == ('8c03c932' if first_record else '8c03c93a') and int(entry['ops']) == op_count
            and int(entry['inputs']) == (21 if first_record else 19) and int(finish['events']) == event_count
            and entry['descriptor'] == finish['descriptor'], 'wrong bounded descriptor')
    require(entry['cycle'] == finish['cycle'] == cycle
            and edge['cycle'] == (cycle if next_cycle is None else next_cycle)
            and int(edge['cycle'])>=int(cycle), 'edge cycle')
    require(finish['next'] == edge['block'] == '8c03c94c'
            and int(finish['pointer'],16) == int(edge['pointer'],16) == base+12
            and finish['generation'] == edge['generation'] == generation and edge['exact'] == '1', 'edge identity')
    require([finish[k] for k in ('x', 'y', 'z')] == [edge[k] for k in ('x', 'y', 'z')]
            and edge['z'] == edge['expected_z'], 'edge operand mismatch')
    require(int(entry['fpscr'], 16) == 0x40001, 'unsupported FPSCR')
    mxcsr = int(entry['mxcsr'], 16)
    require((mxcsr & 0xe040) == (int(finish['mxcsr'], 16) & 0xe040), 'changed FP mode')
    mode = (mxcsr >> 13) & 3
    require(mode == 3, 'not observed toward-zero mode')
    live, state = {}, {}
    for row in records(session, 'FC067_SUPPLY_INPUT'):
        reg, value = int(row['reg']), int(row['value'], 16)
        require(reg not in live and 0 <= value <= 0xffffffff
                and row['value'] == row['expected'] and row['exact'] == '1', 'live input mismatch')
        state[reg, 0] = live[reg] = value
    require(set(live) == ({1,2,4,5,8,*range(32,48)} if first_record else {0,5,8,*range(32,48)})
            and (live[1]==0xfff0 and live[2]==4 if first_record else live[0]==base),
            'live register set or record pointer')
    ops = records(session, 'FC067_SUPPLY_OP')
    names = ['readm', 'add']*4 + ['add', 'ftrv', 'test']
    pcs = [0x8c03c93a + offset for offset in (0, 0, 2, 2, 4, 4, 6, 6, 8, 10, 12)]
    if first_record:names=['readm','shld','and','add']+names;pcs=[0x8c03c932,0x8c03c934,0x8c03c936,0x8c03c938]+pcs
    require(len(ops) == op_count and [row['op'] for row in ops] == names
            and [int(row['pc'], 16) for row in ops] == pcs, 'not observed predecessor program')
    dynamic = []
    for line in session.splitlines():
        match = re.search(r'FC067_SUPPLY_(READ|VALUE|STORE) ', line)
        if match:
            dynamic.append((match[1], records(line, 'FC067_SUPPLY_'+match[1])[0]))
    require(len(dynamic) == event_count, 'dynamic coverage')
    cursor, loads, point, matrix, output = 0, [], [], [], []

    def take(kind, index):
        nonlocal cursor
        require(cursor < len(dynamic) and dynamic[cursor][0] == kind, 'event type/order')
        row = dynamic[cursor][1]
        cursor += 1
        require(int(row['event']) == cursor and int(row['index']) == index, 'event sequence/index')
        return row

    def result(index, token, values):
        keys = vector_keys(token, len(values))
        for part, (key, expected) in enumerate(zip(keys, values)):
            row = take('VALUE', index)
            require(row['slot'] == '0' and int(row['part']) == part and row['count'] == '1'
                    and (int(row['reg']), int(row['version'])) == key and key not in state, 'stale/incorrect SSA result')
            value = int(row['value'], 16)
            require(0 <= value <= 0xffffffff and (expected is None or value == expected), 'computed result mismatch')
            state[key] = value
        return [state[key] for key in keys]

    for index, op in enumerate(ops):
        require(int(op['index']) == index and op['rd2']=='-' and op['rs3']==('ic' if first_record and index==0 else '-'), 'operation layout')
        local=index-prefix
        if local<0:
            require(op['rd']==f'r0:1:{index+1}','index destination')
            if index==0:
                require(op['size']=='2' and op['rs1']=='r5:1:0' and op['rs2']=='-','index load shape')
                read=take('READ',index);require(read['size']=='2' and int(read['address'],16)==live[5]+12,'index load address')
                index_word=result(index,op['rd'],[None])[0]
                require(index_word==((index_word&0xffff) if index_word&0x8000==0 else (index_word&0xffff)|0xffff0000),'signed index load')
            else:
                require(op['size']=='0' and op['rs1']==f'r0:1:{index}' and op['rs2']=={1:'r2:1:0',2:'r1:1:0',3:'r4:1:0'}[index],'index address program')
                a=state[0,index];expected=((a<<4)&0xffffffff) if index==1 else (a&live[1]) if index==2 else (a+live[4])&0xffffffff
                result(index,op['rd'],[expected])
                if index==3:require(expected==base,'computed record target')
        elif local < 8:
            component = local//2
            if local % 2 == 0:
                require(op['size'] == '4' and op['rs2'] == '-'
                        and op['rs1'] == f'r5:1:{component}'
                        and op['rd'] == f'r{16+component}:1:1', 'source load shape')
                address = operand(op['rs1'], state)
                row = take('READ', index)
                require(row['size'] == '4' and int(row['address'], 16) == address
                        and address == live[5]+4*component, 'source load address')
                value = result(index, op['rd'], [None])[0]
                loads.append((address, value))
            else:
                require(op['size'] == '0' and op['rs2'] == 'i4'
                        and op['rs1'] == f'r5:1:{component}'
                        and op['rd'] == f'r5:1:{component+1}', 'source increment shape')
                result(index, op['rd'], [(operand(op['rs1'], state)+4) & 0xffffffff])
        elif local == 8:
            require(op['size'] == '0' and op['rd'] == f'r0:1:{prefix+1}'
                    and op['rs1'] == f'r0:1:{prefix}' and op['rs2'] == 'ic', 'record increment shape')
            result(index, op['rd'], [(base+12) & 0xffffffff])
        elif local == 9:
            require(op['size'] == '0' and vector_keys(op['rd'], 4) == [(r, 2) for r in range(16, 20)]
                    and vector_keys(op['rs1'], 4) == [(r, 1) for r in range(16, 20)]
                    and vector_keys(op['rs2'], 16) == [(r, 0) for r in range(32, 48)], 'FTRV dependency shape')
            point = [state[key] for key in vector_keys(op['rs1'], 4)]
            matrix = [state[key] for key in vector_keys(op['rs2'], 16)]
            expected = [rounded_bits(sum(fraction(matrix[4*j+i])*fraction(point[j]) for j in range(4)), mode)
                        for i in range(4)]
            output = result(index, op['rd'], expected)
        else:
            require(op['size'] == '0' and op['rd'] == 'r68:1:1'
                    and op['rs1'] == 'r8:1:0' and op['rs2'] == f'r0:1:{prefix+1}', 'branch test shape')
            result(index, op['rd'], [int((live[8] & state[0, prefix+1]) == 0)])
    if first_record:require(index_word&0xffff==point[3]&0xffff,'index and fourth float bytes differ')
    require(cursor == event_count and output[:3] == [int(finish[k], 16) for k in ('x', 'y', 'z')],
            'transform result does not reach edge')
    positions = [session.index('FC067_SUPPLY_'+tag+' ') for tag in ('ENTRY', 'EXIT', 'EDGE')]
    require(positions == sorted(positions), 'edge chronology')
    return dict(predecessor_block=entry['block'], transform_pc='8c03c944', predecessor_ops=op_count,
                predecessor_events=event_count, source_loads=loads, source_point_words=point,
                xf_matrix_words=matrix, transformed_words=output, mxcsr_rounding_mode=mode,
                predecessor_edge_proven=True, mathematical_transform_verified=True,
                initial_coordinate_calculation_proven=True, world_camera_recovered=False,
                source_coordinate_space='unknown', matrix_provenance='live-XF-registers-upstream-untraced',
                scope='one-opaque-vertex-FTRV-to-initial-record')


def bind_initial(session, result):
    inputs = {int(row['reg']): int(row['value'], 16) for row in records(session, 'FC067_INITIAL_INPUT')}
    require(result['transformed_words'][:3] == [inputs.get(r) for r in (16, 17, 18)], 'successor coordinate binding')
    positions = [session.index(tag) for tag in ('FC067_RAM_BEGIN ', 'FC067_SUPPLY_ENTRY ',
                 'FC067_SUPPLY_EXIT ', 'FC067_SUPPLY_EDGE ', 'FC067_RAM_WRITE event=1 ', 'FC067_INITIAL_ENTRY ')]
    require(positions == sorted(positions), 'lease/edge/store chronology')
    lease = records(session, 'FC067_RAM_BEGIN')
    require(len(lease) == 1 and lease[0]['generation'] == '1' and lease[0]['start'] == '8ce74250', 'source lease')
    require('FC067_RAM_STOP' not in session[positions[0]:positions[3]], 'terminated source lease')


def inspect(session, scene, manifest):
    result = inspect_initial(session, scene, manifest)
    edge = inspect_edge(session)
    bind_initial(session, edge)
    # Earlier verifiers correctly kept upstream-calculation flags false in
    # their narrower scope; do not flatten those flags into this new witness.
    return dict(prior_store_chain=result, **edge)


def check_controls(session):
    def change(tag, old, new):
        lines = session.splitlines()
        for i, line in enumerate(lines):
            if 'FC067_SUPPLY_'+tag+' ' in line and old in line:
                lines[i] = line.replace(old, new, 1)
                return '\n'.join(lines)+'\n'
        raise ValueError('control target missing')
    z = records(session, 'FC067_SUPPLY_EDGE')[0]['z']
    input_row = next(row for row in records(session, 'FC067_SUPPLY_INPUT') if row['reg'] == '44')
    original = f"value={input_row['value']} expected={input_row['value']}"
    # Negating translation is large enough to survive final binary32 rounding.
    wrong = int(input_row['value'], 16) ^ 0x80000000
    variants = [change('EDGE', 'block=8c03c94c', 'block=8c03c94e'),
                change('EDGE', 'generation=1', 'generation=2'),
                change('EDGE', f'expected_z={z}', f'expected_z={int(z,16)^1:x}'),
                change('INPUT', original, f'value={wrong:x} expected={wrong:x}'),
                change('VALUE', 'event=18 index=10', 'event=17 index=10'),
                change('OP', 'rs1=r16:4:1,1,1,1', 'rs1=r16:4:0,0,0,0')]
    for changed in variants:
        try:
            inspect_edge(changed)
        except ValueError:
            continue
        raise ValueError('wrong predecessor control accepted')
    return len(variants)


def inspect_capture(capture):
    frame = capture/'frame-001782'
    def read(name):
        path = frame/name
        require(path.stat().st_size <= 8*1024*1024, 'JSON bound')
        return json.loads(path.read_text(encoding='utf-8'))
    session = load_session(capture)
    result = inspect(session, read('pvr-scene.json'), read('manifest.json'))
    result['initial_edge_controls_rejected'] = check_controls(session)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', required=True, type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(inspect_capture(args.capture), sort_keys=True))
    except (ValueError, OSError, KeyError, IndexError, TypeError) as error:
        parser.exit(1, f'Initial predecessor rejected: {error}\n')
