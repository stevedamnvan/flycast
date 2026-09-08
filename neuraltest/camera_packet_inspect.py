"""Bounded opaque decoded-packet map, not upstream transform/camera evidence."""
import argparse
from collections import Counter
import json
from pathlib import Path
import re

from transform_span_inspect import records
from transform_store_inspect import load_session, require


VERTICES = (4, 5, 6, 146, 147, 148)
ORDINALS = (1781, 1782, 1783)


def words(text, count):
    require(re.fullmatch(r'[0-9a-f]{8}(,[0-9a-f]{8}){' + str(count-1) + '}', text), 'word encoding')
    return [int(value, 16) for value in text.split(',')]


def inspect(session, frames):
    require('FC067_CAMERA_PACKET_REJECT' not in session, 'live packet rejection')
    require(len(frames) == 3, 'frame bound')
    events = []
    for line in session.splitlines():
        match = re.search(r'FC067_CAMERA_PACKET_(BEGIN|DECODE|FINAL|END) ', line)
        if match:
            events.append((match[1], records(line, 'FC067_CAMERA_PACKET_'+match[1])[0]))
    require(len(events) == 42, 'event coverage')
    pattern = ['BEGIN'] + ['DECODE']*6 + ['FINAL']*6 + ['END']
    require([kind for kind, _ in events] == pattern*3, 'event order')
    output, epoch, last_cycle = [], None, 0
    for position, ((scene, manifest), ordinal) in enumerate(zip(frames, ORDINALS)):
        rows = [row for _, row in events[position*14:(position+1)*14]]
        begin, end = rows[0], rows[-1]
        stamp = manifest['producer_identity']
        require(stamp['available'] is True and stamp['clock'] == 'sh4-scheduler-cycles'
                and stamp['ordinal'] == ordinal and stamp['epoch'] > 0, 'producer identity')
        require(scene['frame_id'] == manifest['frame_id'] == ordinal+1
                and scene['git_sha'] == manifest['git_sha']
                and scene['game_id'] == manifest['game_id'] == 'T1401N', 'frame/scene identity')
        require(all(int(begin[key]) == stamp[key] for key in ('epoch', 'ordinal', 'cycle'))
                and int(begin['sequence']) == position+1, 'begin identity')
        require(int(end['ordinal']) == ordinal and int(end['frames']) == position+1
                and int(end['packets']) == (position+1)*6, 'terminal counters')
        require((epoch is None or stamp['epoch'] == epoch) and stamp['cycle'] > last_cycle,
                'epoch reset or cycle reversal')
        epoch, last_cycle = stamp['epoch'], stamp['cycle']
        offsets, draw_counts = set(), Counter()
        for slot, vertex in enumerate(VERTICES):
            decoded, final = rows[slot+1], rows[slot+7]
            require(all(int(decoded[key]) == stamp[key] for key in ('epoch', 'ordinal', 'cycle'))
                    and int(decoded['expected_epoch']) == stamp['epoch'], 'sample producer mismatch')
            require(int(final['ordinal']) == ordinal and int(final['list']) == 0, 'final frame/list')
            require(all(int(row['slot']) == slot and int(row['vertex']) == vertex for row in (decoded, final)),
                    'slot/vertex association')
            require(decoded['bytes'] == '32' and decoded['part'] == '0' and decoded['type'] == '3',
                    'unsupported selected packet layout')
            address, offset = int(decoded['child'], 16), int(decoded['offset'])
            require(decoded['child'] == begin['root'] and 0 <= address <= 0xffffffff
                    and 0 <= offset <= 8*1024*1024-32 and offset % 32 == 0
                    and (address, offset) not in offsets, 'packet offset/alias')
            offsets.add((address, offset))
            require(final['child'] == decoded['child'] and final['offset'] == decoded['offset'],
                    'decoder/final packet ownership')
            packet = words(decoded['words'], 8)
            require(packet[0] >> 29 == 7, 'not a vertex packet')
            require(vertex < len(scene['vertices']) and packet[1:4] == words(final['xyz'], 3)
                    == scene['vertices'][vertex][:3], 'packet/decoded geometry mismatch')
            draw_id, index = int(final['draw']), int(final['index'])
            draws = [draw for draw in scene['draws'] if draw['list'] == 0 and draw['ordinal'] == draw_id]
            require(len(draws) == 1, 'missing/duplicate draw')
            draw = draws[0]
            require(draw['range_space'] == 'indices' and draw['count'] > 0
                    and all(int(final[key]) == draw[key] for key in ('first', 'count', 'tcw', 'tsp')),
                    'draw contract mismatch')
            first, count = draw['first'], draw['count']
            require(0 <= first <= index < first+count <= len(scene['indices'])
                    and scene['indices'][index] == vertex, 'index association')
            owners = [d['ordinal'] for d in scene['draws'] if d['list'] == 0
                      and vertex in scene['indices'][d['first']:d['first']+d['count']]]
            require(owners == [draw_id], 'ambiguous opaque membership')
            draw_counts[draw_id] += 1
            output.append(dict(producer=dict(stamp), frame_id=manifest['frame_id'], slot=slot,
                               vertex=vertex, root=begin['root'], child=decoded['child'],
                               ta_offset=offset, draw=draw_id, index=index, type=3,
                               packet_words=packet))
        require(sorted(draw_counts.values()) == [3, 3], 'not three samples from each of two opaque draws')
    return dict(scope='decoded-TA-packet-map-only', frames=3, observed_vertices=18,
                packets=output, source_value_matching_used=False,
                upstream_transform_linked=False, usable_camera_contract=False,
                shared_camera_proven=False, raster_visibility_proven=False,
                production_enabled=False)


