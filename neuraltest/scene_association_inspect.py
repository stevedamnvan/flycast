"""Bind linked decoded samples to a captured scene; no world/visibility promotion."""
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess

from shared_calibration_inspect import load_sample, compare_samples
from transform_span_inspect import records
from transform_store_inspect import load_session, require


def associate(scene, snapshot, witness, epoch):
    require(scene['schema'] == 'flycast-pvr-scene-v2', 'association needs captured sorted order')
    require(snapshot['written'] == '1' and snapshot['rtt'] == '0', 'snapshot not accepted geometry')
    require(int(snapshot['epoch']) == epoch and int(snapshot['context'], 16) == witness['context'],
            'snapshot epoch/context mismatch')
    require(int(snapshot['frame']) == scene['frame_id'] > 0, 'snapshot frame mismatch')
    vertex = witness['vertex']
    require(int(snapshot['vertex']) == vertex, 'snapshot vertex mismatch')
    vertices, indices = scene['vertices'], scene['indices']
    require(len(vertices) == int(snapshot['vertices']) <= 65536
            and len(indices) == int(snapshot['indices']) <= 262144, 'snapshot geometry counts')
    require(0 <= vertex < len(vertices) and vertices[vertex][:3] == witness['ta']['packet'][1:4],
            'witnessed vertex position changed')
    require(len(scene['draws']) <= 8192 and len(scene['sorted_triangles']) <= 262144, 'association count bound')
    sources = [d for d in scene['draws'] if d['list'] == 2 and d['range_space'] == 'vertices'
               and d['first'] <= vertex < d['first'] + d['count']]
    require(len(sources) == 1, 'missing or ambiguous source draw')
    source = sources[0]
    # Deliberately bounded to the actual three-vertex source family. Do not
    # silently apply strip reconstruction rules to arbitrary source draws.
    require(not source['naomi2'] and source['count'] == 3, 'unsupported source primitive family')
    expected = list(range(source['first'], source['first'] + 3))
    hits = []
    previous_end = 0
    for ordinal, command in enumerate(scene['sorted_triangles']):
        first, count = command['first'], command['count']
        require(count % 3 == 0 and 0 <= first <= len(indices) and 0 <= count <= len(indices)-first,
                'sorted command range')
        require(count == 0 or first >= previous_end, 'overlapping sorted commands')
        if count:
            previous_end = first + count
        for offset in range(first, first+count, 3):
            triangle = indices[offset:offset+3]
            if vertex in triangle:
                require(triangle == expected, 'source versus sorted triangle mismatch')
                hits.append((ordinal, command, offset))
    require(len(hits) == 1, 'missing or duplicated GPU primitive')
    ordinal, command, offset = hits[0]
    materials = [d for d in scene['draws'] if d['list'] == 2 and d['ordinal'] == command['poly_index']]
    require(len(materials) == 1 and not materials[0]['naomi2'], 'missing sorted material state')
    material = materials[0]
    source_cull, material_cull = (source['isp'] >> 27) & 3, (material['isp'] >> 27) & 3
    # Match the captured Dreamcast subset of PolyParam equivalence, plus actual
    # texture generations. This is not a semantic/PBR material interpretation.
    require(all(source[k] == material[k] for k in
                ('tcw', 'tsp', 'tileclip', 'tcw1', 'tsp1', 'texture', 'texture1'))
            and ((source['pcw'] ^ material['pcw']) & 0x300CE) == 0
            and ((source['isp'] ^ material['isp']) & 0xF4000000) == 0
            and (source_cull < 2 or source_cull == material_cull),
            'sorted material or texture generation mismatch')
    points = [[struct.unpack('<f', struct.pack('<I', n))[0] for n in vertices[i][:3]] for i in expected]
    require(all(math.isfinite(v) for p in points for v in p), 'nonfinite sampled primitive')
    width, height = scene['framebuffer_size']
    require([width, height] == [640, 480], 'unsupported sample screen rectangle')
    m = [struct.unpack('<f', struct.pack('<I', n))[0] for n in scene['viewport_bits']]
    require(len(m) == 16 and all(math.isfinite(v) for v in m)
            and all(m[i] == (1. if i in (10, 15) else 0.) for i in (1,2,3,4,6,7,8,9,10,11,14,15)),
            'unsupported viewport mapping')
    screen = [[(m[0]*p[0]+m[12]+1)*width/2, (1-m[5]*p[1]-m[13])*height/2] for p in points]
    bounds = [min(p[0] for p in screen), min(p[1] for p in screen),
              max(p[0] for p in screen), max(p[1] for p in screen)]
    return dict(vertex=vertex, source_list='translucent', source_draw=source['ordinal'],
                source_vertices=expected, sorted_command=ordinal, triangle_index_offset=offset,
                material_state_draw=material['ordinal'], texture_state=source['texture'], tcw=source['tcw'],
                screen_bounds=bounds,
                outside_viewport_bounds=bounds[2]<0 or bounds[3]<0 or bounds[0]>=width or bounds[1]>=height,
                visibility='unproven-alpha-depth-occlusion', semantic_role='unknown')


