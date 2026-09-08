import unittest
from unittest.mock import patch
from producer_continuity_inspect import inspect


def fixture(gap=False, wrong=False):
    rows=['OP descriptor=0 index=0 pc=8c03a9b0 op=ftrv rd=r16:4:1,1,1,1 rd2=- rs1=- rs2=- rs3=-',
          'ENTRY descriptor=0 step=1 dispatch=10']
    rows += [f'VALUE step=1 index=0 part={i} value={i+1:08x}' for i in range(4)]
    rows += [f'ENTRY descriptor=1 step=2 dispatch={12 if gap else 11}']
    for i in range(3):
        rows += [f'INPUT reg={16+i} value={9 if wrong else i+1:08x}',
                 f'OP descriptor=1 index={i} pc=8c03a9c2 op=writem rd=- rd2=- rs1=- rs2=r{16+i}:1:0 rs3=-',
                 f'STORE step=2 index={i} address={0x1000+4*i:08x}']
    return '\n'.join('FC067_PRODUCER_'+r for r in rows)


class ProducerContinuityTests(unittest.TestCase):
    def test_edges_and_gap(self):
        record=dict(step=2,base=0x1000,xyz=[1,2,3],family='direct')
        # Isolate continuity logic; integrated CLI separately runs arithmetic verification.
        with patch('producer_continuity_inspect.inspect_records',return_value=({},[record])):
            self.assertEqual(inspect(fixture())['records_with_matrix_continuity'],1)
            self.assertEqual(inspect(fixture(gap=True))['records_without_matrix_continuity'],1)
            with self.assertRaisesRegex(ValueError,'cross-block register mismatch'):
                inspect(fixture(wrong=True))
