"""Bounded byte-writer replay for one RAM record; not world/camera reconstruction."""
import argparse
import json
from pathlib import Path
import re

from transform_store_inspect import load_session, require
from xyz_operand_inspect import inspect as inspect_xyz

START = 0x8ce74250
RAM_MASK = 0xffffff  # This witness is Dreamcast 16 MiB, not Naomi.
BEGIN = 7601901888
END = 7602643776
PATHS = {'cpu', 'dma', 'dma-handler', 'pointer-copy', 'sq-generic',
         'sq-fast', 'sq-nonvmem', 'hle-status', 'hle-drive'}
FIELDS = {
    'WRITE': 'event generation cycle path pc address size first count source_offset',
    'BYTE': 'event byte source expected actual previous_writer',
    'READ': 'generation cycle pc address value expected coverage writers',
    'HLE': 'cycle syscall command idle status drive',
    'STOP': 'reason events reads coverage',
}


def overlap(address, size):
    require(0 <= address < 0xe0000000 and (address >> 26) & 7 == 3,
            'not an untransformed RAM alias')
    start, target = address & RAM_MASK, START & RAM_MASK
    require(0 < size <= RAM_MASK+1 and start+size <= RAM_MASK+1,
            'unsupported wrapped span')
    first, end = max(start, target), min(start+size, target+12)
    require(first < end, 'write misses record')
    return first-target, end-first, first-start


def inspect_record(session):
    rows = []
    for line in session.splitlines():
        match = re.search(r'\bFC067_RAM_(\w+) (.*)', line)
        if match:
            pairs = re.findall(r'(\w+)=([^ ]+)', match[2])
            require(len(dict(pairs)) == len(pairs), 'duplicate field')
            rows.append((match[1], dict(pairs)))
    require(rows and rows[0][0] == 'BEGIN', 'missing lifecycle start')
    begin = rows[0][1]
    require(begin == {'cycle': str(BEGIN), 'context': '00509700', 'generation': '1',
                     'start': '8ce74250', 'bytes': '12', 'cap': '64',
                     'max_cycles': '400000000'}, 'wrong lifecycle contract')
    state, writers, events, reads, hle = [None]*12, [0]*12, 0, [], 0
    last_cycle, pending, stopped, pcs = BEGIN, [], False, []
    for kind, row in rows[1:]:
        require(not stopped, 'events after terminal record')
        require(kind in FIELDS and set(row) == set(FIELDS[kind].split()),
                'unsupported/rejected event or missing fields: '+kind)
        if 'cycle' in row:
            cycle = int(row['cycle'])
            require(last_cycle <= cycle <= BEGIN+400000000, 'cycle outside lease')
            last_cycle = cycle
        require(not pending or kind == 'BYTE', 'incomplete write bytes')
        if kind == 'WRITE':
            require(not reads and events < 64, 'write after consumption or cap')
            events += 1
            require(int(row['event']) == events and row['generation'] == '1',
                    'wrong event/generation')
            address, size = int(row['address'], 16), int(row['size'])
            first, count, offset = overlap(address, size)
            require((int(row['first']), int(row['count']), int(row['source_offset']))
                    == (first, count, offset), 'incorrect physical overlap')
            require(row['path'] in PATHS, 'unsupported writer path')
            require(row['path'] != 'cpu' or size in (1, 2, 4, 8), 'CPU width')
            pcs.append(row['pc'])
            pending = list(range(first, first+count))
        elif kind == 'BYTE':
            require(pending and int(row['event']) == events, 'orphan writer byte')
            byte = pending.pop(0)
            require(int(row['byte']) == byte, 'byte order')
            source, expected, actual = (int(row[key], 16) for key in ('source', 'expected', 'actual'))
            require(0 <= actual <= 255 and source == expected == actual, 'byte mismatch')
            require(int(row['previous_writer']) == writers[byte], 'stale last writer')
            state[byte], writers[byte] = actual, events
        elif kind == 'READ':
            index = len(reads)
            require(index < 3 and all(writers) and row['generation'] == '1', 'incomplete writer generation')
            require(int(row['pc'], 16) == 0x8c03cc7c+index*2
                    and int(row['address'], 16) == START+index*4
                    and int(row['cycle']) == END and row['coverage'] == 'fff', 'wrong consumer')
            base = index*4
            value = int.from_bytes(bytes(state[base:base+4]), 'little')
            require(value == int(row['value'], 16) == int(row['expected'], 16), 'consumer differs from last writers')
            require([int(x) for x in row['writers'].split(',')] == writers[base:base+4], 'wrong consumer writer IDs')
            reads.append(value)
        elif kind == 'HLE':
            hle += 1
            require(hle <= 64 and row['syscall'] == '0' and
                    (row['command'], row['idle'], row['status'], row['drive'])
                    in {('2', '1', '0', '0'), ('1', '0', '1', '0'), ('4', '0', '0', '1')},
                    'uncovered HLE command')
        elif kind == 'STOP':
            require(row == {'reason': 'target-covered', 'events': str(events), 'reads': '3',
                            'coverage': 'fff'} and len(reads) == 3, 'observer stopped without complete record')
            stopped = True
        else:
            raise ValueError('unsupported/rejected RAM event: '+kind)
    require(stopped and not pending, 'unterminated writer lease')
    return {'record_words': reads, 'last_writers': writers, 'write_events': events,
            'writer_pcs': pcs, 'hle_guard_events': hle, 'record_bytes': 12,
            'camera_recovered': False, 'position_value_calculation_proven': False}


