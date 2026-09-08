"""Dispatch/register continuity; source coordinate meaning remains unknown."""
import argparse
import json
import re
from pathlib import Path
from producer_record_inspect import inspect as inspect_records
from transform_ledger_inspect import decode
from transform_store_inspect import require


def registers(token):
    if not token.startswith('r'): return []
    base, count, versions = token[1:].split(':')
    return list(range(int(base), int(base)+int(count)))


def inspect(text, include_identities=False):
    result, records = inspect_records(text)
    ops = {}
    for line in text.splitlines():
        if 'FC067_PRODUCER_OP ' in line:
            r = dict(re.findall(r'(\w+)=([^ ]+)', line))
            ops[int(r['descriptor']), int(r['index'])] = r
    values, origins, stores = {}, {}, {}
    dispatch = None
    gaps = checks = 0
    operation_origins = {}
    transform_inputs = {}
    projection_inputs, store_constants = {}, {}
    for line in text.splitlines():
        m = re.search(r'FC067_PRODUCER_(ENTRY|INPUT|VALUE|STORE) (.*)', line)
        if not m: continue
        kind, r = m[1], dict(re.findall(r'(\w+)=([^ ]+)', m[2]))
        if kind == 'ENTRY':
            require('dispatch' in r and int(r['dispatch']) > 0, 'missing dispatch')
            current = int(r['dispatch'])
            require(dispatch is None or current > dispatch, 'dispatch order')
            if dispatch is None or current != dispatch+1:
                values, origins = {}, {}
                gaps += 1
            dispatch, descriptor, step = current, int(r['descriptor']), int(r['step'])
            operation_origins = {}
        elif kind == 'INPUT':
            reg, value = int(r['reg']), int(r['value'],16)
            if reg in values:
                require(values[reg] == value, 'cross-block register mismatch')
                checks += 1
            values[reg] = value
            origins.setdefault(reg, frozenset())
        else:
            index = int(r['index'])
            op = ops[descriptor,index]
            if kind == 'STORE':
                source = registers(op['rs2'])
                stores[step,int(r['address'],16)] = frozenset().union(*(origins.get(reg,frozenset()) for reg in source))
                if include_identities:
                    store_constants[step,int(r['address'],16)] = {reg:values[reg] for reg in (20,24,25) if reg in values}
                continue
            if index not in operation_origins:
                if op['op']=='fdiv' and include_identities:
                    denominator = registers(op['rs2'])
                    for identity in origins.get(denominator[0],frozenset()):
                        projection_inputs[identity] = values[registers(op['rs1'])[0]]
                if op['op']=='ftrv' and include_identities:
                    transform_inputs[step,op['pc']] = dict(
                        point_words=[values[reg] for reg in registers(op['rs1'])],
                        matrix_words=[values[reg] for reg in registers(op['rs2'])])
                source = [reg for field in ('rs1','rs2','rs3') for reg in registers(op[field])]
                operation_origins[index] = (frozenset({(step,op['pc'])}) if op['op']=='ftrv' else
                    frozenset() if op['op']=='readm' else frozenset().union(*(origins.get(reg,frozenset()) for reg in source)))
            slot, part = int(r['part']) >> 16, int(r['part']) & 65535
            dest = registers(op['rd'] if slot==0 else op['rd2'])
            value = int(r['value'],16)
            for offset in range(len(dest) if len(dest)<=2 else 1):
                reg = dest[part+offset]
                values[reg] = (value >> (32*offset)) & 0xffffffff
                origins[reg] = operation_origins[index]
    linked = unlinked = 0
    linked_ids, unlinked_ids = [], []
    record_transforms = []
    for record in records:
        tags = [stores[record['step'],record['base']+4*i] for i in range(3)]
        if len(tags[0])==1 and tags[0]==tags[1]==tags[2]:
            expected = '8c03a9ea' if record['family']=='divided' else '8c03a9b0'
            require(next(iter(tags[0]))[1] == expected, 'wrong matrix family')
            linked += 1
            linked_ids.append((record['step'], record['base']))
            if include_identities:
                transform = next(iter(tags[0]))
                record_transforms.append(dict(record_id=(record['step'],record['base']),
                                              transform_id=transform, **transform_inputs[transform],
                                              numerator_word=projection_inputs.get(transform),
                                              output_constants=store_constants[record['step'],record['base']],
                                              xyz_words=record['xyz']))
        else:
            unlinked += 1
            unlinked_ids.append((record['step'], record['base']))
    result.update(register_continuity_checks=checks, dispatch_segments=gaps,
                  records_with_matrix_continuity=linked, records_without_matrix_continuity=unlinked,
                  source_coordinate_meaning_verified=False)
    if include_identities:
        result.update(linked_record_ids=linked_ids, unlinked_record_ids=unlinked_ids)
        result['record_transforms'] = record_transforms
    return result


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('ledger',type=Path)
    args=parser.parse_args()
    print(json.dumps(inspect(decode(args.ledger.read_bytes())),sort_keys=True))
