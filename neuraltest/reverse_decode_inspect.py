"""Verify one TA-buffer-to-opaque-vertex witness; not earlier transfer provenance."""
import argparse
import json
from pathlib import Path

from transform_span_inspect import records
from transform_store_inspect import load_session, require


def inspect(session, scene, manifest):
    decoded = records(session, 'FC067_REVERSE_DECODE')
    final = records(session, 'FC067_REVERSE_FINAL')
    require(len(decoded) == len(final) == 1, 'witness cardinality')
    require('FC067_REVERSE_REJECT' not in session, 'decoder rejected observation')
    require(session.index('FC067_REVERSE_DECODE ') < session.index('FC067_REVERSE_FINAL '),
            'witness order')
    d, f = decoded[0], final[0]
    stamp = manifest['producer_identity']
    require(stamp['available'] and stamp['clock'] == 'sh4-scheduler-cycles', 'missing producer')
    require(all(int(d[k]) == stamp[k] for k in ('epoch', 'ordinal', 'cycle')),
            'stale producer generation')
    require(stamp['ordinal'] == 1781 and stamp['cycle'] == 7605222912, 'different target producer')
    require(scene['schema'] == 'flycast-pvr-scene-v2' and scene['frame_id'] == manifest['frame_id'] == 1782
            and scene['game_id'] == manifest['game_id'] == 'T1401N', 'different scene')
    require(d['context'] == '00509700' and d['type'] == '3' and d['part'] == '0'
            and d['bytes'] == '32', 'unsupported packet variant')
    require(int(d['offset']) == 32 and int(d['input_offset']) == 36
            and f['offset'] == d['offset'], 'packet/header offset disagreement')
    require(d['vertex'] == f['vertex'] == '4' and f['list'] == '0' and f['draw'] == '1',
            'different final vertex')
    words = [int(w, 16) for w in d['words'].split(',')]
    require(len(words) == 8 and words[0] >> 29 == 7, 'not a complete vertex packet')
    require(int(f['vertices']) == len(scene['vertices']) and int(f['indices']) == len(scene['indices']),
            'different final buffers')
    draws = [draw for draw in scene['draws'] if draw['list'] == 0 and draw['ordinal'] == 1]
    require(len(draws) == 1, 'missing or ambiguous opaque draw')
    draw = draws[0]
    require(draw['range_space'] == 'indices'
            and all(draw[k] == int(f[k]) for k in ('first', 'count', 'tcw', 'tsp'))
            and draw['first'] == 4 and draw['count'] == 166
            and draw['tcw'] == 392880 and draw['tsp'] == 543696109, 'draw/material disagreement')
    require(draw['texture'] == dict(upload_generation=1, palette_hash=None, rtt_generation=0)
            and draw['texture1'] is None and not draw['naomi2'], 'texture generation or platform changed')
    require(scene['indices'][draw['first']] == 4, 'final index is not decoded vertex')
    vertex = scene['vertices'][4]
    require(vertex[:5] == words[1:6], 'packet/vertex position or UV disagreement')
    require(vertex[5] == [(words[6] >> shift) & 255 for shift in (0, 8, 16, 24)]
            and vertex[6] == [(words[7] >> shift) & 255 for shift in (0, 8, 16, 24)],
            'packet/vertex color disagreement')
    return dict(scope='one-type3-TA-buffer-to-opaque-vertex', packet_offset=32,
                vertex_input_offset=36, final_vertex=4, transfer_provenance=False,
                cpu_producer_provenance=False, camera_recovered=False)


def inspect_capture(capture):
    frame = capture / 'frame-001782'
    def read(name):
        path = frame / name
        require(path.stat().st_size <= 8 * 1024 * 1024, 'JSON exceeds bound')
        return json.loads(path.read_text(encoding='utf-8'))
    return inspect(load_session(capture), read('pvr-scene.json'), read('manifest.json'))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, required=True)
    args = parser.parse_args()
    try:
        print(json.dumps(inspect_capture(args.capture), sort_keys=True))
    except (ValueError, OSError, KeyError, IndexError, TypeError) as error:
        parser.exit(1, f'reverse decoder evidence rejected: {error}\n')
