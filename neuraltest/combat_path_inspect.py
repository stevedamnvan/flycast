"""Bounded combat candidate projection/RAM path, not an opaque draw association."""
import argparse
import json
from pathlib import Path
import re

from binary32_oracle import evaluate
from transform_semantics_inspect import dot_words
from transform_span_inspect import records
from transform_store_inspect import load_session, compare_native, require


def words(value):
    result = [int(v, 16) for v in value.split(',')]
    require(all(0 <= v <= 0xffffffff for v in result), 'invalid binary32 word')
    return result


def projection(source, scale, offsets, mode):
    require(len(source) == 4 and source[3] == 0x3f800000 and len(offsets) == 2, 'projection input contract')
    reciprocal = evaluate('div', source[3], source[2], 0, mode)
    products = [evaluate('mul', v, reciprocal, 0, mode) for v in source[:2]]
    # Multiplying an already rounded product by exactly one is exact; the
    # existing oracle then models the separate fadd, not fused projection.
    xy = [evaluate('madd', offset, p, 0x3f800000, mode) for p, offset in zip(products, offsets)]
    return xy + [evaluate('mul', reciprocal, scale, 0, mode)]


def inspect_session(session):
    require('FC067_COMBAT_METADATA ' not in session, 'missing executed block metadata')
    sources = records(session, 'FC067_COMBAT_SOURCE')
    require(len(sources) == 1, 'expected one selected transform')
    source = sources[0]
    require(source['pc'] == '8c0610b4' and source['role'] == 'unknown', 'wrong candidate or promoted role')
    cycle = int(source['cycles'])
    require(7600000000 <= cycle < 7800000000, 'candidate outside combat window')
    matrices = records(session, 'FC067_COMBAT_MATRIX')
    require([int(m['column']) for m in matrices] == list(range(4)), 'matrix column order')
    output = words(source['output'])
    require(len(words(source['input'])) == len(output) == 4 and all(len(words(m['bits'])) == 4 for m in matrices),
            'transform vector/matrix shape')
    require(dot_words([words(m['bits']) for m in matrices], words(source['input'])) == output,
            'candidate transform arithmetic differs')
    blocks = records(session, 'FC067_COMBAT_BLOCK')
    constants = records(session, 'FC067_COMBAT_CONSTANTS')
    buffers = records(session, 'FC067_COMBAT_BUFFER')
    for rows in (blocks, constants, buffers):
        require([int(r['step']) for r in rows] == list(range(1, 9)), 'missing/reordered bounded observations')
    require(all(len(words(b['f'])) == 8 for b in blocks)
            and all(len(words(b['words'])) == 8 for b in buffers)
            and all(len(words(c['offset'])) == 2 for c in constants), 'register/buffer observation shape')
    for i, block in enumerate(blocks):
        require(cycle <= int(block['cycles']) <= cycle+20000000, 'block outside cycle bound')
        if i:
            require(blocks[i-1]['next'] == block['block'], 'executed block path discontinuity')
    require([b['block'] for b in blocks[:3]] == ['8c0610a0', '8c0610c2', '8c0610d2'], 'wrong candidate path')
    require(records(session, 'FC067_COMBAT_STOP') == [dict(reason='eight-block-bound', ta_lineage='unknown')],
            'missing bounded termination')
    ops = re.findall(r'FC067_COMBAT_EXEC_SHIL step=3 block=8c0610d2 descriptor=\d+ pc=([0-9a-f]+) op=(.*)', session)
    expected = [
        ('8c0610d2','fdiv f3.1 <- f3.0, f2.0'), ('8c0610d4','sub r6.1 <- r6.0, 1'),
        ('8c0610d4','seteq sr.T.1 <- r6.1, 0'), ('8c0610d6','fmul f0.1 <- f0.0, f3.1'),
        ('8c0610d8','fmul f1.1 <- f1.0, f3.1'), ('8c0610da','fmul f3.2 <- f3.1, f4.0'),
        ('8c0610dc','fadd f0.2 <- f0.1, f8.0'), ('8c0610de','fadd f1.2 <- f1.1, f9.0'),
        ('8c0610e0','sub r5.1 <- r5.0, 4'), ('8c0610e0','writem  <- r5.1, f3.2'),
        ('8c0610e2','sub r5.2 <- r5.1, 4'), ('8c0610e2','writem  <- r5.2, f1.2'),
        ('8c0610e4','sub r5.3 <- r5.2, 4'), ('8c0610e4','writem  <- r5.3, f0.2'),
        ('8c0610e6','jcond pc_dyn.1 <- sr.T.1'), ('8c0610e8','add r5.4 <- r5.3, 16')]
    require(ops == expected, 'executed projection/store dependencies differ')
    before, after = words(blocks[1]['f']), words(blocks[2]['f'])
    require(words(blocks[0]['f']) == before and before[:4] == output, 'intervening source register change')
    offsets = words(constants[1]['offset'])
    require(all(c['offset'] == constants[0]['offset'] and c['mxcsr'] == constants[0]['mxcsr'] for c in constants[:3]),
            'projection constants or rounding changed')
    mode = (int(constants[1]['mxcsr'], 16) >> 13) & 3
    require(mode == 3, 'unexpected observed rounding')
    derived = projection(output, before[4], offsets, mode)
    require(derived == [after[0], after[1], after[3]], 'projected register arithmetic mismatch')
    base = int(buffers[0]['address'], 16)
    require((base & 0x1c000000) == 0x0c000000 and base % 4 == 0
            and (base & 0xffffff) <= 0x1000000-32, 'invalid pinned RAM span')
    require(all(int(r['address'], 16) == base for r in buffers), 'unpinned output buffer')
    require(int(blocks[1]['r5'], 16) == base+12 and int(blocks[2]['r5'], 16) == base+16,
            'store address dependency mismatch')
    require(words(buffers[2]['words'])[:3] == derived, 'RAM output mismatch')
    require(words(buffers[0]['words'])[3:] == words(buffers[2]['words'])[3:], 'unexpected adjacent buffer change')
    return dict(scope='one-combat-transform-to-projected-RAM-block-path', cycles=cycle,
                output_base=f'{base:08x}', projected_words=[f'{v:08x}' for v in derived],
                depth_scale_word=f'{before[4]:08x}', offset_words=constants[1]['offset'],
                role='unknown', ta_lineage='unproven', camera='unproven')


def verify(capture, baseline):
    session = load_session(capture)
    result = inspect_session(session)
    mutations = [('step=3 block=8c0610d2', 'step=9 block=8c0610d2'),
                 ('fdiv f3.1 <- f3.0, f2.0', 'fdiv f3.1 <- f2.0, f3.0'),
                 ('writem  <- r5.3, f0.2', 'writem  <- r5.3, f1.2'),
                 ('reason=eight-block-bound', 'reason=cycle-bound'),
                 ('words='+','.join(result['projected_words']), 'words=00000000,'+','.join(result['projected_words'][1:]))]
    for original, wrong in mutations:
        require(original in session, 'control did not target observed text')
        try:
            inspect_session(session.replace(original, wrong, 1))
        except ValueError:
            pass
        else:
            raise ValueError('wrong path control accepted')
    result.update(rejected_offline_controls=len(mutations),
                  native_plane_comparisons=compare_native(capture, capture, baseline))
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('capture', 'baseline'):
        parser.add_argument('--'+name, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(verify(args.capture, args.baseline), indent=2))
