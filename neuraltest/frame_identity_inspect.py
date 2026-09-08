"""Validate bounded producer/dequeue/capture trace, not cross-run determinism."""
import argparse
import json
from pathlib import Path
from transform_span_inspect import records
from transform_store_inspect import load_session, require


def inspect(session):
    submissions = records(session, 'FC067_SUBMIT')
    dequeues = records(session, 'FC067_DEQUEUE')
    attempts = records(session, 'FC067_CAPTURE_ATTEMPT')
    retained = [r for r in attempts if int(r['retained_after']) > int(r['retained_before'])]
    require(len(retained) == 3, 'expected three retained attempts')
    result = []
    for index, row in enumerate(retained):
        require(row['success'] == '1' and int(row['retained_before']) == index
                and int(row['retained_after']) == index+1
                and int(row['seen_after']) == int(row['seen_before'])+1, 'retention discontinuity')
        producer = row['producer_id']
        submit = [s for s in submissions if s['id'] == producer]
        dequeue = [d for d in dequeues if d['id'] == producer]
        require(len(submit) == len(dequeue) == 1, 'ambiguous or missing producer')
        s,d = submit[0],dequeue[0]
        require(s['accepted'] == '1' and s['rtt'] == d['rtt'] == '0'
                and s['address'] == d['address'], 'wrong submitted context')
        cycle = int(row['producer_cycle'])
        require(cycle > 0 and cycle == int(s['cycles']) == int(d['producer_cycles']), 'producer cycle mismatch')
        submit_pos = session.index('FC067_SUBMIT id='+producer+' ')
        dequeue_pos = session.index('FC067_DEQUEUE id='+producer+' ')
        capture_pos = session.index('FC067_CAPTURE_ATTEMPT id='+row['id']+' ')
        require(submit_pos < dequeue_pos < capture_pos, 'producer observation order')
        current = dict(frame_id=int(row['id']), producer_id=int(producer), producer_cycle=cycle,
                       seen_before=int(row['seen_before']), context=s['address'])
        if result:
            previous = result[-1]
            require(current['frame_id'] == previous['frame_id']+1
                    and current['producer_id'] == previous['producer_id']+1
                    and current['seen_before'] == previous['seen_before']+1
                    and current['producer_cycle'] > previous['producer_cycle'], 'nonconsecutive retained sequence')
        result.append(current)
    return result


def verify(session):
    result = inspect(session)
    first = result[0]
    mutations = [
        ('producer_cycle='+str(first['producer_cycle']), 'producer_cycle='+str(first['producer_cycle']+1)),
        ('producer_id='+str(first['producer_id']), 'producer_id='+str(first['producer_id']+1)),
        ('retained_before=0 retained_after=1', 'retained_before=0 retained_after=2'),
        ('FC067_SUBMIT id='+str(first['producer_id'])+' ', 'FC067_SUBMIT id=0 '),
    ]
    for old,new in mutations:
        require(old in session, 'negative target absent')
        try:
            inspect(session.replace(old,new,1))
        except ValueError:
            continue
        raise ValueError('accepted altered producer witness')
    return dict(frames=result, rejected_offline_controls=len(mutations),
                cross_run_determinism='unproven', original_offset_cause='unproven')


def validate_manifests(manifests, witnessed):
    """Compare new capture metadata to an independently retained producer trace."""
    require(len(manifests) == len(witnessed) == 3, 'bounded manifest count')
    epoch = None
    for manifest, reference in zip(manifests, witnessed):
        identity = manifest.get('producer_identity')
        require(isinstance(identity, dict) and identity.get('available') is True,
                'producer identity unavailable')
        require(identity.get('clock') == 'sh4-scheduler-cycles', 'wrong producer clock')
        require(all(type(identity.get(k)) is int and 0 <= identity[k] < 2**64
                    for k in ('epoch','ordinal','cycle')), 'invalid producer integer')
        require(identity['epoch'] > 0 and identity['ordinal'] > 0, 'missing producer epoch/ordinal')
        if epoch is None:
            epoch = identity['epoch']
        require(identity['epoch'] == epoch, 'capture crosses producer reset')
        require(manifest['frame_id'] == reference['frame_id']
                and identity['ordinal'] == reference['producer_id']
                and identity['cycle'] == reference['producer_cycle'], 'manifest versus producer witness differs')
    # Epoch is process-local and was not in the old trace. Equality within this
    # capture is checked; cross-process epoch provenance is not invented.
    return dict(matched_frames=3, epoch=epoch, cross_process_epoch='not-witnessed')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, required=True)
    parser.add_argument('--manifests', type=Path)
    args = parser.parse_args()
    result = verify(load_session(args.capture))
    if args.manifests:
        paths = sorted(args.manifests.glob('frame-*/manifest.json'))
        require(len(paths) == 3 and all(p.stat().st_size <= 1024*1024 for p in paths), 'manifest bounds')
        result['manifest_match'] = validate_manifests([json.loads(p.read_text()) for p in paths], result['frames'])
    print(json.dumps(result, indent=2))
