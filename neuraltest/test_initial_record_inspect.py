import unittest

from initial_record_inspect import inspect_block,check_controls


def fixture():
    # Synthetic precomputed register values; deliberately no transform claim.
    inputs={0:0x8ce7425c,6:3,16:0x3f800000,17:0x40000000,18:0x40800000}
    rows=['FC067_INITIAL_ENTRY block=8c03c94c cycle=7602512960 descriptor=0 ops=10 inputs=5 fpscr=40001 mxcsr=fffd']
    for reg,value in inputs.items(): rows.append(f'FC067_INITIAL_INPUT reg={reg} value={value:x} expected={value:x} exact=1')
    ops=[('sub','4c',0,'r6:1:1','r6:1:0','i1',2),
         ('seteq','4c',0,'r68:1:1','r6:1:1','i0',0),
         ('sub','4e',0,'r0:1:1','r0:1:0','i4',0x8ce74258),
         ('writem','4e',4,'-','r0:1:1','r18:1:0',0x40800000),
         ('sub','50',0,'r0:1:2','r0:1:1','i4',0x8ce74254),
         ('writem','50',4,'-','r0:1:2','r17:1:0',0x40000000),
         ('sub','52',0,'r0:1:3','r0:1:2','i4',0x8ce74250),
         ('writem','52',4,'-','r0:1:3','r16:1:0',0x3f800000),
         ('jcond','54',0,'r71:1:1','r68:1:1','-',0),
         ('add','56',0,'r0:1:4','r0:1:3','i10',0x8ce74260)]
    for index,(op,pc,size,rd,a,b,value) in enumerate(ops):
        rows.append(f'FC067_INITIAL_OP index={index} pc=8c03c9{pc} op={op} size={size} rd={rd} rd2=- rs1={a} rs2={b} rs3=-')
    for index,(op,pc,size,rd,a,b,value) in enumerate(ops):
        if op=='writem':
            address={3:0x8ce74258,5:0x8ce74254,7:0x8ce74250}[index]
            rows.append(f'FC067_INITIAL_STORE event={index+1} pc=8c03c9{pc} address={address:x} size=4 value={value:x}')
        else:
            reg,_,version=rd[1:].split(':')
            rows.append(f'FC067_INITIAL_VALUE event={index+1} index={index} slot=0 part=0 reg={reg} version={version} count=1 value={value:x}')
    rows.append('FC067_INITIAL_EXIT descriptor=0 events=10 selected=1 mxcsr=fffd')
    return '\n'.join(rows)+'\n'


class InitialRecordTests(unittest.TestCase):
    def test_golden_preserves_unknown_calculation(self):
        result=inspect_block(fixture())
        self.assertEqual(result['initial_xyz_words'],[0x3f800000,0x40000000,0x40800000])
        self.assertTrue(result['initial_store_block_proven'])
        self.assertFalse(result['initial_coordinate_calculation_proven'])
        self.assertFalse(result['world_camera_recovered'])
        self.assertEqual(check_controls(fixture()),4)

    def test_control_ignores_earlier_ram_observer(self):
        self.assertEqual(check_controls('FC067_RAM_WRITE pc=8c03c950 address=8ce74254\n'+fixture()),4)

    def test_changed_inputs_and_results(self):
        for old,new in [('reg=0 value=8ce7425c expected=8ce7425c','reg=0 value=8ce7426c expected=8ce7426c'),
                        ('value=8ce74258','value=8ce74259'),('reg=0 version=2','reg=0 version=1'),
                        ('pc=8c03c94e','pc=8c03c950')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                inspect_block(fixture().replace(old,new,1))

    def test_duplicate_missing_or_live_rejected(self):
        first=next(line for line in fixture().splitlines() if 'FC067_INITIAL_VALUE event=1 ' in line)
        for source in [fixture()+fixture(),fixture().replace(first+'\n',''),
                       fixture()+'FC067_INITIAL_REJECT reason=entry-operand-mismatch\n']:
            with self.assertRaises(ValueError): inspect_block(source)

    def test_no_float_or_load_promotion(self):
        for old,new in [('op=sub','op=fmul'),('FC067_INITIAL_STORE event=4','FC067_INITIAL_READ event=4'),
                        ('count=1','count=4'),('size=4 rd=-','size=8 rd=-')]:
            with self.assertRaises(ValueError): inspect_block(fixture().replace(old,new,1))


if __name__=='__main__': unittest.main()
