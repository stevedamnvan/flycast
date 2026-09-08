"""Check the bounded observed scalar routine; never infer a PVR camera."""
import argparse
import json
from pathlib import Path
from transform_span_inspect import records
from transform_store_inspect import load_session, require


def inspect(session):
    blocks = records(session, 'FC067_ROUTINE_BLOCK')
    stops = records(session, 'FC067_ROUTINE_STOP')
    require(len(blocks) == 4 and len(stops) == 1, 'routine observation cardinality')
    require('FC067_ROUTINE_METADATA' not in session, 'missing executed descriptor')
    require([b['block'] for b in blocks] == ['8c048aa0','8c048ad2','8c048afe','8c048b1a'],
            'different observed path')
    require([b['step'] for b in blocks] == ['1','2','3','4'], 'step order')
    for i, block in enumerate(blocks):
        require(block['return'] == '8c048e70', 'return identity')
        require(block['next'] == (blocks[i+1]['block'] if i < 3 else block['return']), 'broken block edge')
    require(stops[0] == dict(reason='returned',steps='4') and blocks[-1]['r0'] == '00000002',
            'not observed return')
    reads = records(session, 'FC067_OPAQUE_READ')
    for pc, word in [('8c048aa4','00000000bda5382f'),('8c048aaa','000000003d95af05')]:
        selected = [r for r in reads if r['pc'] == pc]
        require(len(selected) == 1 and selected[0]['value'] == selected[0]['expected'] == word
                and selected[0]['exact'] == '1', 'copy read mismatch')
    ops = records(session, 'FC067_ROUTINE_SHIL')
    require([sum(r['step'] == str(i) for r in ops) for i in range(1,5)] == [12,15,10,9],
            'executed descriptor completeness')
    # These are descriptor membership checks, not post-store RAM proof.
    for pc in ('8c048b28','8c048b2c'):
        require(len([r for r in ops if r['pc'] == pc and r['step'] == '4' and r['op'] == 'writem']) == 1,
                'missing scalar-store descriptor')
    return dict(blocks=4, returned=True, return_r0=2, copy_reads=2,
                scalar_store_bytes_verified=False, ta_lineage='uncorrelated', camera_recovered=False)


def verify(session):
    result = inspect(session)
    controls = [('next=8c048ad2','next=8c048afe'), ('return=8c048e70','return=8c048e72'),
                ('reason=returned steps=4','reason=block-cap steps=4'),
                ('pc=8c048aaa value=000000003d95af05','pc=8c048aaa value=000000003d95af04')]
    for old,new in controls:
        require(old in session, 'control target absent')
        try: inspect(session.replace(old,new,1))
        except ValueError: continue
        raise ValueError('accepted altered routine evidence')
    result['rejected_offline_controls'] = len(controls)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(verify(load_session(args.capture)), indent=2))
