"""Synthetic binding protocol only; no inferred emulator provenance."""
import unittest
from large_ledger_binding_inspect import inspect


def fixture():
    lines=[];rows=[]
    for frame in range(3):
        gen=1790+frame;ordinal=1781+frame
        for slot in range(921):
            for kind in range(4):lines.append(f'FC067_CT_BLOCK generation={gen} slot={slot} kind={kind} base={0x1000+slot*32:x}')
        for index in range(2745):
            slot=index%921;base=0x1000+slot*32;sample=frame*2745+index+1;copy=frame*5069+index+1
            rows.append(dict(sample=sample,vertex=1420+index,generation=gen,ordinal=ordinal,draw=277,
                             ram_x=base,cycle=10,decoder_pointer=0x100000+sample*32,after=[0,11,22,33]))
            for component,value in enumerate((11,22,33)):
                lines.append(f'FC067_CT_GATHER generation={gen} sample={copy} slot={slot} component={component} address={base+component*4:x} value={value:x} expected={value:x}')
            lines.append(f'FC067_LEDGER_BIND generation={gen} sample={sample} copy_id={copy} vertex={1420+index} base={base:x} pointer={0x100000+sample*32:x} exact=1')
        lines.append(f'FC067_CT_PREFIX generation={gen} ordinal={ordinal} copies=5069 cycle=20')
    return '\n'.join(lines)+'\n',rows


class BindingTests(unittest.TestCase):
    def test_complete_protocol(self):
        result=inspect(*fixture())
        self.assertEqual(result['supported_consumers'],8235)
        self.assertEqual(result['traced_address_instances'],2763)
        self.assertFalse(result['original_transform_arithmetic_proven'])

    def test_negative_controls(self):
        text,rows=fixture()
        for old,new in [('copy_id=1 vertex','copy_id=2 vertex'),('value=b expected=b','value=a expected=b'),
                        ('copies=5069','copies=5068'),('sample=1 copy_id=1','sample=2 copy_id=1'),
                        ('kind=0 base=1000','kind=0 base=1004')]:
            self.assertIn(old,text)
            with self.subTest(old=old),self.assertRaises(ValueError):inspect(text.replace(old,new,1),rows)


if __name__=='__main__':unittest.main()
