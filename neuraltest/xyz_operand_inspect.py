"""Check executed indexed RAM loads feeding the selected XYZ stores, not their earlier producer."""
import argparse
import json
from pathlib import Path
import re

from sq_writer_inspect import inspect as inspect_writers
from transform_span_inspect import records
from transform_store_inspect import load_session, require

U32 = 0xffffffff


def reg_key(text):
    match = re.fullmatch(r'r(\d+):1:(\d+)', text)
    require(match is not None, 'unsupported SSA register parameter')
    return int(match[1]), int(match[2])


def operand(text, state):
    if text == '-':
        return 0
    if text.startswith('i'):
        return int(text[1:], 16)
    key = reg_key(text)
    require(key in state, 'missing SSA input')
    return state[key]


def calculate(name, a, b):
    if name == 'add':
        return (a+b) & U32
    if name == 'and':
        return a & b
    if name == 'shl':
        require(0 <= b < 32, 'unsupported fixed shift')
        return (a << b) & U32
    if name == 'shld':
        if b & 0x80000000:
            return 0 if b & 31 == 0 else a >> ((~b & 31)+1)
        return (a << (b & 31)) & U32
    if name == 'setge':
        signed = lambda x: x if x < 0x80000000 else x-0x100000000
        return int(signed(a) >= signed(b))
    if name == 'jcond':
        return a
    raise ValueError('unsupported observed arithmetic: '+name)


def inspect(session, scene, manifest):
    result = inspect_writers(session, scene, manifest)
    result.update(inspect_operands(session))
    return result


def inspect_operands(session, target=None, store_tag='FC067_SQ_STORE', flush_tag='FC067_SQ_FLUSH'):
    """Verify one invocation; callers must independently bind a nonlegacy target to a real packet."""
    target = target or dict(step='1', block='8c03cc68', cycle='7602643776',
                            context='00509700', ta_offset='32', sq='e0000020')
    require('FC067_XYZ_REJECT' not in session, 'operand observer rejected')
    entries, exits = records(session,'FC067_XYZ_ENTRY'), records(session,'FC067_XYZ_EXIT')
    require(len(entries) == len(exits) == 1, 'not one target invocation')
    entry, finish = entries[0], exits[0]
    require(all(entry[k] == v for k,v in target.items()), 'wrong dynamic target')
    descriptor = entry['descriptor']
    inputs, ops = records(session,'FC067_XYZ_INPUT'), records(session,'FC067_XYZ_OP')
    reads, values = records(session,'FC067_XYZ_READ'), records(session,'FC067_XYZ_VALUE')
    require(len(inputs) == int(entry['inputs']) == 6 and len(ops) == int(entry['ops']) == 27
            and len(reads) == 8 and len(values) == 20, 'incomplete operand record set')
    require(all(row['descriptor'] == descriptor for row in inputs+ops+reads+values+[finish]), 'descriptor mismatch')
    state = {}
    for row in inputs:
        key = int(row['reg']), 0
        require(key not in state and row['value'] == row['expected'] and row['exact'] == '1', 'entry operand disagreement')
        state[key] = int(row['value'],16)
    require(set(state) == {(r,0) for r in (4,5,6,9,10,14)} and state[(4,0)] == int(target['sq'],16),
            'missing live-in dependency')
    read_by_index = {int(row['index']):row for row in reads}
    value_by_index = {int(row['index']):row for row in values}
    require(len(read_by_index) == len(reads) and len(value_by_index) == len(values), 'duplicate execution value')
    section = session.split('FC067_XYZ_ENTRY ',1)[1].split('FC067_XYZ_EXIT ',1)[0]
    store_rows = records(section,store_tag)
    sq_stores = {row['pc']:row for row in store_rows}
    require(len(sq_stores) == len(store_rows) == 7, 'duplicate or missing SQ store')
    expected_stream = []
    stores = 0
    addresses = []
    position_words = []
    for index, op in enumerate(ops):
        require(int(op['index']) == index and op['rd2'] == '-', 'operation identity or unsupported second result')
        a, b, offset = [operand(op[k],state) for k in ('rs1','rs2','rs3')]
        name = op['op']
        if name == 'writem':
            require(op['rd'] == '-' and op['size'] == '4' and op['pc'] in sq_stores, 'unobserved SQ store')
            stored = sq_stores[op['pc']]
            require(int(stored['address'],16) == (a+offset)&U32 and int(stored['actual'],16) == b,
                    'SSA source does not equal actual SQ store')
            expected_stream.append(('STORE',op['pc']))
            stores += 1
            continue
        require(index in value_by_index, 'missing live result')
        row = value_by_index[index]
        key = reg_key(op['rd'])
        require(row['slot'] == row['part'] == '0' and row['count'] == '1'
                and (int(row['reg']),int(row['version'])) == key and key not in state, 'wrong SSA destination')
        actual = int(row['value'],16)
        require(0 <= actual <= U32, 'not a binary32 result')
        if name == 'readm':
            require(index in read_by_index, 'missing actual load address')
            read = read_by_index[index]
            address = (a+offset)&U32
            require(int(read['address'],16) == address and read['size'] == op['size'], 'indexed load address disagreement')
            require(op['size'] in ('2','4'), 'unsupported load width')
            if op['size'] == '2':
                word = actual & 65535
                require(actual == (word if word < 32768 else word | 0xffff0000), 'incorrect signed word load')
            expected_stream.append(('READ',str(index)))
            if index in (13,14,15):
                addresses.append(address)
                position_words.append(actual)
        else:
            require(actual == calculate(name,a,b), 'observed arithmetic disagreement')
        state[key] = actual
        expected_stream.append(('VALUE',str(index)))
    require(stores == 7 and len(addresses) == 3 and addresses == [addresses[0]+i*4 for i in range(3)],
            'incomplete contiguous XYZ source')
    section = session.split('FC067_XYZ_ENTRY ',1)[1].split('FC067_XYZ_EXIT ',1)[0]
    actual_stream = []
    for line in section.splitlines():
        for tag, field in [('FC067_XYZ_READ','index'),('FC067_XYZ_VALUE','index'),(store_tag,'pc')]:
            if tag+' ' in line:
                row = dict(re.findall(r'(\w+)=([^ ]+)',line))
                actual_stream.append((tag.rsplit('_',1)[1],row[field]))
    require(actual_stream == expected_stream and finish['events'] == '28', 'dynamic operation ordering differs')
    require(session.index('FC067_XYZ_EXIT ') < session.index(flush_tag+' '), 'XYZ invocation ends after copy')
    result = dict(position_ram_addresses=[f'{address:08x}' for address in addresses],
                  position_words=position_words,
                  indexed_address_arithmetic_proven=True, returned_xyz_loads=3,
                  position_value_calculation_proven=False, camera_recovered=False)
    return result


