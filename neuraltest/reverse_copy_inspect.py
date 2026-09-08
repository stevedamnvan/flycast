"""Validate the one-packet SQ-to-TA copy witness, not its earlier filling stores."""
import argparse
import json
from pathlib import Path
import re

from reverse_decode_inspect import inspect as inspect_decode
from transform_span_inspect import records
from transform_store_inspect import load_session, require


def inspect(session, scene, manifest):
    result = inspect_decode(session, scene, manifest)
    copies = records(session, 'FC067_REVERSE_COPY')
    links = records(session, 'FC067_REVERSE_LINK')
    words = records(session, 'FC067_REVERSE_COPY_WORD')
    require(len(copies) == len(links) == 1 and len(words) == 8, 'copy witness cardinality')
    require(re.findall(r'FC067_REVERSE_(COPY_WORD|COPY|LINK|DECODE|FINAL) ', session)
            == ['COPY'] + ['COPY_WORD']*8 + ['LINK', 'DECODE', 'FINAL'], 'copy event order')
    c, link = copies[0], links[0]
    require(c['generation'] == link['generation'] == '1' and c['exact'] == link['exact'] == '1',
            'missing or stale copy generation')
    require(c['context'] == '00509700' and c['offset'] == '32' and c['bytes'] == '32',
            'wrong copy interval')
    require(c['path'] == '2' and c['address'] == 'e0000020' and c['source_index'] == '0',
            'not the selected SQ transfer')
    require(int(c['source'], 16) != 0 and int(c['source'], 16) % 32 == 0
            and int(c['destination'], 16) != 0
            and c['destination'] == link['destination'] == link['packet'], 'wrong destination identity')
    require(c['cycle'] == link['cycle'] == '7602643776'
            and int(c['cycle']) < manifest['producer_identity']['cycle'], 'copy cycle disagreement')
    packet = records(session, 'FC067_REVERSE_DECODE')[0]['words'].split(',')
    for index, word in enumerate(words):
        require(word['generation'] == '1' and int(word['index']) == index
                and word['before'] == word['after'] == packet[index], 'source/copy/decoder byte mismatch')
    result.update(transfer_provenance=True, transfer_path='TAWriteSQ', sq_slot=1,
                  copy_cycle=7602643776, observation_generation=1,
                  generation_scope='one-process-one-selected-packet-no-rearm',
                  cpu_producer_provenance=False)
    return result


def check_controls(session, scene, manifest):
    link = records(session, 'FC067_REVERSE_LINK')[0]
    first = records(session, 'FC067_REVERSE_COPY_WORD')[0]
    changes = [
        ('COPY generation=1', 'COPY generation=2'),
        ('context=00509700 offset=32 bytes=32', 'context=00509700 offset=64 bytes=32'),
        ('packet='+link['packet'], 'packet=0'),
        ('before='+first['before'], 'before='+format(int(first['before'], 16)^1, '08x')),
        ('path=2 address=e0000020', 'path=1 address=e0000020')]
    for old, new in changes:
        mutated = session.replace(old, new, 1)
        require(mutated != session, 'control failed to mutate witness')
        try:
            inspect(mutated, scene, manifest)
        except ValueError:
            continue
        raise ValueError('wrong witness control accepted: '+new)
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
        parser.exit(1, f'reverse copy evidence rejected: {error}\n')
