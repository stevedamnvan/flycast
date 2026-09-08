"""Check bounded executed SQ last writers, not the position calculation or camera."""
import argparse
import json
from pathlib import Path
import re

from reverse_copy_inspect import inspect as inspect_copy
from transform_span_inspect import records
from transform_store_inspect import load_session, require


def apply_store(known, writers, row, event):
    address, offset, size = int(row['address'], 16), int(row['offset']), int(row['size'])
    require(address >> 26 == 0x38 and address & 32 and offset == address & 31,
            'not physical SQ slot1')
    require(size in (4, 8) and offset + size <= 32, 'unsupported SQ store width')
    value, expected = int(row['actual'], 16), int(row['expected'], 16)
    require(row['exact'] == '1' and value == expected and 0 <= value < 1 << (size*8),
            'post-store value mismatch')
    require(int(row['pc'], 16) != 0 and int(row['pc'], 16) % 2 == 0, 'invalid store PC')
    for i, byte in enumerate(value.to_bytes(size, 'little')):
        known[offset+i], writers[offset+i] = byte, event


def inspect(session, scene, manifest):
    result = inspect_copy(session, scene, manifest)
    trace = []
    for line in session.splitlines():
        match = re.search(r'FC067_SQ_(\w+) (.*)', line)
        if match:
            trace.append((match[1], dict(re.findall(r'(\w+)=([^ ]+)', match[2]))))
    require(trace and trace[0][0] == 'BEGIN' and trace[-1][0] == 'STOP', 'missing SQ boundaries')
    begin, stop = trace[0][1], trace[-1][1]
    require(begin['boundary'] == 'first-observed-store' and begin['generation'] == '1'
            and stop['reason'] == 'target-covered', 'incomplete SQ observation')
    require(session.index('FC067_SQ_STOP ') < session.index('FC067_REVERSE_COPY '), 'copy precedes writer completion')
    last_cycle = int(begin['cycle'])
    require(7602640000 <= last_cycle <= 7602643776, 'outside bounded writer window')
    known, writers = [None]*32, [None]*32
    events, generation, finished = 0, 1, False
    store_rows, word_rows = {}, []
    for tag, row in trace[1:-1]:
        if tag == 'WORD':
            require(finished, 'word summary before target flush')
            word_rows.append(row)
            continue
        require(not finished and tag in ('STORE', 'FLUSH'), 'unexpected SQ event')
        events += 1
        cycle = int(row['cycle'])
        require(events <= 256 and int(row['event']) == events and int(row['generation']) == generation,
                'event or generation disagreement')
        require(last_cycle <= cycle <= 7602643776, 'clock order disagreement')
        last_cycle = cycle
        if tag == 'STORE':
            apply_store(known, writers, row, events)
            store_rows[events] = row
        else:
            coverage = sum(1 << i for i, byte in enumerate(known) if byte is not None)
            require(int(row['coverage'], 16) == coverage, 'coverage disagreement')
            require(row['exact'] == ('1' if coverage == 0xffffffff else '0'), 'flush consistency failure')
            if row['target'] == '1':
                require(cycle == 7602643776 and row['address'] == 'e0000020' and coverage == 0xffffffff,
                        'wrong or uncovered target flush')
                finished = True
            else:
                require(row['target'] == '0', 'invalid target marker')
                generation += 1  # Flushing never clears physical SQ contents.
    require(finished and len(word_rows) == 8 and int(stop['events']) == events, 'incomplete final SQ coverage')
    copy_words = records(session, 'FC067_REVERSE_COPY_WORD')
    expected_pcs = [0x8c03cc40, 0x8c03cc82, 0x8c03cc84, 0x8c03cc86,
                    0x8c03cc8c, 0x8c03cc96, 0x8c03cc8e, 0x8c03cc90]
    for i, row in enumerate(word_rows):
        require(int(row['index']) == i, 'word order disagreement')
        byte_range = slice(4*i, 4*i+4)
        require([int(v) for v in row['writers'].split(',')] == writers[byte_range], 'false last writer')
        value = int.from_bytes(bytes(known[byte_range]), 'little')
        require(int(row['value'], 16) == int(copy_words[i]['before'], 16) == value, 'writer/copy bytes disagree')
        require(all(int(store_rows[event]['pc'], 16) == expected_pcs[i] for event in writers[byte_range]),
                'different target filling-store PC')
    result.update(sq_filling_store_provenance=True, known_bytes=32, sq_events=events,
                  store_count=len(store_rows), position_store_pcs=['8c03cc82','8c03cc84','8c03cc86'],
                  position_calculation_provenance=False, camera_recovered=False)
    return result


def check_controls(session, scene, manifest):
    first = records(session, 'FC067_SQ_STORE')[0]
    changes = [
        ('STORE event=1 generation=1', 'STORE event=1 generation=2'),
        ('pc=8c03cc82', 'pc=8c03cc80'),
        ('coverage=ffffffff', 'coverage=fffffffe'),
        ('writers=1,1,1,1', 'writers=2,1,1,1'),
        ('expected='+first['expected'], 'expected='+format(int(first['expected'],16)^1,'016x'))]
    for old, new in changes:
        mutated = session.replace(old, new, 1)
        require(mutated != session, 'control did not mutate')
        try:
            inspect(mutated, scene, manifest)
        except ValueError:
            continue
        raise ValueError('SQ control accepted: '+new)
    return len(changes)


def inspect_capture(capture):
    frame = capture / 'frame-001782'
    def read(name):
        path = frame / name
        require(path.stat().st_size <= 8*1024*1024, 'JSON exceeds bound')
        return json.loads(path.read_text(encoding='utf-8'))
    session, scene, manifest = load_session(capture), read('pvr-scene.json'), read('manifest.json')
    result = inspect(session, scene, manifest)
    result['offline_controls_rejected'] = check_controls(session, scene, manifest)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', required=True, type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(inspect_capture(args.capture), sort_keys=True))
    except (ValueError, OSError, KeyError, IndexError, TypeError) as error:
        parser.exit(1, f'SQ writer evidence rejected: {error}\n')