def mutate_input(session, reg, value):
    lines = session.splitlines()
    for i,line in enumerate(lines):
        if 'FC067_XYZ_INPUT ' in line and re.search(rf'\breg={reg} ',line):
            lines[i] = re.sub(r'\b(value|expected)=[0-9a-f]+',lambda m:f'{m[1]}={value:08x}',line)
    return '\n'.join(lines)


def check_controls(session,scene,manifest):
    inputs={int(r['reg']):int(r['value'],16) for r in records(session,'FC067_XYZ_INPUT')}
    read=next(r for r in records(session,'FC067_XYZ_READ') if r['index']=='13')
    mutations=[mutate_input(session,reg,inputs[reg]^1) for reg in (6,9,14)]
    mutations.append(session.replace('index=13 address='+read['address'],
                                    'index=13 address='+format(int(read['address'],16)^4,'08x'),1))
    mutations.append(session.replace('rs2=r12:1:1 rs3=i4','rs2=r12:1:0 rs3=i4',1))
    for mutated in mutations:
        require(mutated != session,'operand control did not mutate')
        try:
            inspect(mutated,scene,manifest)
        except ValueError:
            continue
        raise ValueError('wrong operand control accepted')
    return len(mutations)


def inspect_capture(capture):
    frame = capture/'frame-001782'
    def read(name):
        path = frame/name
        require(path.stat().st_size <= 8*1024*1024,'JSON exceeds bound')
        return json.loads(path.read_text(encoding='utf-8'))
    session,scene,manifest=load_session(capture),read('pvr-scene.json'),read('manifest.json')
    result=inspect(session,scene,manifest)
    result['offline_controls_rejected']=check_controls(session,scene,manifest)
    return result


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture',required=True,type=Path)
    args=parser.parse_args()
    try:
        print(json.dumps(inspect_capture(args.capture),sort_keys=True))
    except (ValueError,OSError,KeyError,IndexError,TypeError) as error:
        parser.exit(1,f'XYZ operand evidence rejected: {error}\n')
