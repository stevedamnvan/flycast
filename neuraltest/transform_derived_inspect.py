"""Bounded observed arithmetic/store verification, not camera or TA recovery."""
import argparse
import json
import re
from pathlib import Path

from binary32_oracle import evaluate
from transform_span_inspect import inspect_span, records
from transform_store_inspect import compare_native, load_session, require


def inspect_derived(path, negative=False):
    source, destination, terminal = inspect_span(path, False)
    base, original = source[:2]
    session = load_session(path)
    require(re.findall(r'FC067_(ARITH_IN|ARITH_OUT|DERIVED_STORE|DERIVED_END) ', session)
            == ['ARITH_IN', 'ARITH_OUT'] + ['ARITH_IN', 'ARITH_OUT', 'DERIVED_STORE'] * 3
            + ['DERIVED_END'], 'wrong arithmetic/store execution order')
    inputs = records(session, 'FC067_ARITH_IN')
    outputs = records(session, 'FC067_ARITH_OUT')
    stores = records(session, 'FC067_DERIVED_STORE')
    pcs = (0x8c070c74, 0x8c070c88, 0x8c070c9a, 0x8c070ca2)
    require(len(inputs) == len(outputs) == 4, 'expected four arithmetic observations')
    require(len(stores) == 3, 'expected three derived stores')
    actual, operands, rejected = [], [], []
    for i, (pc, operation, row, out) in enumerate(zip(
            pcs, ('div', 'mul', 'madd', 'madd'), inputs, outputs)):
        require(int(row['pc'], 16) == int(out['pc'], 16) == pc, 'wrong arithmetic instruction')
        a, b, c, shadow = (int(row[k], 16) for k in ('a', 'b', 'c', 'oracle_a'))
        require(row['fused'] in ('0', '1'), 'invalid fused metadata')
        fused = row['fused'] == '1'
        require(fused == (i >= 2), 'unexpected observed operation implementation')
        mode = (int(row['mxcsr'], 16) >> 13) & 3
        require(mode == 3, 'capture requires observed toward-zero mode')
        value = int(out['value'], 16)
        require(evaluate(operation, a, b, c, mode, fused) == value, 'actual arithmetic mismatch')
        mutated = negative and i == 2
        require(shadow == (a ^ (0x80000000 if mutated else 0)), 'wrong shadow control')
        fails = evaluate(operation, shadow, b, c, mode, fused) != value
        require(fails == mutated, 'shadow control failed to discriminate')
        rejected.append(fails)
        actual.append(value)
        operands.append((a, b, c))
    require(operands == [(0x3f800000, original[2], 0),
                         (0x3f851eb8, actual[0], 0),
                         (0x43a00000, original[0], actual[0]),
                         (0x43700000, original[1], actual[0])], 'wrong observed arithmetic dependencies')
    require(evaluate('div', *operands[0], 0) != actual[0], 'wrong-rounding control did not fail')
    for store, pc, offset, value in zip(stores, (0x8c070c92, 0x8c070c9c, 0x8c070ca4),
                                       (8, 0, 4), actual[1:]):
        require(int(store['pc'], 16) == pc and int(store['address'], 16) == base + offset,
                'wrong derived destination')
        require(int(store['value'], 16) == int(store['stored'], 16) == value,
                'derived store/readback mismatch')
        require(store['ram'] == store['exact'] == '1', 'unverified derived store')
    require(int(terminal, 16) == actual[1], 'old generation termination mismatch')
    ends = records(session, 'FC067_DERIVED_END')
    require(ends == [dict(generation='2', candidate='true', ta_lineage='unknown', camera='unknown')],
            'invalid scope or generation end')
    return dict(source=source, destination=destination, operands=operands, actual=actual,
                rejected=rejected)


def verify(positive, negative, baseline):
    pos, neg = inspect_derived(positive), inspect_derived(negative, True)
    require(all(pos[k] == neg[k] for k in ('source', 'destination', 'operands', 'actual')),
            'negative changed actual game observations')
    planes = compare_native(positive, negative, baseline)
    return dict(arithmetic_results=4, stores=3, rejected_shadow_operations=sum(neg['rejected']),
                wrong_rounding_rejected=True, native_plane_comparisons=planes,
                ta_lineage='unproven', camera='unproven')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('positive', 'negative', 'baseline'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(verify(args.positive, args.negative, args.baseline), indent=2))
