#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Join captured frames to actual helper sessions; not external neural proof."""
import argparse
import json
import re
from pathlib import Path
from remake_temporal_compare import captures, require
from remake_material_compare import materials


def receipts(text):
    received = {}
    for seq, frame, producer, size, digest in re.findall(
            r'live_receive sequence=(\d+) frame=(\d+) producer=(\d+) bytes=(\d+) digest=(\d+)', text):
        key = int(seq)
        require(key not in received, 'duplicate helper sequence')
        received[key] = tuple(map(int, (frame, producer, size, digest)))
    returned = {(int(seq), int(frame)) for seq, frame in re.findall(
        r'live_return sequence=(\d+) frame=(\d+) published=1', text)}
    return received, returned


def verify(record, packet_identity, sessions):
    token = record.get('session_token')
    require(token in sessions, 'capture session missing from launcher')
    received, returned = sessions[token]
    seq, frame = record['sequence'], record['source_frame']
    require(seq in received, 'capture receipt absent from helper')
    actual = received[seq]
    require((frame, record['producer_ordinal'], record['source_digest']) ==
            (actual[0], actual[1], actual[3]), 'capture/helper identity mismatch')
    require((seq, frame) in returned, 'capture return unpublished')
    require(packet_identity[:3] == (frame, record['producer_epoch'], record['producer_ordinal']),
            'captured packet identity mismatch')
    return token


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--run', type=Path, required=True)
    p.add_argument('--host-log', type=Path, required=True)
    p.add_argument('--expected', type=int, required=True)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    require(2 <= a.expected <= 300, 'expected capture bound')
    launch = json.loads((a.run/'launch.json').read_text())
    sessions = {}
    for session in launch['sessions']:
        command = session['command']; token = command[command.index('--live-channel-async')+1]
        require(token not in sessions, 'duplicate token')
        sessions[token] = receipts((a.run/f"consumer-g{session['generation']}.log").read_text(errors='replace'))
    host = a.host_log.read_text(errors='replace')
    presents = {tuple(map(int, v)) for v in re.findall(
        r'Remake preview present: source=(\d+) current=(\d+) kind=remake-evaluated completed=1 hresult=0', host)}
    frames = captures(a.run/'captures')
    require(len(frames) == a.expected, 'incomplete capture count')
    groups = {}; timeline = []
    for frame, (directory, record) in sorted(frames.items()):
        token = verify(record, materials(directory/'remake-view.bin')[0], sessions)
        require((frame, record['current_frame']) in presents, 'missing completed Present')
        groups[token] = groups.get(token, 0)+1
        timeline.append(dict(frame=frame, token=token, sequence=record['sequence']))
    require(len(groups) >= 2, 'capture does not span handover')
    resets = {}
    sections = re.split(r'Remake fresh session requested: ([A-Za-z0-9-]+)', host)
    for i in range(1, len(sections), 2):
        token, section = sections[i:i+2]
        match = re.search(r'Remake async neural evaluation: source=\d+ current=\d+ sequence=\d+ accepted=1 reset=(\d+)', section)
        if token in groups:
            require(match and match[1] == '1', 'first accepted evaluation must reset history')
            resets[token] = True
    require(set(resets) == set(groups), 'missing session reset evidence')
    result = dict(frames=len(frames), groups=groups, first_accepted_resets=resets,
                  gaps=[(x,y) for x,y in zip(sorted(frames),sorted(frames)[1:]) if y != x+1],
                  timeline=timeline, scope='receipt, packet, session, reset and Present joins only',
                  external_provenance_verified=False, visual_quality_verified=False)
    with a.out.open('x') as stream:
        json.dump(result, stream, indent=2)
    print(json.dumps({k:result[k] for k in ('frames','groups','gaps')}))


if __name__ == '__main__':
    main()
