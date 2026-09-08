import unittest

from factor_edge_inspect import inspect_edge, check_controls, source_record


def fixture():
    # Synthetic record Y=-20,Z=4; exact reciprocal q=1/4.
    rows=['FC067_PRED_ENTRY block=8c03c9a4 cycle=7602640640 descriptor=1 ops=7 inputs=2 fpscr=40001 mxcsr=fffd',
          'FC067_PRED_INPUT reg=4 value=8ce74254 expected=8ce74254 exact=1',
          'FC067_PRED_INPUT reg=23 value=0 expected=0 exact=1']
    ops=[('readm','a4',4,'r17:1:1','r4:1:0','-',0xc1a00000),
         ('add','a4',0,'r4:1:1','r4:1:0','i4',0x8ce74258),
         ('readm','a6',4,'r18:1:1','r4:1:1','-',0x40800000),
         ('add','a6',0,'r4:1:2','r4:1:1','i4',0x8ce7425c),
         ('mov32','a8',0,'r19:1:1','i3f800000','-',0x3f800000),
         ('fsetgt','aa',0,'r68:1:1','r18:1:1','r23:1:0',1),
         ('fdiv','ac',0,'r19:1:2','r19:1:1','r18:1:1',0x3e800000)]
    for index,(op,pc,size,rd,a,b,value) in enumerate(ops):
        rows.append(f'FC067_PRED_OP index={index} pc=8c03c9{pc} op={op} size={size} rd={rd} rd2=- rs1={a} rs2={b} rs3=-')
    event=0
    for index,(op,pc,size,rd,a,b,value) in enumerate(ops):
        if op=='readm':
            event+=1
            address=0x8ce74254 if index==0 else 0x8ce74258
            rows.append(f'FC067_PRED_READ event={event} index={index} address={address:x} size=4')
        event+=1
        reg,_,version=rd[1:].split(':')
        rows.append(f'FC067_PRED_VALUE event={event} index={index} slot=0 part=0 reg={reg} version={version} count=1 value={value:x}')
    rows+=['FC067_PRED_EXIT descriptor=1 events=9 next=8c03c9c0 pointer=8ce7425c factor=3e800000 cycle=7602640640 mxcsr=fffd',
           'FC067_PRED_EDGE block=8c03c9c0 pointer=8ce7425c factor=3e800000 expected=3e800000 cycle=7602640640 exact=1']
    return '\n'.join(rows)+'\n'


class FactorEdgeTests(unittest.TestCase):
    def test_golden_and_controls(self):
        result=inspect_edge(fixture())
        self.assertEqual(result['factor_word'],0x3e800000)
        self.assertEqual(result['source_loads'],[(0x8ce74254,0xc1a00000),(0x8ce74258,0x40800000)])
        self.assertTrue(result['reciprocal_record_depth_proven'])
        self.assertEqual(result['record_z_coordinate_system'],'unknown')
        self.assertFalse(result['world_camera_recovered'])
        self.assertEqual(check_controls(fixture()),5)

    def test_wrong_edge_factor_or_pointer(self):
        for old,new in [('expected=3e800000','expected=3e800001'),
                        ('next=8c03c9c0','next=8c03c9c2'),('pointer=8ce7425c','pointer=8ce7426c'),
                        ('reg=19 version=2','reg=19 version=1')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                inspect_edge(fixture().replace(old,new,1))

    def test_arithmetic_and_nonfinite_fail_closed(self):
        for old,new in [('value=3e800000','value=3e800001'),('value=40800000','value=0'),
                        ('value=40800000','value=7f800000'),('value=40800000','value=1'),
                        ('op=fdiv','op=fmul')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                inspect_edge(fixture().replace(old,new,1))

    def test_mode_and_duplicate_witness(self):
        for source in [fixture()+fixture(),fixture()+'FC067_PRED_REJECT reason=next-entry-mismatch\n',
                       fixture().replace('events=9 next=','events=8 next='),
                       fixture().replace('cycle=7602640640 mxcsr=fffd','cycle=7602640640 mxcsr=9ffd')]:
            with self.assertRaises(ValueError):
                inspect_edge(source)

    def test_load_address_or_event_order(self):
        for old,new in [('index=2 address=8ce74258','index=2 address=8ce74254'),
                        ('event=4 index=2','event=5 index=2'),('pc=8c03c9aa','pc=8c03c9ac')]:
            with self.assertRaises(ValueError):
                inspect_edge(fixture().replace(old,new,1))

    def test_source_generation_before_overwrite(self):
        words=[0xc2c80000,0xc1a00000,0x40800000]
        rows=[]
        for component,word in enumerate(words):
            for byte,value in enumerate(word.to_bytes(4,'little')):
                rows.append(f'FC067_RAM_BYTE event={3-component} byte={4*component+byte} actual={value:02x}')
        session='\n'.join(rows)+'\n'+fixture()
        loads=[(0x8ce74254,words[1]),(0x8ce74258,words[2])]
        self.assertEqual(source_record(session,loads),[[2]*4,[1]*4])
        with self.assertRaises(ValueError):
            source_record(session.replace('event=1 byte=8 actual=00','event=1 byte=8 actual=01'),loads)
        with self.assertRaises(ValueError):
            source_record(session,[(0x8ce74260,0)])
        # Later overwritten values cannot retroactively satisfy the source read.
        with self.assertRaises(ValueError):
            source_record(fixture()+'\n'+'\n'.join(rows),loads)


if __name__=='__main__':
    unittest.main()
