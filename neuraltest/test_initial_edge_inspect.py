import unittest

from initial_edge_inspect import bind_initial, check_controls, inspect_edge


def fixture(identity=False, w=0x3f800000, shift=False):
    # Independent binary32 goldens: M*[1,2,3,1] = [10,5,21,1].
    # Column-major, asymmetric M exercises both layout and translation.
    matrix = [0x40000000, 0x3f800000, 0, 0,
              0, 0x40400000, 0x3f800000, 0,
              0x3f800000, 0, 0x40800000, 0,
              0x40a00000, 0xc0000000, 0x40e00000, 0x3f800000]
    output = [0x41200000, 0x40a00000, 0x41a80000, w]
    if shift:
        matrix[12] = 0x40c00000  # Translation X=6 -> output X=11.
        output[0] = 0x41300000
    if identity:
        matrix = [0x3f800000 if i//4 == i%4 else 0 for i in range(16)]
        output = [0x3f800000, 0x40000000, 0x40400000, w]
    point = [0x3f800000, 0x40000000, 0x40400000, w]
    live = {0: 0x8ce74250, 5: 0x8c010000, 8: 31, **{32+i: v for i, v in enumerate(matrix)}}
    rows = ['FC067_SUPPLY_ENTRY block=8c03c93a cycle=7602512960 descriptor=1 ops=11 inputs=19 fpscr=40001 mxcsr=fffd']
    rows += [f'FC067_SUPPLY_INPUT reg={r} value={v:x} expected={v:x} exact=1' for r, v in live.items()]
    ops = []
    for i in range(4):
        ops += [('readm', 0x8c03c93a+2*i, 4, f'r{16+i}:1:1', f'r5:1:{i}', '-', [point[i]]),
                ('add', 0x8c03c93a+2*i, 0, f'r5:1:{i+1}', f'r5:1:{i}', 'i4', [live[5]+4*(i+1)])]
    ops += [('add', 0x8c03c942, 0, 'r0:1:1', 'r0:1:0', 'ic', [0x8ce7425c]),
            ('ftrv', 0x8c03c944, 0, 'r16:4:2,2,2,2', 'r16:4:1,1,1,1', 'r32:16:'+','.join(['0']*16), output),
            ('test', 0x8c03c946, 0, 'r68:1:1', 'r8:1:0', 'r0:1:1', [0])]
    for index, (op, pc, size, rd, a, b, _) in enumerate(ops):
        rows.append(f'FC067_SUPPLY_OP index={index} pc={pc:x} op={op} size={size} rd={rd} rd2=- rs1={a} rs2={b} rs3=-')
    event = 0
    for index, (op, pc, size, rd, a, b, values) in enumerate(ops):
        if op == 'readm':
            event += 1
            rows.append(f'FC067_SUPPLY_READ event={event} index={index} address={live[5]+2*index:x} size=4')
        reg, count, versions = rd[1:].split(':')
        for part, value in enumerate(values):
            event += 1
            rows.append(f'FC067_SUPPLY_VALUE event={event} index={index} slot=0 part={part} reg={int(reg)+part} version={versions.split(",")[part]} count=1 value={value:x}')
    xyz = f'x={output[0]:x} y={output[1]:x} z={output[2]:x}'
    rows += [f'FC067_SUPPLY_EXIT descriptor=1 events=18 next=8c03c94c pointer=8ce7425c {xyz} cycle=7602512960 generation=1 mxcsr=fffd',
             f'FC067_SUPPLY_EDGE block=8c03c94c pointer=8ce7425c {xyz} expected_z={output[2]:x} cycle=7602512960 generation=1 exact=1']
    return '\n'.join(rows)+'\n'


def bound_fixture(supply=None):
    return ('FC067_RAM_BEGIN generation=1 start=8ce74250\n'
            + (fixture() if supply is None else supply)
            + 'FC067_RAM_WRITE event=1 pc=8c03c94e\nFC067_INITIAL_ENTRY block=8c03c94c\n'
            + 'FC067_INITIAL_INPUT reg=16 value=41200000\n'
            + 'FC067_INITIAL_INPUT reg=17 value=40a00000\n'
            + 'FC067_INITIAL_INPUT reg=18 value=41a80000\n')


class InitialEdgeTests(unittest.TestCase):
    def test_independent_matrix_goldens_and_controls(self):
        result = inspect_edge(fixture())
        self.assertEqual(result['transformed_words'], [0x41200000, 0x40a00000, 0x41a80000, 0x3f800000])
        self.assertEqual(result['source_loads'], [(0x8c010000+4*i, v) for i, v in enumerate([0x3f800000, 0x40000000, 0x40400000, 0x3f800000])])
        self.assertTrue(result['initial_coordinate_calculation_proven'])
        self.assertFalse(result['world_camera_recovered'])
        self.assertEqual(result['source_coordinate_space'], 'unknown')
        self.assertEqual(check_controls(fixture()), 6)

    def test_nonunit_w_is_not_normalized(self):
        result = inspect_edge(fixture(identity=True, w=0x3f8005df))
        self.assertEqual(result['source_point_words'][3], 0x3f8005df)
        self.assertEqual(result['transformed_words'][3], 0x3f8005df)
        with self.assertRaises(ValueError):
            inspect_edge(fixture(identity=True, w=0x3f8005df).replace('version=2 count=1 value=3f8005df', 'version=2 count=1 value=3f800000'))

    def test_edge_identity_and_live_negative(self):
        for old, new in [('block=8c03c94c', 'block=8c03c94e'), ('generation=1 exact=1', 'generation=2 exact=1'),
                         ('pointer=8ce7425c', 'pointer=8ce7426c'), ('expected_z=41a80000', 'expected_z=41a80001'),
                         ('cycle=7602512960 generation=1 exact=1', 'cycle=7602512961 generation=1 exact=1')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                inspect_edge(fixture().replace(old, new, 1))
        with self.assertRaises(ValueError):
            inspect_edge(fixture()+'FC067_SUPPLY_REJECT reason=next-entry-mismatch\n')

    def test_missing_duplicate_or_reordered_events(self):
        lines = fixture().splitlines(keepends=True)
        value = next(line for line in lines if 'FC067_SUPPLY_VALUE event=14 ' in line)
        edge = lines[-1]
        for source in [fixture()+fixture(), fixture().replace(value, ''), fixture()+value,
                       fixture().replace(edge, ''), edge+fixture().replace(edge, ''),
                       fixture().replace('event=18 index=10', 'event=17 index=10')]:
            with self.assertRaises(ValueError):
                inspect_edge(source)

    def test_load_layout_ssa_and_fp_faults(self):
        for old, new in [('address=8c010008', 'address=8c01000c'), ('rs1=r16:4:1,1,1,1', 'rs1=r16:4:0,0,0,0'),
                         ('part=2 reg=18 version=2', 'part=1 reg=18 version=2'), ('value=41a80000', 'value=41a80001'),
                         ('reg=32 value=40000000 expected=40000000', 'reg=32 value=7f800000 expected=7f800000'),
                         ('op=ftrv', 'op=fmac'), ('fpscr=40001', 'fpscr=0'), ('mxcsr=fffd', 'mxcsr=9ffd')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                inspect_edge(fixture().replace(old, new, 1))

    def test_successor_and_lease_binding(self):
        source = bound_fixture()
        bind_initial(source, inspect_edge(source))
        # A self-consistent alternative transform/edge still cannot supply
        # unchanged successor operands. This is not only a checksum test.
        changed = bound_fixture(fixture(shift=True))
        alternative = inspect_edge(changed)
        with self.assertRaises(ValueError):
            bind_initial(changed, alternative)
        for changed in [source.replace('FC067_SUPPLY_ENTRY ', 'FC067_RAM_STOP reason=reset\nFC067_SUPPLY_ENTRY ', 1),
                        source.replace('generation=1 start=', 'generation=2 start='),
                        source.replace('FC067_RAM_WRITE event=1', 'FC067_RAM_WRITE event=2')]:
            with self.assertRaises(ValueError):
                bind_initial(changed, inspect_edge(changed))

    def test_prior_observer_lines_do_not_shadow_controls(self):
        self.assertEqual(check_controls('FC067_INITIAL_INPUT reg=44 value=40a00000 expected=40a00000\n'+fixture()), 6)


if __name__ == '__main__':
    unittest.main()
