"""Verify one exact copied TA packet to decoded vertex, not world camera."""
import argparse
import json
from pathlib import Path
import re

from transform_ta_inspect import inspect_ta
from transform_span_inspect import records
from transform_store_inspect import compare_native, load_session, require


def inspect_context(path, negative=False):
    ta = inspect_ta(path, False)
    session = load_session(path)
    require(re.findall(r'FC067_PACKET_(COPY|DECODE|STOP) ', session) == ['COPY', 'DECODE'],
            'copy/decode missing, reordered or invalidated')
    copied = records(session, 'FC067_PACKET_COPY')[0]
    decoded = records(session, 'FC067_PACKET_DECODE')[0]
    context = int(copied['context'], 16)
    offset, vertex = int(copied['offset']), int(decoded['vertex'])
    require(0 <= context < 0x800000 and offset % 32 == 0 and 0 <= offset <= 0x800000 - 32,
            'invalid Dreamcast context/packet offset')
    require(copied['bytes'] == '32' and copied['exact'] == '1'
            and copied['generation'] == decoded['generation'] == '1', 'unverified copy generation')
    require(int(decoded['child'], 16) == int(decoded['root'], 16) == context,
            'actual parser context differs')
    require(int(decoded['expected_context'], 16) == (context ^ (32 if negative else 0))
            and decoded['context_exact'] == ('0' if negative else '1'), 'wrong context control')
    require(decoded['packet_exact'] == decoded['xyz_exact'] == '1', 'copied packet or decoded xyz changed')
    require([int(v, 16) for v in decoded['xyz'].split(',')] == ta['packet'][1:4], 'wrong decoded coordinates')
    require(0 <= vertex < 65536, 'decoded vertex outside bounded scene contract')
    return dict(ta=ta, context=context, packet_offset=offset, vertex=vertex)


def verify(positive, negative, baseline):
    pos, neg = inspect_context(positive), inspect_context(negative, True)
    require(pos == neg, 'negative changed actual context/vertex lineage')
    return dict(scope='one-source-transform-to-decoded-vertex', context=f"{pos['context']:08x}",
                packet_offset=pos['packet_offset'], vertex=pos['vertex'], wrong_context_rejected=True,
                native_plane_comparisons=compare_native(positive, negative, baseline),
                world_camera='unproven', presented_frame='unproven')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('positive', 'negative', 'baseline'):
        parser.add_argument('--'+name, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(verify(args.positive, args.negative, args.baseline), indent=2))
