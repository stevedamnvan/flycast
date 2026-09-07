"""Check one bounded RAM-read/copy/overwrite observation, not TA or camera recovery."""
import argparse
import json
from pathlib import Path
import re

from transform_store_inspect import compare_native, inspect_capture, load_session, require


def records(session, tag):
    return [dict(re.findall(r'(\w+)=([^ ]+)', line)) for line in session.splitlines()
            if tag + ' ' in line]


def inspect_span(path, negative=False):
    source = inspect_capture(path, False)
    base, output = source[:2]
    session = load_session(path)
    stream = re.findall(r'FC067_SPAN_(BEGIN|ACCESS|READ_RESULT|CONSUMER_STORE|STOP) ', session)
    require(stream == ['BEGIN'] + ['ACCESS', 'READ_RESULT', 'CONSUMER_STORE'] * 3
            + ['ACCESS', 'READ_RESULT', 'ACCESS', 'STOP', 'CONSUMER_STORE'],
            'wrong cross-event execution order')
    begins = records(session, 'FC067_SPAN_BEGIN')
    require(len(begins) == 1, 'expected one span generation')
    begin = begins[0]
    require(int(begin['base'], 16) == base and begin['bytes'] == '16'
            and begin['sequence'] == '1' and begin['coverage'] == 'shil-cpu-access-only',
            'invalid span source/scope')
    accesses = records(session, 'FC067_SPAN_ACCESS')
    require([int(r['event']) for r in accesses] == [1, 3, 5, 7, 10], 'wrong access sequence')
    reads = records(session, 'FC067_SPAN_READ_RESULT')
    require(len(reads) == 4, 'expected four loaded-value observations')
    for ordinal, (pc, index) in enumerate(zip((0x8c070c4a, 0x8c070c52, 0x8c070c5a, 0x8c070c64),
                                            (0, 1, 2, 2))):
        access, read = accesses[ordinal], reads[ordinal]
        require(int(access['pc'], 16) == pc == int(read['pc'], 16), 'wrong read instruction')
        require(int(access['address'], 16) == base + 4 * index, 'wrong read address')
        require(access['size'] == '4' and access['write'] == '0'
                and access['phase'] == 'before-access', 'wrong read classification')
        require(int(access['bytes'], 16) == output[index] == int(read['value'], 16),
                'loaded value differs from witnessed source')
        mutated = negative and ordinal == 0
        require(int(read['expected'], 16) == (output[index] ^ int(mutated)),
                'wrong loaded-value control')
        require(read['exact'] == ('0' if mutated else '1'), 'wrong read checker disposition')
    stores = records(session, 'FC067_SPAN_CONSUMER_STORE')
    require(len(stores) == 4, 'expected three copies and one terminating store')
    destination = int(stores[0]['address'], 16)
    require(destination & 3 == 0 and destination & 0x1c000000 == 0x0c000000,
            'copied destination is not aligned RAM')
    for i, store in enumerate(stores[:3]):
        require(int(store['pc'], 16) == 0x8c070c4e + 8 * i, 'wrong copy instruction')
        require(int(store['address'], 16) == destination + 4 * i, 'wrong copy address')
        require(int(store['value'], 16) == int(store['stored'], 16) == output[i],
                'copy source or committed value differs')
        require(all(store[k] == '1' for k in ('ram', 'exact', 'copy', 'source_match')),
                'invalid copy disposition')
        # This bounded title-specific compiled block binds each loaded SSA value
        # directly to the following copy store; equal numeric values alone do not.
        register = f'f3.{i + 1}'
        read_pc, write_pc = 0x8c070c4a + 8 * i, 0x8c070c4e + 8 * i
        require(f'FC067_SPAN_SHIL pc={read_pc:08x} op=readm {register} <- r4.0' in session,
                'missing compiled read dependency')
        require(f'FC067_SPAN_SHIL pc={write_pc:08x} op=writem  <- r10.0, {register}, {32+4*i}' in session,
                'missing compiled copy dependency')
    overwrite = accesses[-1]
    require(overwrite['pc'] == '8c070c92' and overwrite['write'] == '1'
            and overwrite['size'] == '4' and overwrite['phase'] == 'before-access'
            and int(overwrite['address'], 16) == base + 8
            and int(overwrite['bytes'], 16) == output[2], 'wrong overwrite boundary')
    stops = records(session, 'FC067_SPAN_STOP')
    require(len(stops) == 1 and stops[0]['reason'] == 'overwriting-store'
            and stops[0]['pc'] == '8c070c92' and stops[0]['events'] == '10'
            and stops[0]['ta_lineage'] == 'unknown', 'missing or invalid terminal event')
    final = stores[-1]
    require(final['pc'] == '8c070c92' and int(final['address'], 16) == base + 8
            and final['ram'] == final['exact'] == '1' and final['copy'] == '0'
            and final['value'] == final['stored'], 'invalid terminal write readback')
    return source, destination, final['value']


def verify(positive, negative, baseline):
    require(inspect_span(positive) == inspect_span(negative, True),
            'positive/negative executed source or consumer differ')
    count = compare_native(positive, negative, baseline)
    return {'scope': 'one-cpu-buffer-consumer', 'loaded_values_exact': 4,
            'copies_exact': 3, 'termination': 'overwriting-store',
            'wrong_read_expectation_rejected': True, 'native_plane_comparisons': count,
            'ta_lineage': 'unknown', 'camera_recovered': False}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ('positive', 'negative', 'baseline'):
        parser.add_argument('--' + option, required=True, type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(verify(args.positive, args.negative, args.baseline), sort_keys=True))
    except (ValueError, OSError, KeyError) as error:
        parser.exit(1, f'transform-span evidence rejected: {error}\n')
