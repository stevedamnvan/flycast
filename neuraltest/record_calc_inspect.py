"""Replay the selected final coordinate block, without inventing world/camera inputs."""
import argparse
import json
from pathlib import Path
import re

from binary32_oracle import fraction, rounded_bits
from ram_record_inspect import inspect as inspect_record
from transform_span_inspect import records
from transform_store_inspect import load_session, require
from xyz_operand_inspect import operand, reg_key


def calculate(name, a, b, mode):
    if name == 'fmul':
        return rounded_bits(fraction(a)*fraction(b), mode)
    if name == 'fadd':
        return rounded_bits(fraction(a)+fraction(b), mode)
    if name in ('mov32', 'jcond'):
        return a
    if name == 'sub':
        return (a-b) & 0xffffffff
    if name == 'add':
        return (a+b) & 0xffffffff
    if name == 'seteq':
        return int(a == b)
    raise ValueError('unsupported calculation: '+name)


def inspect_block(session):
    require('FC067_CALC_REJECT' not in session, 'live calculation rejected')
    entries, exits = records(session, 'FC067_CALC_ENTRY'), records(session, 'FC067_CALC_EXIT')
    require(len(entries) == len(exits) == 1, 'not one selected block')
    entry, finish = entries[0], exits[0]
    require(entry.get('block') == '8c03c9c0' and entry.get('cycle') == '7602640640'
            and entry.get('ops') == '17' and entry.get('inputs') == '7', 'wrong selected block')
    require('mxcsr' in entry and 'fpscr' in entry and 'mxcsr' in finish, 'missing floating-point mode')
    mxcsr = int(entry['mxcsr'], 16)
    require((mxcsr & 0xe040) == (int(finish['mxcsr'], 16) & 0xe040), 'floating-point mode changed')
    mode = (mxcsr >> 13) & 3
    inputs, ops = records(session, 'FC067_CALC_INPUT'), records(session, 'FC067_CALC_OP')
    require(len(inputs) == 7 and len(ops) == 17, 'incomplete descriptor')
    suffixes = [0xc0,0xc2,0xc4,0xc6,0xc8,0xca,0xca,0xcc,0xcc,0xce,0xce,0xd0,0xd0,0xd2,0xd4,0xd6,0xd6]
    require([int(op['pc'],16) for op in ops] == [0x8c03c900+x for x in suffixes], 'wrong guest instruction PCs')
    state, live = {}, {}
    for row in inputs:
        reg, value = int(row['reg']), int(row['value'], 16)
        require(reg not in live and row.get('expected') == row['value'] and row.get('exact') == '1', 'input mismatch')
        require(0 <= value <= 0xffffffff, 'input word range')
        state[reg, 0] = live[reg] = value
    require(set(live) == {4,5,16,17,19,20,21}, 'unexpected live-in set')
    require(live[4] == 0x8ce7425c, 'wrong record pointer')
    dynamic = []
    for line in session.splitlines():
        match = re.search(r'FC067_CALC_(VALUE|READ|STORE) ', line)
        if match:
            dynamic.append((match[1], records(line, 'FC067_CALC_'+match[1])[0]))
    require(len(dynamic) == int(finish['events']) == 18 and finish['selected'] == '1'
            and finish['descriptor'] == entry['descriptor'], 'incomplete dynamic event set')
    cursor, stores, read_count, float_count = 0, [], 0, 0
    def take(kind, index=None):
        nonlocal cursor
        require(cursor < len(dynamic) and dynamic[cursor][0] == kind, 'wrong event order')
        row = dynamic[cursor][1]
        cursor += 1
        require(int(row['event']) == cursor, 'event sequence mismatch')
        if index is not None:
            require(int(row['index']) == index, 'event operation mismatch')
        return row
    for index, op in enumerate(ops):
        require(int(op['index']) == index and op['rd2'] == '-', 'unsupported descriptor layout')
        a, b, c = (operand(op[key], state) for key in ('rs1','rs2','rs3'))
        if op['op'] == 'writem':
            require(op['rd'] == '-' and op['size'] == '4', 'store contract')
            row = take('STORE')
            require(row['pc'] == op['pc'] and row['size'] == '4'
                    and int(row['address'],16) == (a+c)&0xffffffff
                    and int(row['value'],16) == b, 'store address/value differs from SSA')
            stores.append(row)
            continue
        expected = None
        if op['op'] == 'readm':
            row = take('READ', index)
            require(op['size'] == row['size'] == '4'
                    and int(row['address'],16) == (a+c)&0xffffffff, 'load address differs from SSA')
            read_count += 1
        else:
            require(op['size'] == '0' and op['rs3'] == '-', 'unsupported arithmetic shape')
            require((op['rs2'] == '-') == (op['op'] in ('mov32','jcond')), 'wrong unary/binary operand shape')
            expected = calculate(op['op'], a, b, mode)
            float_count += op['op'] in ('fmul','fadd')
        row = take('VALUE', index)
        key = reg_key(op['rd'])
        require(row['slot'] == row['part'] == '0' and row['count'] == '1'
                and (int(row['reg']),int(row['version'])) == key and key not in state, 'wrong/stale result register')
        value = int(row['value'],16)
        require(0 <= value <= 0xffffffff and (expected is None or value == expected), 'arithmetic result mismatch')
        state[key] = value
    require(cursor == len(dynamic) and len(stores) == 3 and read_count == 1 and float_count == 4, 'wrong calculation shape')
    require([int(row['pc'],16) for row in stores] == [0x8c03c9ca,0x8c03c9cc,0x8c03c9ce]
            and [int(row['address'],16) for row in stores] == [0x8ce74258,0x8ce74254,0x8ce74250], 'wrong final stores')
    return {'final_words_xyz': [int(row['value'],16) for row in reversed(stores)],
            'block_ops': 17, 'block_events': 18, 'float_ops_verified': 4,
            'mxcsr_rounding_mode': mode, 'live_input_words': live,
            'final_coordinate_block_proven': True, 'world_camera_recovered': False,
            'depth_factor_origin_proven': False}


