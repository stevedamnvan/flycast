"""Bounded combat packet/copy/quad witness, not whole-scene camera proof."""
import argparse
import copy
import json
import re
from pathlib import Path

from combat_path_inspect import inspect_session, words
from transform_span_inspect import records
from transform_store_inspect import load_session, require


def one(session, tag):
    rows = records(session, tag)
    require(len(rows) == 1, 'missing or repeated '+tag)
    return rows[0]


def inspect(session, scene):
    source = inspect_session(session)
    watched = one(session, 'FC067_WATCH_BEGIN')
    require(watched['base'] == source['output_base'] and watched['active'] == '1'
            and words(watched['words']) == words(','.join(source['projected_words'])), 'wrong watched source')
    accesses = records(session, 'FC067_WATCH_ACCESS')
    reads = records(session, 'FC067_WATCH_READ')
    stores = records(session, 'FC067_WATCH_STORE')
    require(len(accesses) == 7 and len(reads) == 6, 'unexpected source lifetime')
    for i in range(3):
        access, read, store = accesses[i], reads[i], stores[i]
        pc, event = 0x8c06fc2e+i*4, 177+i*2
        expected = int(source['projected_words'][i],16)
        require(int(access['event']) == int(read['event']) == event and int(store['event']) == event+1
                and int(access['pc'],16) == int(read['pc'],16) == pc and int(store['pc'],16) == pc+2
                and int(access['address'],16) == int(source['output_base'],16)+i*4
                and int(store['address'],16) == 0xe0de71a4+i*4
                and access['size'] == store['size'] == '4' and access['write'] == '0'
                and read['exact'] == store['exact'] == store['sq'] == '1' and store['ram'] == '0',
                'first actual read/copy identity differs')
        require(all(int(row[key],16) == expected for row,key in
                    ((access,'bytes'),(read,'value'),(read,'expected'),(store,'value'),(store,'stored'))),
                'read/copy value differs')
    ops = re.findall(r'FC067_WATCH_SHIL event=177 pc=8c06fc2e descriptor=\d+ pc=([0-9a-f]+) op=(.*)',session)
    selected = [(pc,op) for pc,op in ops if 0x8c06fc2e <= int(pc,16) <= 0x8c06fc38]
    require(selected == [('8c06fc2e','readm r3.3 <- r4.4'),('8c06fc30','writem  <- r6.1, r3.3, 4'),
                         ('8c06fc32','readm r2.2 <- r4.4, 4'),('8c06fc34','writem  <- r6.1, r2.2, 8'),
                         ('8c06fc36','readm r3.4 <- r4.4, 8'),('8c06fc38','writem  <- r6.1, r3.4, 12')],
            'actual compiled copy dependencies differ')
    stop = one(session,'FC067_WATCH_STOP')
    require(stop['reason'] == 'cpu-overwrite' and stop['events'] == '14567'
            and accesses[-1]['write'] == '1' and accesses[-1]['event'] == stop['events']
            and accesses[-1]['address'] == source['output_base'], 'source overwrite missing')
    flush = one(session, 'FC067_WATCH_FLUSH')
    packet = words(flush['words'])
    require(flush['pc'] == '8c06fc4c' and flush['address'] == 'e0de71a0'
            and flush['area'] == '3' and flush['destination'] == '0cde71a0'
            and flush['ram_exact'] == flush['xyz_exact'] == '1', 'wrong first flush')
    require(len(packet) == 8 and packet[1:4] == words(','.join(source['projected_words'])), 'flush source differs')
    begin = one(session, 'FC067_PACKET_BEGIN')
    require(begin == dict(base='0cde71a0', bytes='32', source='first-combat-sq-flush'), 'wrong packet generation')
    ta = one(session, 'FC067_PACKET_TA')
    require(ta['base'] == begin['base'] and ta['exact'] == ta['ta'] == '1'
            and ta['address'] == '00000000' and int(ta['count']) == 1086
            and int(ta['offset']) == 128 and int(ta['events']) == 634413, 'wrong actual TA range')
    require(int(ta['start']) == int(flush['cycles']) and 0 < int(ta['cycles'])-int(ta['start']) < 200000000,
            'packet age/source differs')
    stops = records(session, 'FC067_PACKET_STOP')
    require(len(stops) == 1 and stops[0]['reason'] == 'actual-ta-input-observed', 'packet invalidated')
    copied = one(session, 'FC067_PACKET_COPY')
    decoded = one(session, 'FC067_PACKET_DECODE')
    require(copied['exact'] == '1' and copied['bytes'] == '32' and copied['offset'] == '516800'
            and copied['generation'] == decoded['generation'] == '1', 'copy generation mismatch')
    require(copied['context'] == decoded['child'] == decoded['root'] == decoded['expected_context'] == '00509700'
            and decoded['context_exact'] == decoded['packet_exact'] == decoded['xyz_exact'] == '1'
            and words(decoded['xyz']) == packet[1:4] and decoded['vertex'] == '14770', 'decoded witness differs')
    epoch = one(session, 'FC067_CALIB_COPY')
    decode_epoch = one(session, 'FC067_CALIB_DECODE')
    snapshot = one(session, 'FC067_SCENE_SNAPSHOT')
    require(epoch['epoch'] == decode_epoch['epoch'] == snapshot['epoch'] == '1790'
            and snapshot['context'] == copied['context'] and snapshot['vertex'] == decoded['vertex']
            and snapshot['written'] == '1' and snapshot['rtt'] == '0', 'snapshot identity differs')
    tags = ['FC067_WATCH_FLUSH ', 'FC067_PACKET_BEGIN ', 'FC067_PACKET_TA ',
            'FC067_PACKET_COPY ', 'FC067_PACKET_DECODE ', 'FC067_SCENE_SNAPSHOT ']
    positions = [session.index(tag) for tag in tags]
    require(positions == sorted(positions), 'witness order differs')
    require(scene['schema'] == 'flycast-pvr-scene-v2' and scene['frame_id'] == int(snapshot['frame']) == 1782
            and scene['game_id'] == 'T1401N' and len(scene['vertices']) == int(snapshot['vertices'])
            and len(scene['indices']) == int(snapshot['indices']), 'scene identity/count mismatch')
    require(scene['vertices'][14770][:3] == packet[1:4], 'snapshot vertex changed')
    sources = [d for d in scene['draws'] if d['list'] == 2 and d['range_space'] == 'vertices'
               and d['first'] <= 14770 < d['first']+d['count']]
    require(len(sources) == 1 and sources[0]['ordinal'] == 0 and sources[0]['first'] == 14768
            and sources[0]['count'] == 4 and not sources[0]['naomi2'], 'unsupported source quad')
    hits = []
    for ordinal, command in enumerate(scene['sorted_triangles']):
        first, count = command['first'], command['count']
        require(count % 3 == 0 and 0 <= first <= len(scene['indices'])
                and 0 <= count <= len(scene['indices'])-first, 'invalid sorted range')
        for offset in range(first, first+count, 3):
            triangle = scene['indices'][offset:offset+3]
            if 14770 in triangle:
                hits.append((ordinal, command['poly_index'], offset, triangle))
    require(hits == [(10,4,17580,[14770,14769,14771]), (10,4,17586,[14768,14769,14770])], 'quad submission differs')
    material = [d for d in scene['draws'] if d['list'] == 2 and d['ordinal'] == 4]
    require(len(material) == 1, 'missing material state')
    require(all(sources[0][k] == material[0][k] for k in ('tcw','tsp','texture','texture1','tcw1','tsp1','tileclip')),
            'material generation/state mismatch')
    a,b = sources[0],material[0]
    require(((a['pcw']^b['pcw'])&0x300CE) == 0 and ((a['isp']^b['isp'])&0xF4000000) == 0
            and (((a['isp']>>27)&3)<2 or ((a['isp']>>27)&3)==((b['isp']>>27)&3)),
            'material render state differs')
    return dict(scope='one-combat-translucent-quad', frame=1782, vertex=14770,
                source_draw=0, material_draw=4, camera='unproven', visibility='unproven')


