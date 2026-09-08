"""Assemble observed producer XYZ stores; no unobserved-writer ownership claim."""
import argparse
import json
import re
from pathlib import Path

from producer_matrix_inspect import inspect as inspect_arithmetic
from transform_ledger_inspect import decode
from transform_store_inspect import require

SITES = {
    '8c03aa10': ('divided', 2), '8c03aa12': ('divided', 1), '8c03aa14': ('divided', 0),
    '8c03a9be': ('direct', 2), '8c03a9c0': ('direct', 1), '8c03a9c2': ('direct', 0),
    '8c03c9ca': ('calc', 2), '8c03c9cc': ('calc', 1), '8c03c9ce': ('calc', 0),
}


def assemble(stores):
    records = []
    pending = None
    for row in stores:
        pc = row['pc']
        if pc not in SITES:
            require(pending is None, 'interrupted XYZ triplet')
            continue
        family, component = SITES[pc]
        require(row['size'] == 4 and row['address'] % 4 == 0, 'XYZ store shape')
        if component == 2:
            require(pending is None, 'unfinished XYZ triplet')
            pending = dict(family=family, step=row['step'], base=row['address']-8,
                           xyz=[None, None, row['value']], next=1)
        else:
            require(pending is not None and pending['family'] == family
                    and pending['step'] == row['step'] and pending['next'] == component
                    and row['address'] == pending['base']+4*component, 'XYZ order/address/identity')
            pending['xyz'][component] = row['value']
            pending['next'] -= 1
            if component == 0:
                del pending['next']
                records.append(pending)
                pending = None
    require(pending is None, 'incomplete XYZ triplet')
    return records


def inspect(text, include_calc=False):
    result = inspect_arithmetic(text, 3, True, include_calc)
    descriptors, stores = {}, []
    for line in text.splitlines():
        match = re.search(r'FC067_PRODUCER_(OP|ENTRY|STORE) (.*)', line)
        if not match: continue
        kind = match[1]
        row = dict(re.findall(r'(\w+)=([^ ]+)', match[2]))
        if kind == 'OP': descriptors[int(row['descriptor']), int(row['index'])] = row['pc']
        elif kind == 'ENTRY': descriptor = int(row['descriptor'])
        else:
            stores.append(dict(pc=descriptors[descriptor, int(row['index'])],
                               step=int(row['step']), address=int(row['address'], 16),
                               size=int(row['size']), value=int(row['value'], 16)))
    records = assemble(stores)
    result['xyz_records'] = {family: sum(r['family'] == family for r in records) for family in ('direct', 'divided', 'calc')}
    result['unique_record_addresses'] = len({r['base'] for r in records})
    result['consumer_ownership_verified'] = False
    return result, records


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('ledger', type=Path)
    args = parser.parse_args()
    print(json.dumps(inspect(decode(args.ledger.read_bytes()))[0], sort_keys=True))
