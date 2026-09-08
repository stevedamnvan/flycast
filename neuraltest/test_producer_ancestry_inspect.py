import unittest
from unittest.mock import patch
from producer_ancestry_inspect import inspect


def fixture(gap=False, overwrite=False):
    rows=['OP descriptor=0 index=0 pc=8c03c944 op=ftrv rd=r16:4:1,1,1,1 rd2=- rs1=- rs2=- rs3=-',
          'ENTRY descriptor=0 step=1 dispatch=1']
    rows += [f'VALUE index=0 part={i} value=00000001' for i in range(4)]
    rows += [f'ENTRY descriptor=1 step=2 dispatch={3 if gap else 2}']
    for i in range(3):
        rows += [f'INPUT reg={16+i} value=00000001',
                 f'OP descriptor=1 index={i} pc=8c03c952 op=writem rd=- rd2=- rs1=- rs2=r{16+i}:1:0 rs3=-',
                 f'MEMORY kind=write address={0x1000+4*i:08x} size=4',
                 f'STORE index={i} address={0x1000+4*i:08x} size=4']
    if overwrite:rows += ['MEMORY kind=write address=00001000 size=4']
    rows += ['ENTRY descriptor=2 step=3 dispatch=4']
    for i in range(3):
        rows += [f'OP descriptor=2 index={i} pc=8c03c9a4 op=readm rd=r{16+i}:1:1 rd2=- rs1=- rs2=- rs3=-',
                 f'READ index={i} address={0x1000+4*i:08x} size=4',f'VALUE index={i} part=0 value=00000001',
                 f'OP descriptor=2 index={3+i} pc=8c03c9ce op=writem rd=- rd2=- rs1=- rs2=r{16+i}:1:1 rs3=-',
                 f'STORE index={3+i} address={0x2000+4*i:08x} size=4']
    return '\n'.join('FC067_PRODUCER_'+r for r in rows)


@patch('producer_ancestry_inspect.evaluate', new=lambda node: 1)
class AncestryTests(unittest.TestCase):
    def test_accumulation_retains_both_transforms(self):
        extra=['OP descriptor=0 index=1 pc=8c03c96a op=ftrv rd=r24:4:1,1,1,1 rd2=- rs1=- rs2=- rs3=-']
        extra += [f'VALUE index=1 part={i} value=00000001' for i in range(4)]
        for i in range(3):
            extra += [f'OP descriptor=0 index={i+2} pc=8c03c974 op=fadd rd=r{16+i}:1:2 rd2=- rs1=r{16+i}:1:1 rs2=r{24+i}:1:1 rs3=-',
                      f'VALUE index={i+2} part=0 value=00000001']
        text=fixture().replace('FC067_PRODUCER_ENTRY descriptor=1', '\n'.join('FC067_PRODUCER_'+r for r in extra)+'\nFC067_PRODUCER_ENTRY descriptor=1')
        record=dict(step=3,base=0x2000,xyz=[1,1,1],family='calc')
        with patch('producer_ancestry_inspect.memory_edges',return_value={}), patch('producer_ancestry_inspect.records',return_value=({},[record])):
            self.assertEqual(inspect(text)['contribution_histogram'],{'calc:2':1})

    def test_memory_origin_and_invalidation(self):
        record=dict(step=3,base=0x2000,xyz=[1,1,1],family='calc')
        # Isolated propagation tests; integrated runs separately validate arithmetic/edges.
        with patch('producer_ancestry_inspect.memory_edges',return_value={}), patch('producer_ancestry_inspect.records',return_value=({},[record])):
            self.assertEqual(inspect(fixture())['records_with_contribution_sets'],1)
            self.assertEqual(inspect(fixture(gap=True))['records_without_complete_sets'],1)
            self.assertEqual(inspect(fixture(overwrite=True))['records_without_complete_sets'],1)
