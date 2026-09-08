"""Byte-level last-writer check in the explicitly instrumented target domains."""
import argparse
import json
import re
from pathlib import Path
from producer_record_inspect import inspect as inspect_records, SITES
from transform_ledger_inspect import decode
from transform_store_inspect import require


def verify_memory(rows):
    memory = {}
    reads = writes = covered = unknown = 0
    previous_cycle = 0
    for event, row in enumerate(rows, 1):
        require(int(row['event']) == event and event <= 131072, 'memory sequence/bound')
        cycle = int(row['cycle'])
        require(cycle >= previous_cycle, 'memory chronology')
        previous_cycle = cycle
        address, size, value = int(row['address'], 16), int(row['size']), int(row['value'], 16)
        require(size in (1, 2, 4, 8) and address < 0xe0000000 and ((address >> 26) & 7) == 3, 'memory shape')
        base = address & 0x1ffffff
        require(base+size <= 0x1000000, 'RAM bound')
        if row['kind'] == 'write':
            writes += 1
            for i in range(size): memory[base+i] = ((value >> (8*i)) & 255, row['pc'], event)
        else:
            require(row['kind'] == 'read' and size == 4, 'read shape')
            reads += 1
            writers = [memory.get(base+i) for i in range(size)]
            if any(w is None for w in writers):
                unknown += 1
                continue
            require(all(w[0] == ((value >> (8*i)) & 255) for i, w in enumerate(writers)), 'last writer/read bytes')
            if len({w[2] for w in writers}) == 1 and writers[0][1] in SITES and SITES[writers[0][1]][0] != 'calc':
                covered += 1
    require(reads > 0 and writes > 0, 'empty memory evidence')
    return dict(memory_reads=reads, memory_writes=writes, reads_without_observed_writer=unknown,
                new_family_last_writer_reads=covered,
                complete_transform_to_consumer_lineage=False)


def inspect(text, include_calc=False):
    result, records = inspect_records(text, include_calc)
    rows = [dict(re.findall(r'(\w+)=([^ ]+)', line)) for line in text.splitlines()
            if 'FC067_PRODUCER_MEMORY ' in line]
    result.update(verify_memory(rows))
    result.update(verify_copies(text))
    result.update(bind_records(text, records))
    return result