def inspect(session, scene, manifest):
    result = inspect_xyz(session, scene, manifest)
    record = inspect_record(session)
    for index, value in enumerate(record['record_words']):
        # The live RAM check is emitted after the actual load and immediately before
        # its XYZ result. Require same-run dynamic association, not value matching alone.
        pattern = (rf'FC067_RAM_READ [^\n]*pc={0x8c03cc7c+2*index:08x}[^\n]*\n'
                   + (r'[^\n]*FC067_RAM_STOP [^\n]*\n' if index == 2 else '')
                   + rf'[^\n]*FC067_XYZ_VALUE [^\n]*index={13+index} [^\n]*value={value:016x}(?:\r?\n|$)')
        require(re.search(pattern, session) is not None, 'RAM read not linked to actual XYZ return')
    result.update(record)
    result['record_last_writer_replay_proven'] = True
    result['coverage_scope'] = 'observed-x64-stores-sq-bulk-and-guarded-hle-not-all-host-write-paths'
    return result


def check_controls(session):
    changes = [session.replace('generation=1 start=', 'generation=2 start=', 1),
               session.replace('previous_writer=0', 'previous_writer=64', 1),
               session.replace('coverage=fff', 'coverage=ffe', 1),
               session.replace('reason=target-covered', 'reason=event-cap', 1)]
    byte = re.search(r'(FC067_RAM_BYTE [^\n]*expected=)([0-9a-f]{2})', session)
    require(byte is not None, 'missing control byte')
    changes.append(session[:byte.start(2)]+f'{int(byte[2],16)^1:02x}'+session[byte.end(2):])
    for changed in changes:
        require(changed != session, 'control did not mutate')
        try:
            inspect_record(changed)
        except ValueError:
            continue
        raise ValueError('falsifying control accepted')
    return len(changes)


def inspect_capture(capture):
    frame = capture/'frame-001782'
    def read(name):
        path = frame/name
        require(path.stat().st_size <= 8*1024*1024, 'JSON exceeds bound')
        return json.loads(path.read_text(encoding='utf-8'))
    session = load_session(capture)
    result = inspect(session, read('pvr-scene.json'), read('manifest.json'))
    result['record_controls_rejected'] = check_controls(session)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', required=True, type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(inspect_capture(args.capture), sort_keys=True))
    except (ValueError, OSError, KeyError, IndexError, TypeError) as error:
        parser.exit(1, f'RAM writer evidence rejected: {error}\n')
