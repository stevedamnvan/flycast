"""Bounded arithmetic witness only; does not accept the rejected camera chain."""
import argparse
import json
import re
from pathlib import Path

from binary32_oracle import fraction, rounded_bits
from initial_edge_inspect import vector_keys
from transform_span_inspect import records
from transform_store_inspect import load_session, require
from xyz_operand_inspect import operand, calculate


def inspect(session, target=0x8ce6e478, generation=1790, require_chain_rejection=True, candidate_limit=None):
    if candidate_limit is None: candidate_limit=6 if require_chain_rejection else 108
    require(type(candidate_limit) is int and 1<=candidate_limit<=16578,'candidate budget')
    def rows(tag):
        return records(session, 'FC067_EXTRA_'+tag)
    def one(tag):
        found = rows(tag)
        require(len(found) == 1, 'missing/duplicate '+tag)
        return found[0]
    require('FC067_EXTRA_REJECT' not in session, 'extra observer rejected')
    entry, end, selected, filtered = [one(t) for t in ('ENTRY', 'EXIT', 'SELECTION', 'FILTER')]
    require(entry['block'] == '8c03c95c' and entry['ops'] == '31'
            and entry['inputs'] == '21' and end['events'] == '42'
            and end['selected'] == '1' and entry['descriptor'] == end['descriptor'], 'descriptor')
    require(int(selected['generation']) == generation and selected['pc'] == '8c03c97c'
            and int(selected['target'],16) == int(selected['expected_target'],16) == target
            and 1 <= int(selected['candidate']) <= candidate_limit
            and 1 <= int(filtered['checks']) <= 4096, 'selection')
    require(entry['fpscr'] == '40001' and int(entry['mxcsr'],16) & 0xe040 == 0xe040
            and int(entry['mxcsr'],16) & 0xe040 == int(end['mxcsr'],16) & 0xe040, 'FP mode')
    mode = 3
    state = {}
    for row in rows('INPUT'):
        key = int(row['reg']), 0
        require(key not in state and row['value'] == row['expected'] and row['exact'] == '1', 'live input')
        state[key] = int(row['value'],16)
    require(set(state) == {(r,0) for r in (1,2,4,5,7,*range(32,48))}, 'live set')
    require(state[1,0] == 0xfff0 and state[2,0] == 4, 'index contract')
    ops = rows('OP')
    names = ['readm','add']*3 + ['readm','readm','add','shld','and','ftrv','add']
    names += ['readm','add']*3 + ['fadd']*3 + ['sub','seteq','sub','writem','sub','writem','jcond','sub','writem']
    offsets = [0,0,2,2,4,4,6,8,8,10,12,14,16,18,18,20,20,22,22,24,26,28,30,30,32,32,34,34,36,38,38]
    require(len(ops) == 31 and [o['op'] for o in ops] == names
            and [int(o['pc'],16) for o in ops] == [0x8c03c95c+x for x in offsets], 'program')
    events = []
    for line in session.splitlines():
        match = re.search(r'FC067_EXTRA_(READ|VALUE|STORE) ',line)
        if match:
            events.append((match[1], records(line,'FC067_EXTRA_'+match[1])[0]))
    require(len(events) == 42, 'event coverage')
    cursor = 0
    loads, stores = [], []
    def take(kind, index):
        nonlocal cursor
        require(cursor < len(events) and events[cursor][0] == kind, 'event order')
        row = events[cursor][1]
        cursor += 1
        require(int(row['event']) == cursor and (kind == 'STORE' or int(row['index']) == index), 'event identity')
        return row
    for index, op in enumerate(ops):
        name = op['op']
        require(int(op['index']) == index and op['rd2'] == op['rs3'] == '-', 'operation shape')
        require(op['size'] == ('4' if name in ('readm','writem') else '0'), 'operation size')
        if name == 'ftrv':
            require(vector_keys(op['rs1'],4) == [(r,1) for r in range(16,20)]
                    and vector_keys(op['rs2'],16) == [(r,0) for r in range(32,48)], 'matrix layout')
            point = [state[k] for k in vector_keys(op['rs1'],4)]
            matrix = [state[k] for k in vector_keys(op['rs2'],16)]
            expected = [rounded_bits(sum(fraction(matrix[4*j+i])*fraction(point[j]) for j in range(4)), mode) for i in range(4)]
            transformed = expected[:]
        else:
            a, b = operand(op['rs1'],state), operand(op['rs2'],state)
            if name == 'readm':
                row = take('READ',index)
                require(int(row['address'],16) == a and row['size'] == '4' and op['rs2'] == '-', 'read address')
                expected = [None]
            elif name == 'writem':
                row = take('STORE',index)
                require(row['pc'] == op['pc'] and int(row['address'],16) == a and row['size'] == '4'
                        and int(row['value'],16) == b and row['value'] == row['actual'] and row['exact'] == '1', 'actual store')
                stores.append((a,b))
                continue
            elif name == 'fadd':
                expected = [rounded_bits(fraction(a)+fraction(b),mode)]
            elif name == 'sub':
                expected = [(a-b)&0xffffffff]
            elif name == 'seteq':
                expected = [int(a == b)]
            else:
                expected = [calculate(name,a,b)]
        for part, (key, value) in enumerate(zip(vector_keys(op['rd'],len(expected)),expected)):
            row = take('VALUE',index)
            actual = int(row['value'],16)
            require(key not in state and (int(row['reg']),int(row['version'])) == key
                    and row['slot'] == '0' and row['count'] == '1' and int(row['part']) == part
                    and (value is None or value == actual), 'arithmetic/SSA result')
            state[key] = actual
            if name == 'readm':
                loads.append((a,actual))
    source = state[5,0]
    require([a for a,v in loads] == [source+i for i in (0,4,8,12,12)]+[target-8+i for i in (0,4,8)], 'load domains')
    require(loads[3] == loads[4] == (int(filtered['address'],16),int(filtered['word'],16)), 'filter versus executed loads')
    require((((loads[3][1]<<4)&0xfff0)+state[4,0]+8)&0xffffffff == target, 'indexed target')
    require([a for a,v in stores] == [target,target-4,target-8]
            and [v for a,v in stores][::-1] == [state[r,3] for r in range(16,19)], 'XYZ stores')
    require([state[r,3] for r in range(16,19)] == [rounded_bits(fraction(transformed[i])+fraction(loads[5+i][1]),mode) for i in range(3)], 'accumulation')
    unknown = records(session,'FC067_CT_UNKNOWN_WRITE')
    if require_chain_rejection:
        require(len(unknown) == 1 and unknown[0]['slot'] == '4' and unknown[0]['pc'] == '8c03c97c'
                and int(unknown[0]['address'],16) == target, 'missing retained chain rejection')
    else:
        require(not unknown, 'unexpected unknown writer')
    return dict(scope='one-extra-writer-arithmetic-only', source_words=point,
                transformed_words=transformed, stores=stores, loads=loads, events=cursor,
                accumulation_verified=True, complete_source_chain=False, world_camera_recovered=False)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture',type=Path)
    args = parser.parse_args()
    print(json.dumps(inspect(load_session(args.capture)),indent=2))