def inspect(session, scene, manifest):
    result = inspect_record(session, scene, manifest)
    block = inspect_block(session)
    require(block['final_words_xyz'] == result['record_words'], 'calculation differs from consumed record')
    # Buffer emits only after the selected invocation. Require the actual final
    # writes to precede it, and consumption to follow it, in the same log session.
    positions = [session.index('FC067_RAM_WRITE event=6 '), session.index('FC067_CALC_ENTRY '),
                 session.index('FC067_CALC_EXIT '), session.index('FC067_RAM_READ ')]
    require(positions == sorted(positions), 'calculation/record chronology mismatch')
    result.update(block)
    return result


def check_controls(session):
    inputs = records(session, 'FC067_CALC_INPUT')
    row = next(row for row in inputs if row['reg'] == '20')
    old = f"reg=20 value={row['value']} expected={row['expected']}"
    changed_word = f"{int(row['value'],16)^1:x}"
    def mutate(old, new):
        lines = session.splitlines()
        for i,line in enumerate(lines):
            if 'FC067_CALC_' in line and old in line:
                lines[i] = line.replace(old, new, 1)
                return '\n'.join(lines)+'\n'
        raise ValueError('control target missing')
    changes = [mutate(old, f'reg=20 value={changed_word} expected={changed_word}'),
               mutate('op=fmul', 'op=fadd'),
               mutate('rs2=r19:1:0', 'rs2=r20:1:0'),
               mutate('pc=8c03c9cc address=8ce74254', 'pc=8c03c9cc address=8ce74250'),
               mutate('event=18 index=16', 'event=17 index=16')]
    for changed in changes:
        require(changed != session, 'control did not mutate')
        try:
            inspect_block(changed)
        except ValueError:
            continue
        raise ValueError('wrong calculation control accepted')
    return len(changes)


def inspect_capture(capture):
    frame = capture/'frame-001782'
    def read(name):
        path = frame/name
        require(path.stat().st_size <= 8*1024*1024, 'JSON exceeds bound')
        return json.loads(path.read_text(encoding='utf-8'))
    session = load_session(capture)
    result = inspect(session, read('pvr-scene.json'), read('manifest.json'))
    result['calculation_controls_rejected'] = check_controls(session)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', required=True, type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(inspect_capture(args.capture), sort_keys=True))
    except (ValueError, OSError, KeyError, IndexError, TypeError) as error:
        parser.exit(1, f'Coordinate calculation rejected: {error}\n')
