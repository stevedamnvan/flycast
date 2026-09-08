import re
import unittest

import test_reverse_copy_inspect as copy_fixture
from transform_span_inspect import records
from xyz_operand_inspect import inspect, calculate, check_controls, mutate_input


def fixture():
    """Synthetic indexed gather; no captured game data or asset bytes."""
    f=copy_fixture.ReverseCopyTests(); f.setUp()
    packet=records(f.session,'FC067_REVERSE_DECODE')[0]['words'].split(',')
    original=','.join(packet); packet[4]='3f803f80'
    f.session=f.session.replace(original,','.join(packet))
    f.session=re.sub(r'(COPY_WORD generation=1 index=4 before=)[0-9a-f]+ after=[0-9a-f]+',
                     r'\g<1>3f803f80 after=3f803f80',f.session)
    f.scene['vertices'][4][3]=0x3f803f80
    inputs={4:0xe0000020,5:0x1000,6:0x2000,9:0x3fff,10:0x3000,14:4}
    # Each tuple is an independently specified scalar SSA operation.
    ops=[
        ('readm',2,'r0:1:1','r5:1:0','-','-'),
        ('add',0,'r5:1:1','r5:1:0','i2','-'),
        ('readm',2,'r1:1:1','r5:1:1','-','-'),
        ('add',0,'r5:1:2','r5:1:1','i2','-'),
        ('readm',4,'r2:1:1','r5:1:2','-','-'),
        ('add',0,'r5:1:3','r5:1:2','i4','-'),
        ('setge',0,'r68:1:1','r1:1:1','i0','-'),
        ('and',0,'r0:1:2','r0:1:1','r9:1:0','-'),
        ('and',0,'r1:1:2','r1:1:1','r9:1:0','-'),
        ('shld',0,'r0:1:3','r0:1:2','r14:1:0','-'),
        ('shld',0,'r1:1:3','r1:1:2','r14:1:0','-'),
        ('add',0,'r0:1:4','r0:1:3','r6:1:0','-'),
        ('add',0,'r1:1:4','r1:1:3','r10:1:0','-'),
        ('readm',4,'r12:1:1','r0:1:4','-','-'),
        ('readm',4,'r13:1:1','r0:1:4','-','i4'),
        ('readm',4,'r0:1:5','r0:1:4','-','i8'),
        ('writem',4,'-','r4:1:0','r12:1:1','i4'),
        ('writem',4,'-','r4:1:0','r13:1:1','i8'),
        ('writem',4,'-','r4:1:0','r0:1:5','ic'),
        ('readm',4,'r12:1:2','r1:1:4','-','-'),
        ('readm',4,'r13:1:2','r1:1:4','-','i4'),
        ('writem',4,'-','r4:1:0','r2:1:1','i10'),
        ('writem',4,'-','r4:1:0','r12:1:2','i18'),
        ('writem',4,'-','r4:1:0','r13:1:2','i1c'),
        ('shl',0,'r2:1:2','r2:1:1','i10','-'),
        ('jcond',0,'r71:1:1','r68:1:1','-','-'),
        ('writem',4,'-','r4:1:0','r2:1:2','i14')]
    values=dict(zip([0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,19,20,24,25],
        [5,0x1002,3,0x1004,0x3f803f80,0x1008,1,5,3,0x50,0x30,0x2050,0x3030,
         0x3f800000,0x40000000,0x3f000000,0xff0000ff,0,0x3f800000,1]))
    reads={0:0x1000,2:0x1002,4:0x1004,13:0x2050,14:0x2054,15:0x2058,19:0x3030,20:0x3034}
    store_word={16:1,17:2,18:3,21:4,22:6,23:7,26:5}
    pcs=['8c03cc40','8c03cc82','8c03cc84','8c03cc86','8c03cc8c','8c03cc96','8c03cc8e','8c03cc90']
    def store(word,event):
        return (f'FC067_SQ_STORE event={event} generation=1 cycle=7602643776 pc={pcs[word]} '
                f'address={0xe0000020+4*word:08x} offset={4*word} size=4 '
                f'expected={int(packet[word],16):016x} actual={int(packet[word],16):016x} exact=1\n')
    s='FC067_SQ_BEGIN cycle=7602643776 generation=1 boundary=first-observed-store\n'+store(0,1)
    s+='FC067_XYZ_ENTRY descriptor=0 step=1 block=8c03cc68 cycle=7602643776 ops=27 inputs=6 context=00509700 ta_offset=32 sq=e0000020\n'
    for reg,value in inputs.items():
        s+=f'FC067_XYZ_INPUT descriptor=0 reg={reg} name=r{reg} value={value:08x} expected={value:08x} exact=1\n'
    for i,(name,size,rd,a,b,c) in enumerate(ops):
        pc=pcs[store_word[i]] if i in store_word else f'{0x8c03cc68+2*i:08x}'
        s+=f'FC067_XYZ_OP descriptor=0 index={i} pc={pc} op={name} size={size} rd={rd} rd2=- rs1={a} rs2={b} rs3={c}\n'
    event=1
    for i,(name,size,rd,a,b,c) in enumerate(ops):
        if i in reads:
            s+=f'FC067_XYZ_READ descriptor=0 index={i} address={reads[i]:08x} size={size}\n'
        if i in values:
            reg,_,version=rd[1:].split(':')
            s+=f'FC067_XYZ_VALUE descriptor=0 index={i} slot=0 part=0 reg={reg} version={version} count=1 value={values[i]:016x}\n'
        else:
            event+=1; s+=store(store_word[i],event)
    s+='FC067_XYZ_EXIT descriptor=0 events=28\nFC067_SQ_FLUSH event=9 generation=1 cycle=7602643776 address=e0000020 coverage=ffffffff exact=1 target=1\n'
    for i,writer in enumerate([1,2,3,4,5,8,6,7]):
        s+=f'FC067_SQ_WORD index={i} value={packet[i]} writers={writer},{writer},{writer},{writer}\n'
    return s+'FC067_SQ_STOP reason=target-covered events=9\n'+f.session,f.scene,f.manifest