def bind_frame(witnessed, captured, frame_manifest, materials, associations):
    # The probe and restored capture have different executable SHAs. Permit
    # that one explicit difference only; never ignore frame or geometry data.
    a, b = witnessed.copy(), captured.copy()
    shas = (a.pop('git_sha'), b.pop('git_sha'))
    require(all(isinstance(sha, str) and sha for sha in shas), 'missing source SHA')
    require(json.dumps(a, sort_keys=True, allow_nan=False) == json.dumps(b, sort_keys=True, allow_nan=False),
            'matched frame scene differs from witnessed snapshot')
    require(materials['schema'] == 'flycast-source-materials-v1', 'material schema')
    for field in ('frame_id', 'game_id', 'git_sha'):
        require(captured[field] == frame_manifest[field] == materials[field], 'matched frame identity '+field)
    require(materials['scene_sha'] == captured['git_sha'], 'material scene SHA')
    result = []
    for row in associations:
        bindings = []
        for ordinal in (row['source_draw'], row['material_state_draw']):
            matches = [b for b in materials['bindings'] if b['list'] == 2 and b['ordinal'] == ordinal and b['slot'] == 0]
            require(len(matches) == 1 and matches[0]['asset'] is not None, 'missing or ambiguous material binding')
            binding = matches[0]
            generation = row['texture_state']
            require(generation is not None and binding['tcw'] == row['tcw']
                    and binding['upload_generation'] == generation['upload_generation']
                    and binding['palette_hash'] == generation['palette_hash']
                    and binding['rtt_generation'] == generation['rtt_generation'], 'material binding generation mismatch')
            require(0 <= binding['asset'] < len(materials['assets']), 'material asset range')
            bindings.append(binding)
        require(bindings[0]['asset'] == bindings[1]['asset']
                and bindings[0].get('palette_base') == bindings[1].get('palette_base'), 'source/sorted asset mismatch')
        result.append(dict(vertex=row['vertex'], asset=bindings[0]['asset'],
                           palette_base=bindings[0].get('palette_base'),
                           raw_pixels='verify-separately-with-material-inspect'))
    return result


def verify(original, different, decoder, matched_frame=None):
    samples = [load_sample(original, False), load_sample(different, True)]
    compare_samples(*samples)
    scene_bytes = []
    associations = []
    controls = 0
    for path, sample in zip((original, different), samples):
        snapshots = records(load_session(path), 'FC067_SCENE_SNAPSHOT')
        require(len(snapshots) == 1, 'expected one snapshot event')
        snapshot = snapshots[0]
        source = path/'witness'/'scene.json'
        require(0 < source.stat().st_size <= 32*1024*1024, 'scene byte bound')
        # Reuse the production bounded decoder for complete schema, pass,
        # range and nonfinite checks instead of maintaining a weaker copy.
        result = subprocess.run([str(decoder.resolve()), 'pvr-packet', '--in', str(source),
                                 '--frame', snapshot['frame'], '--game-id', 'T1401N'],
                                capture_output=True, text=True, timeout=30)
        require(result.returncode == 0, 'compiled scene decoder rejected packet: '+result.stderr[:512])
        data = source.read_bytes(); scene_bytes.append(data)
        scene = json.loads(data)
        row = associate(scene, snapshot, sample['witness'], sample['epoch'])
        associations.append(row)
        # Mutate the in-memory observations, never retained evidence. These
        # controls test this association validator, not runtime fault handling.
        mutations = [
            lambda s, log: log.update(frame=str(int(log['frame'])+1)),
            lambda s, log: log.update(epoch=str(int(log['epoch'])+1)),
            lambda s, log: log.update(vertex=str(int(log['vertex'])+1)),
            lambda s, log: s['vertices'][row['vertex']].__setitem__(0, s['vertices'][row['vertex']][0]^1),
            lambda s, log: s['sorted_triangles'][row['sorted_command']].update(poly_index=0xffffffff),
            lambda s, log: s['indices'].__setitem__(row['triangle_index_offset'], 0xffffffff),
        ]
        for mutation in mutations:
            bad, log = copy.deepcopy(scene), snapshot.copy()
            mutation(bad, log)
            try:
                associate(bad, log, sample['witness'], sample['epoch'])
            except ValueError:
                controls += 1
            else:
                raise ValueError('wrong association control accepted')
    require(scene_bytes[0] == scene_bytes[1], 'selection lanes changed witnessed scene')
    matched = None
    if matched_frame is not None:
        docs = []
        for relative, bound in (('pvr-scene.json', 32*1024*1024), ('manifest.json', 1024*1024),
                                ('materials/manifest.json', 8*1024*1024)):
            path = matched_frame/relative
            require(0 < path.stat().st_size <= bound, 'matched-frame file bound')
            docs.append(json.loads(path.read_bytes()))
        matched = bind_frame(scene, *docs, associations)
    return dict(scope='two-sampled-translucent-primitives',
                scene_sha256=hashlib.sha256(scene_bytes[0]).hexdigest(),
                frame=int(snapshots[0]['frame']), associations=associations, matched_frame_materials=matched,
                rejected_offline_controls=controls, world_camera='unproven', visible_world_coverage='unproven')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('original', 'different', 'decoder'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--matched-frame', type=Path)
    args = parser.parse_args()
    print(json.dumps(verify(args.original, args.different, args.decoder, args.matched_frame), indent=2))
