"""Check recorded FTRV operands/results, not their source lineage or FP mode."""
import argparse
import json
import re
from pathlib import Path

from binary32_oracle import fraction, rounded_bits
from initial_edge_inspect import vector_keys
from transform_ledger_inspect import decode
from transform_store_inspect import require
from xyz_operand_inspect import operand


def inspect(text, mode, require_mode=False, include_calc=False):
    require(mode in (0, 1, 2, 3), 'rounding mode')
    require('REJECT' not in text, 'observer rejection')
    ops = {}
    rows = []
    for line in text.splitlines():
        match = re.search(r'FC067_PRODUCER_(\w+) (.*)', line)
        if not match:
            continue
        kind = match[1]
        if kind in ('MEMORY', 'COPY'):
            continue  # Independently consumed by producer ownership verifier.
        fields = dict(re.findall(r'(\w+)=([^ ]+)', match[2]))
        if kind == 'OP':
            key = int(fields['descriptor']), int(fields['index'])
            require(key not in ops, 'duplicate operation')
            ops[key] = fields
        else:
            rows.append((kind, fields))
    active = None
    steps = events = stores = reads = 0
    counts = {}
    scalar_counts = {}
    pending = {}
    state = {}
    for kind, row in rows:
        step = int(row['step'])
        if kind == 'ENTRY':
            require(active is None and step == steps + 1 and step <= (32768 if include_calc else 8192), 'entry sequence')
            active = steps = step
            descriptor = int(row['descriptor'])
            program = sorted((i, op) for (d, i), op in ops.items() if d == descriptor)
            require([i for i, op in program] == list(range(len(program))) and 0 < len(program) <= 128, 'operation coverage')
            expected_events = []
            for i, op in program:
                if op['op'] == 'readm': expected_events.append(('READ', i, None))
                for slot, field in enumerate(('rd', 'rd2')):
                    token = op[field]
                    if token == '-': continue
                    count = int(token.split(':')[1])
                    expected_events.extend(('VALUE', i, (slot << 16) | part) for part in range(count if count > 2 else 1))
                if op['op'] == 'writem': expected_events.append(('STORE', i, None))
            if require_mode:
                require('fpscr' in row and 'mxcsr' in row, 'missing FP mode')
                mxcsr = int(row['mxcsr'], 16)
                require(int(row['fpscr'], 16) == 0x40001 and
                        (mxcsr & 0xe040) == ((mode << 13) | 0x8040), 'unsupported FP mode')
            state, pending, events = {}, {}, 0
            continue
        require(active == step, 'event outside block')
        if kind == 'INPUT':
            key = int(row['reg']), 0
            require(int(row['descriptor']) == descriptor and key not in state, 'input identity')
            state[key] = int(row['value'], 16)
            continue
        if kind == 'EXIT':
            if require_mode:
                require('mxcsr' in row and (int(row['mxcsr'], 16) & 0xe040) == (mxcsr & 0xe040), 'changed FP mode')
            require(int(row['descriptor']) == descriptor and int(row['events']) == events == len(expected_events) and not pending, 'exit coverage')
            active = None
            continue
        require(kind in ('READ', 'VALUE', 'STORE'), 'unknown event')
        events += 1
        require(events <= 512, 'event budget')
        index = int(row['index'])
        require(events <= len(expected_events) and expected_events[events-1] == (kind, index, int(row['part']) if kind == 'VALUE' else None), 'operation event coverage/order')
        op = ops[descriptor, index]
        if kind in ('READ', 'STORE'):
            address = (operand(op['rs1'], state) + operand(op.get('rs3', '-'), state)) & 0xffffffff
            require(int(row['address'], 16) == address and row['size'] == op['size'], 'memory operand address/size')
        if kind == 'READ':
            require(op['op'] == 'readm', 'read operation')
            reads += 1
        if kind == 'STORE':
            require(op['op'] == 'writem' and row['exact'] == '1' and row['value'] == row['actual'], 'store mismatch')
            require(int(row['value'], 16) == operand(op['rs2'], state), 'store SSA value')
            stores += 1
        elif kind == 'VALUE':
            part = int(row['part'])
            slot, component = part >> 16, part & 65535
            token = op['rd'] if slot == 0 else op['rd2']
            count = int(token.split(':')[1])
            keys = vector_keys(token, count)
            if op['op'] in ('add', 'sub', 'mov32'):
                require(slot == 0 and component == 0 and count == 1, 'integer destination')
                a = operand(op['rs1'], state)
                b = operand(op['rs2'], state)
                expected = a if op['op'] == 'mov32' else ((a+b) if op['op'] == 'add' else (a-b)) & 0xffffffff
                require(int(row['value'], 16) == expected, 'integer arithmetic')
            if op['op'] in ('fadd', 'fmul', 'fdiv'):
                require(slot == 0 and component == 0 and count == 1, 'scalar FP destination')
                a, b = fraction(operand(op['rs1'], state)), fraction(operand(op['rs2'], state))
                if op['op'] == 'fadd': expected = a + b
                elif op['op'] == 'fmul': expected = a * b
                else:
                    require(b != 0, 'unsupported zero divisor')
                    expected = a / b
                require(int(row['value'], 16) == rounded_bits(expected, mode), 'scalar FP arithmetic')
                scalar_counts[op['op']] = scalar_counts.get(op['op'], 0) + 1
            if op['op'] == 'ftrv':
                require(slot == 0 and count == 4, 'FTRV destination')
                if component == 0:
                    point = [state[k] for k in vector_keys(op['rs1'], 4)]
                    matrix = [state[k] for k in vector_keys(op['rs2'], 16)]
                    require(not pending, 'unfinished FTRV')
                    pending = {i: rounded_bits(sum(fraction(matrix[4*j+i])*fraction(point[j]) for j in range(4)), mode) for i in range(4)}
                    counts[op['pc']] = counts.get(op['pc'], 0) + 1
                require(component in pending and int(row['value'], 16) == pending.pop(component), 'FTRV arithmetic')
            value = int(row['value'], 16)
            for offset in range(count if count <= 2 else 1):
                key = keys[component + offset]
                require(key not in state, 'duplicate SSA result')
                state[key] = (value >> (32 * offset)) & 0xffffffff
    require(active is None and steps > 0 and counts, 'incomplete trace')
    return {'steps': steps, 'stores': stores, 'reads': reads, 'ftrv': counts, 'scalar_fp': scalar_counts, 'assumed_rounding_mode': mode,
            'fp_mode_observed': require_mode, 'source_lineage_verified': False, 'camera_recovered': False}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('ledger', type=Path)
    parser.add_argument('--mode', type=int, required=True)
    parser.add_argument('--require-mode', action='store_true')
    parser.add_argument('--include-calc', action='store_true')
    args = parser.parse_args()
    print(json.dumps(inspect(decode(args.ledger.read_bytes()), args.mode, args.require_mode, args.include_calc), sort_keys=True))