def bind_records(text, records, include_identities=False):
    completed = {(r['step'], r['base']): r for r in records}
    memory, owners, reads, bound = {}, {}, {}, []
    step = None
    for line in text.splitlines():
        match = re.search(r'FC067_PRODUCER_(ENTRY|EXIT|MEMORY|STORE|COPY) (.*)', line)
        if not match: continue
        kind = match[1]
        row = dict(re.findall(r'(\w+)=([^ ]+)', match[2]))
        if kind == 'ENTRY': step = int(row['step'])
        elif kind == 'EXIT': step = None
        elif kind == 'MEMORY':
            address, size = int(row['address'], 16), int(row['size'])
            if row['kind'] == 'write':
                for i in range(size):
                    memory[address+i] = (step, row['pc'], (int(row['value'], 16) >> (8*i)) & 255)
                    owners.pop(address+i, None)
            else:
                found = [owners.get(address+i) for i in range(size)]
                reads[address] = found[0] if found[0] is not None and all(v == found[0] for v in found) else None
        elif kind == 'STORE':
            key = int(row['step']), int(row['address'], 16)
            if key not in completed: continue
            record = completed[key]
            # Only the final X store can complete this exact step/base record.
            require(int(row['value'], 16) == record['xyz'][0], 'record final value')
            expected_sites = {component: pc for pc, (family, component) in SITES.items() if family == record['family']}
            present = [memory.get(key[1]+i) for i in range(12)]
            if all(v is None for v in present): continue  # Outside observed target domain.
            for i, value in enumerate(present):
                require(value == (key[0], expected_sites[i//4], (record['xyz'][i//4] >> (8*(i%4))) & 255), 'record writer identity')
                owners[key[1]+i] = key
        elif kind == 'COPY':
            base = int(row['base'], 16)
            found = [reads.pop(base+4*i, None) for i in range(3)]
            if found[0] is not None and all(v == found[0] for v in found):
                require(completed[found[0]]['xyz'] == [int(row[k],16) for k in ('x','y','z')], 'owned copy bytes')
                bound.append((int(row['id']), found[0]))
    result = dict(copies_owned_by_complete_record=len(bound),
                consumed_record_executions=len({key for identity,key in bound}),
                matrix_to_record_continuity_verified=False)
    if include_identities: result['copy_record_ids'] = bound
    return result


def verify_copies(text,ordinal=1781):
    require(ordinal in (1781,1782,1783),'producer ordinal bounds')
    reads = {}
    known_addresses = set()
    last_id = last_offset = -1
    copies = tracked = outside = 0
    for line in text.splitlines():
        match = re.search(r'FC067_PRODUCER_(MEMORY|COPY) (.*)', line)
        if not match: continue
        row = dict(re.findall(r'(\w+)=([^ ]+)', match[2]))
        if match[1] == 'MEMORY':
            if row['kind'] == 'read':
                known_addresses.add(int(row['address'], 16))
                reads[int(row['address'], 16)] = (int(row['value'], 16), int(row['cycle']))
            continue
        identity, offset = int(row['id']), int(row['offset'])
        require(identity > last_id and offset > last_offset and offset % 32 == 0, 'copy identity/order')
        require(int(row['ordinal']) == ordinal, 'copy ordinal')
        last_id, last_offset = identity, offset
        copies += 1
        base, cycle = int(row['base'], 16), int(row['cycle'])
        source = [reads.get(base+4*i) for i in range(3)]
        if all(r is None for r in source):
            require(not any(base+4*i in known_addresses for i in range(3)), 'copy reused consumed reads')
            outside += 1
            continue
        require(all(r is not None for r in source), 'partial copy read coverage')
        require(all(r[0] == int(row[k], 16) and r[1] <= cycle for r, k in zip(source, ('x','y','z'))), 'copy versus source read')
        for i in range(3): del reads[base+4*i]
        tracked += 1
    require(copies > 0, 'missing copy identities')
    return dict(observed_copies=copies, copies_with_tracked_reads=tracked,
                copies_outside_tracked_reads=outside, copy_to_ta_tape_verified=False)


def bind_tape(text, records,ordinal=1781):
    require(ordinal in (1781,1782,1783),'producer ordinal bounds')
    copies = {}
    for line in text.splitlines():
        if 'FC067_PRODUCER_COPY ' not in line: continue
        row = dict(re.findall(r'(\w+)=([^ ]+)', line))
        key = tuple(int(row[k]) for k in ('ordinal', 'generation', 'offset'))
        require(key not in copies, 'duplicate copy key')
        copies[key] = row
    matched = other = 0
    used = set()
    for record in records:
        if record['ordinal'] != ordinal: continue
        key = record['ordinal'], record['generation'], record['ta_offset']
        if key not in copies:
            other += 1
            continue
        row = copies[key]
        require(key not in used, 'duplicate tape association')
        used.add(key)
        require(record['ram_x'] == int(row['base'], 16) and
                record['after'][1:4] == [int(row[k], 16) for k in ('x', 'y', 'z')]
                and record['cycle'] >= int(row['cycle']), 'copy/tape source bytes or chronology')
        matched += 1
    require(matched > 0, 'no copy/tape matches')
    return dict(selected_copy_tape_matches=matched, selected_other_copy_paths=other,
                logged_copies_not_selected=len(copies)-matched,
                join_key='ordinal-generation-TA-offset', tape_contains_copy_id=False)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('ledger', type=Path)
    parser.add_argument('--include-calc', action='store_true')
    args = parser.parse_args()
    print(json.dumps(inspect(decode(args.ledger.read_bytes()), args.include_calc), sort_keys=True))
