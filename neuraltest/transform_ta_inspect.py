"""Verify one captured transform-to-TA-input chain, not camera or presentation."""
import argparse
import json
from pathlib import Path
import re

from transform_derived_inspect import inspect_derived
from transform_span_inspect import records
from transform_store_inspect import compare_native, load_session, require


def inspect_ta(path, negative=False):
    derived = inspect_derived(path, False)
    session = load_session(path)
    base = derived['source'][0]
    xyz = [derived['actual'][2], derived['actual'][3], derived['actual'][1]]
    begins = records(session, 'FC067_G2_BEGIN')
    require(begins == [dict(base=f'{base:08x}', bytes='12', generation='2', coverage='shil-cpu-only')],
            'invalid derived generation')
    access = records(session, 'FC067_G2_ACCESS')
    reads = records(session, 'FC067_G2_READ')
    prefix = re.findall(r'FC067_G2_(BEGIN|ACCESS|READ|STORE|FLUSH) ', session)[:14]
    require(prefix == ['BEGIN', 'ACCESS', 'READ'] + ['ACCESS', 'READ', 'STORE']*3
            + ['STORE', 'FLUSH'], 'wrong read/store execution order')
    require(len(access) == 5 and len(reads) == 4, 'wrong bounded access count')
    for a, r, pc, index, event in zip(access, reads,
                                     (0x8c070cb4, 0x8c070ea0, 0x8c070ea4, 0x8c070ea8),
                                     (2, 0, 1, 2), (33, 58, 60, 62)):
        require(int(a['pc'], 16) == int(r['pc'], 16) == pc, 'wrong consumer instruction')
        require(int(a['address'], 16) == base + index*4 and int(a['event']) == event,
                'wrong consumer address/event')
        require(a['size'] == '4' and a['write'] == '0', 'wrong consumer kind')
        require(int(a['bytes'], 16) == int(r['value'], 16) == int(r['expected'], 16) == xyz[index]
                and r['exact'] == '1', 'wrong actual loaded value')
    last = access[-1]
    require(last['event'] == '381' and last['pc'] == '8c070c40'
            and int(last['address'], 16) == base + 8 and last['size'] == '4' and last['write'] == '1'
            and int(last['bytes'], 16) == xyz[2], 'wrong terminating write')
    stores = records(session, 'FC067_G2_STORE')
    require(len(stores) == 9, 'unexpected bounded store observations')
    queue = int(stores[0]['address'], 16) - 4
    require(queue & 31 == 0 and queue & 0xfc000000 == 0xe0000000, 'not a store queue')
    for i, (store, register) in enumerate(zip(stores, ('r2.1', 'r3.1', 'r2.2'))):
        readpc, storepc = 0x8c070ea0 + 4*i, 0x8c070ea2 + 4*i
        require(int(store['pc'], 16) == storepc and int(store['address'], 16) == queue + 4*(i+1),
                'wrong coordinate store destination')
        require(int(store['value'], 16) == int(store['stored'], 16) == xyz[i], 'store mismatch')
        require(store['sq'] == store['exact'] == '1' and store['ram'] == '0', 'wrong store route')
        read_suffix = '' if i == 0 else f', {4*i}'
        require(f'FC067_SPAN_SHIL pc={readpc:08x} op=readm {register} <- r4.0{read_suffix}' in session,
                'missing compiled load dependency')
        require(f'FC067_SPAN_SHIL pc={storepc:08x} op=writem  <- r14.0, {register}, {4*(i+1)}' in session,
                'missing compiled store dependency')
    require('FC067_SPAN_SHIL pc=8c070eb0 op=pref  <- r14.0' in session, 'missing flush instruction')
    flushes = records(session, 'FC067_G2_FLUSH')
    require(len(flushes) == 2, 'unexpected flush count')
    flush = flushes[0]
    packet = [int(v, 16) for v in flush['words'].split(',')]
    destination = int(flush['destination'], 16)
    require(len(packet) == 8 and packet[1:4] == xyz, 'packet coordinates differ')
    require(int(flush['address'], 16) == queue and flush['area'] == '3' and flush['ram_exact'] == '1',
            'flush is not exact RAM route')
    require(destination == 0x0c000000 | (queue & 0x00ffffe0), 'wrong Dreamcast RAM destination')
    g3 = records(session, 'FC067_G3_BEGIN')
    require(g3 == [dict(base=f'{destination:08x}', bytes='32', source='sq-flush',
                        coverage='shil-sq-ta-entry')], 'wrong flushed generation')
    require(not records(session, 'FC067_G3_STOP'), 'flushed generation invalidated or timed out')
    bulk = records(session, 'FC067_G3_TA_BULK')
    require(len(bulk) == 1, 'expected one bulk TA input')
    bulk = bulk[0]
    offset, count = int(bulk['packet_offset']), int(bulk['count'])
    require(offset % 32 == 0 and 0 <= offset and offset + 32 <= count*32, 'packet outside bulk input')
    require(int(bulk['expected_offset']) == offset + (32 if negative else 0)
            and bulk['association_exact'] == ('0' if negative else '1'), 'wrong association control')
    require(int(bulk['base'], 16) == destination and bulk['words'] == flush['words']
            and bulk['exact'] == bulk['poly'] == '1' and int(bulk['address'], 16) & 0x800000 == 0,
            'bulk input bytes/route differ')
    require(0 < int(bulk['events']) <= 5000000
            and 0 <= int(bulk['cycles']) - int(bulk['start']) <= 200000000, 'outside observation bounds')
    order = re.findall(r'FC067_(G2_BEGIN|G2_FLUSH|G3_BEGIN|G3_TA_BULK) ', session)
    require(order == ['G2_BEGIN', 'G2_FLUSH', 'G3_BEGIN', 'G2_FLUSH', 'G3_TA_BULK'], 'wrong lineage order')
    require(records(session, 'FC067_G2_STOP') == [dict(reason='overwrite', events='381',
                                                     pc='8c070c40', ta_lineage='unknown')],
            'old derived generation did not terminate at overwrite')
    return dict(derived=derived, queue=queue, packet=packet, destination=destination,
                offset=offset, count=count, cycles=bulk['cycles'], events=bulk['events'])


def verify(positive, negative, baseline):
    pos, neg = inspect_ta(positive), inspect_ta(negative, True)
    require(pos == neg, 'negative changed actual chain')
    return dict(scope='one-transform-to-bulk-TA-input', packet_offset=pos['offset'],
                bulk_packets=pos['count'], wrong_offset_rejected=True,
                native_plane_comparisons=compare_native(positive, negative, baseline),
                camera='unproven', rendered_frame_ownership='unproven')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('positive', 'negative', 'baseline'):
        parser.add_argument('--'+name, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(verify(args.positive, args.negative, args.baseline), indent=2))