def check_controls(session, frames):
    def change(tag, key, transform):
        lines = session.splitlines()
        for index, line in enumerate(lines):
            if 'FC067_CAMERA_PACKET_'+tag+' ' not in line:
                continue
            row = records(line, 'FC067_CAMERA_PACKET_'+tag)[0]
            lines[index] = re.sub(r'(?<!\w)'+key+r'=[^ \r\n]+',
                                  key+'='+transform(row[key]), line, count=1)
            return '\n'.join(lines)+'\n'
        raise ValueError('control target missing')
    def wrong_word(value):
        values = words(value, 8)
        values[1] ^= 1
        return ','.join(f'{v:08x}' for v in values)
    first = next(line for line in session.splitlines() if 'FC067_CAMERA_PACKET_BEGIN ' in line)
    variants = [change('DECODE', 'expected_epoch', lambda v: str(int(v)+1)),
                change('DECODE', 'words', wrong_word),
                change('FINAL', 'count', lambda v: str(int(v)+1)),
                change('FINAL', 'index', lambda v: str(int(v)+1)),
                change('DECODE', 'offset', lambda v: str(int(v)+32)),
                change('BEGIN', 'epoch', lambda v: str(int(v)+1)),
                session+'\n'+first+'\n', session.replace(first, '', 1)]
    for changed in variants:
        require(changed != session, 'control did not mutate')
        try:
            inspect(changed, frames)
        except ValueError:
            continue
        raise ValueError('wrong packet control accepted')
    return len(variants)


def inspect_capture(path):
    require(sorted(p.name for p in path.glob('frame-*') if p.is_dir())
            == [f'frame-{ordinal+1:06d}' for ordinal in ORDINALS], 'capture frame set')
    frames = []
    for ordinal in ORDINALS:
        frame = path/f'frame-{ordinal+1:06d}'
        def read(name):
            file = frame/name
            require(file.stat().st_size <= 8*1024*1024, 'JSON bound')
            return json.loads(file.read_text(encoding='utf-8'))
        frames.append((read('pvr-scene.json'), read('manifest.json')))
    session = load_session(path)
    result = inspect(session, frames)
    result['offline_controls_rejected'] = check_controls(session, frames)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, required=True)
    args = parser.parse_args()
    try:
        print(json.dumps(inspect_capture(args.capture), sort_keys=True))
    except (ValueError, OSError, KeyError, IndexError, TypeError) as error:
        parser.exit(1, f'Camera packet map rejected: {error}\n')
