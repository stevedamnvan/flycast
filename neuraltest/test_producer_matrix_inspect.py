import unittest

from producer_matrix_inspect import inspect


def fixture():
    rows = ['ENTRY descriptor=0 step=1']
    for reg in range(16, 20):
        rows.append(f'INPUT descriptor=0 step=1 reg={reg} value=3f800000')
    for i in range(16):
        rows.append(f'INPUT descriptor=0 step=1 reg={32+i} value={"3f800000" if i % 5 == 0 else "00000000"}')
    rows.append('OP descriptor=0 index=0 pc=8c03a9b0 op=ftrv rd=r16:4:1,1,1,1 rd2=- rs1=r16:4:0,0,0,0 rs2=r32:16:' + ','.join(['0']*16))
    rows.extend(f'VALUE step=1 index=0 part={i} value=000000003f800000' for i in range(4))
    rows.append('EXIT descriptor=0 step=1 events=4')
    return '\n'.join('FC067_PRODUCER_'+r for r in rows)


class ProducerMatrixTests(unittest.TestCase):
    def test_identity(self):
        result = inspect(fixture(), 3)
        self.assertEqual(result['ftrv'], {'8c03a9b0': 1})
        self.assertFalse(result['camera_recovered'])
        self.assertFalse(result['fp_mode_observed'])

    def test_wrong_result(self):
        with self.assertRaisesRegex(ValueError, 'FTRV arithmetic'):
            inspect(fixture().replace('part=2 value=000000003f800000', 'part=2 value=0000000040000000'), 3)

    def test_observed_mode(self):
        text = fixture().replace('ENTRY descriptor=0 step=1', 'ENTRY descriptor=0 step=1 fpscr=40001 mxcsr=ffc0').replace('events=4', 'events=4 mxcsr=ffc0')
        self.assertTrue(inspect(text, 3, True)['fp_mode_observed'])
        for bad in (fixture(), text.replace('fpscr=40001', 'fpscr=0'),
                    text.replace('events=4 mxcsr=ffc0', 'events=4 mxcsr=9fc0')):
            with self.assertRaises(ValueError):
                inspect(bad, 3, True)

    def test_scalar_operations_and_wrong_result(self):
        for name, expected in (('fadd', '40000000'), ('fmul', '3f800000'), ('fdiv', '3f800000')):
            extra = f'FC067_PRODUCER_OP descriptor=0 index=1 pc=8c03a9f8 op={name} rd=r20:1:1 rd2=- rs1=r16:1:1 rs2=r17:1:1\n'
            extra += f'FC067_PRODUCER_VALUE step=1 index=1 part=0 value={expected}\n'
            text = fixture().replace('FC067_PRODUCER_EXIT', extra+'FC067_PRODUCER_EXIT').replace('events=4', 'events=5')
            self.assertEqual(inspect(text, 3)['scalar_fp'], {name: 1})
            with self.assertRaisesRegex(ValueError, 'scalar FP arithmetic'):
                inspect(text.replace(f'index=1 part=0 value={expected}', 'index=1 part=0 value=40400000'), 3)

    def test_store_source_and_address(self):
        extra = 'FC067_PRODUCER_OP descriptor=0 index=1 pc=8c03aa14 op=writem size=4 rd=- rd2=- rs1=i8ce6e460 rs2=r16:1:1 rs3=-\n'
        extra += 'FC067_PRODUCER_STORE step=1 index=1 address=8ce6e460 size=4 value=3f800000 actual=3f800000 exact=1\n'
        text = fixture().replace('FC067_PRODUCER_EXIT', extra+'FC067_PRODUCER_EXIT').replace('events=4', 'events=5')
        self.assertEqual(inspect(text, 3)['stores'], 1)
        for bad in (text.replace('address=8ce6e460', 'address=8ce6e464'),
                    text.replace('value=3f800000 actual=3f800000', 'value=40000000 actual=40000000')):
            with self.assertRaises(ValueError): inspect(bad, 3)

    def test_missing_operation_even_with_adjusted_count(self):
        text = fixture().replace('FC067_PRODUCER_VALUE step=1 index=0 part=3 value=000000003f800000\n', '').replace('events=4', 'events=3')
        with self.assertRaises(ValueError): inspect(text, 3)

    def test_incomplete_and_duplicate(self):
        for text in (fixture().replace('events=4', 'events=3'),
                     fixture().replace('part=3', 'part=2'),
                     fixture().split('FC067_PRODUCER_EXIT')[0]):
            with self.assertRaises(ValueError):
                inspect(text, 3)


if __name__ == '__main__':
    unittest.main()