def verify(session, scene):
    result = inspect(session, scene)
    mutations = [('ram_exact=1','ram_exact=0'), ('offset=128 base=','offset=160 base='),
                 ('reason=actual-ta-input-observed','reason=cpu-overwrite'),
                 ('offset=516800','offset=516832'), ('vertex=14770','vertex=14771'),
                 ('epoch=1790','epoch=1791'), ('context_exact=1','context_exact=0'),
                 ('writem  <- r6.1, r3.3, 4','writem  <- r6.1, r2.2, 4'),
                 ('address=e0de71a4','address=e0de7224')]
    for before, after in mutations:
        require(before in session, 'missing negative target')
        try:
            inspect(session.replace(before, after, 1), scene)
        except ValueError:
            continue
        raise ValueError('accepted wrong packet witness')
    wrong = copy.deepcopy(scene)
    wrong['indices'][17580] = 14768
    try:
        inspect(session, wrong)
    except ValueError:
        pass
    else:
        raise ValueError('accepted wrong quad triangle')
    return dict(result, rejected_offline_controls=10)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, required=True)
    parser.add_argument('--scene', type=Path, required=True)
    args = parser.parse_args()
    require(args.scene.stat().st_size <= 32*1024*1024, 'scene bound')
    print(json.dumps(verify(load_session(args.capture), json.loads(args.scene.read_text())), indent=2))