class XyzOperandTests(unittest.TestCase):
    def setUp(self):
        self.session,self.scene,self.manifest=fixture()

    def test_supported(self):
        result=inspect(self.session,self.scene,self.manifest)
        self.assertEqual(result['position_ram_addresses'],['00002050','00002054','00002058'])
        self.assertFalse(result['position_value_calculation_proven'])

    def test_controls(self):
        self.assertEqual(check_controls(self.session,self.scene,self.manifest),5)

    def test_wrong_base_still_internally_agrees(self):
        with self.assertRaisesRegex(ValueError,'arithmetic'):
            inspect(mutate_input(self.session,6,0x2001),self.scene,self.manifest)

    def test_missing_and_reordered(self):
        for token in ['FC067_XYZ_READ descriptor=0 index=13','FC067_XYZ_VALUE descriptor=0 index=13']:
            s='\n'.join(line for line in self.session.splitlines() if token+' ' not in line)
            with self.subTest(token=token),self.assertRaises(ValueError): inspect(s,self.scene,self.manifest)

        lines=self.session.splitlines()
        first=next(i for i,line in enumerate(lines) if 'XYZ_VALUE descriptor=0 index=1 ' in line)
        second=next(i for i,line in enumerate(lines) if 'XYZ_VALUE descriptor=0 index=3 ' in line)
        lines[first],lines[second]=lines[second],lines[first]
        with self.assertRaisesRegex(ValueError,'ordering'):
            inspect('\n'.join(lines),self.scene,self.manifest)

    def test_stale_ssa(self):
        with self.assertRaises(ValueError):
            inspect(self.session.replace('rs2=r12:1:1 rs3=i4','rs2=r12:1:0 rs3=i4'),self.scene,self.manifest)

    def test_wrong_returned_load(self):
        s=self.session.replace('index=13 slot=0 part=0 reg=12 version=1 count=1 value=000000003f800000',
                               'index=13 slot=0 part=0 reg=12 version=1 count=1 value=000000003f800001')
        with self.assertRaisesRegex(ValueError,'SQ store'): inspect(s,self.scene,self.manifest)

    def test_integer_semantics(self):
        self.assertEqual(calculate('add',0xffffffff,1),0)
        self.assertEqual(calculate('shld',0x80000000,0xffffffff),0x40000000)
        self.assertEqual(calculate('shld',0xffffffff,0xffffffe0),0)
        self.assertEqual(calculate('setge',0xffffffff,0),0)
        self.assertEqual(calculate('setge',0,0xffffffff),1)

    def test_wrong_target_or_reject(self):
        for s in [self.session.replace('ta_offset=32','ta_offset=64'),self.session+'FC067_XYZ_REJECT reason=test\n']:
            with self.subTest(session=s),self.assertRaises(ValueError): inspect(s,self.scene,self.manifest)


if __name__ == '__main__': unittest.main()
