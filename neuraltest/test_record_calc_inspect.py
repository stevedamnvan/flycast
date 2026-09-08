import unittest

from record_calc_inspect import inspect_block, check_controls, calculate


def fixture():
    """Independent synthetic golden: a=-100, b=-20, q=1/4 -> X295,Y235,Z1/4."""
    inputs = {4:0x8ce7425c,5:2,16:0xc2c80000,17:0xc1a00000,
              19:0x3e800000,20:0x43a00000,21:0x43700000}
    # op, pc suffix, size, output, a, b, independently specified result
    ops = [
        ('mov32','c0',0,'r24:1:1','r19:1:0','-',0x3e800000),
        ('fmul','c2',0,'r16:1:1','r16:1:0','r19:1:0',0xc1c80000),
        ('fmul','c4',0,'r17:1:1','r17:1:0','r24:1:1',0xc0a00000),
        ('fadd','c6',0,'r16:1:2','r16:1:1','r20:1:0',0x43938000),
        ('fadd','c8',0,'r17:1:2','r17:1:1','r21:1:0',0x436b0000),
        ('sub','ca',0,'r4:1:1','r4:1:0','i4',0x8ce74258),
        ('writem','ca',4,'-','r4:1:1','r19:1:0',0x3e800000),
        ('sub','cc',0,'r4:1:2','r4:1:1','i4',0x8ce74254),
        ('writem','cc',4,'-','r4:1:2','r17:1:2',0x436b0000),
        ('sub','ce',0,'r4:1:3','r4:1:2','i4',0x8ce74250),
        ('writem','ce',4,'-','r4:1:3','r16:1:2',0x43938000),
        ('sub','d0',0,'r5:1:1','r5:1:0','i1',1),
        ('seteq','d0',0,'r68:1:1','r5:1:1','i0',0),
        ('add','d2',0,'r4:1:4','r4:1:3','i10',0x8ce74260),
        ('jcond','d4',0,'r71:1:1','r68:1:1','-',0),
        ('readm','d6',4,'r16:1:3','r4:1:4','-',0xc2b40000),
        ('add','d6',0,'r4:1:5','r4:1:4','i4',0x8ce74264)]
    lines=['FC067_CALC_ENTRY block=8c03c9c0 cycle=7602640640 descriptor=0 ops=17 inputs=7 fpscr=40001 mxcsr=fffd']
    for reg,value in inputs.items():
        lines.append(f'FC067_CALC_INPUT reg={reg} value={value:x} expected={value:x} exact=1')
    for index,(op,pc,size,rd,a,b,value) in enumerate(ops):
        lines.append(f'FC067_CALC_OP index={index} pc=8c03c9{pc} op={op} size={size} rd={rd} rd2=- rs1={a} rs2={b} rs3=-')
    event=0
    for index,(op,pc,size,rd,a,b,value) in enumerate(ops):
        event+=1
        if op=='writem':
            address={6:0x8ce74258,8:0x8ce74254,10:0x8ce74250}[index]
            lines.append(f'FC067_CALC_STORE event={event} pc=8c03c9{pc} address={address:x} size=4 value={value:x}')
            continue
        if op=='readm':
            lines.append(f'FC067_CALC_READ event={event} index={index} address=8ce74260 size=4')
            event+=1
        reg,_,version=rd[1:].split(':')
        lines.append(f'FC067_CALC_VALUE event={event} index={index} slot=0 part=0 reg={reg} version={version} count=1 value={value:x}')
    lines.append('FC067_CALC_EXIT descriptor=0 events=18 selected=1 mxcsr=fffd')
    return '\n'.join(lines)+'\n'


class RecordCalcTests(unittest.TestCase):
    def test_golden_and_controls(self):
        result=inspect_block(fixture())
        self.assertEqual(result['final_words_xyz'],[0x43938000,0x436b0000,0x3e800000])
        self.assertEqual(result['float_ops_verified'],4)
        self.assertEqual(result['mxcsr_rounding_mode'],3)
        self.assertFalse(result['world_camera_recovered'])
        self.assertFalse(result['depth_factor_origin_proven'])
        self.assertEqual(check_controls(fixture()),5)

    def test_control_does_not_mutate_other_observers(self):
        session='FC067_RAM_WRITE event=5 pc=8c03c9cc address=8ce74254\n'+fixture()
        self.assertEqual(check_controls(session),5)

    def test_rounding_and_unsupported_arithmetic(self):
        self.assertEqual(calculate('fadd',0x3f800000,0x33800000,0),0x3f800000)
        self.assertEqual(calculate('fadd',0x3f800000,0x33800000,2),0x3f800001)
        for name,a,b in [('fdiv',0x3f800000,0x40000000),('fmul',1,0x3f800000),('fadd',0x7f800000,0)]:
            with self.assertRaises(ValueError):
                calculate(name,a,b,0)

    def test_missing_mode_and_live_rejection(self):
        for source in [fixture().replace(' mxcsr=fffd','',1),fixture()+'FC067_CALC_REJECT reason=entry-operand-mismatch\n']:
            with self.assertRaises(ValueError):
                inspect_block(source)

    def test_wrong_values_shapes_and_pointer(self):
        for old,new in [('reg=4 value=8ce7425c expected=8ce7425c','reg=4 value=8ce7426c expected=8ce7426c'),
                        ('count=1 value=c1c80000','count=1 value=c1c80001'),
                        ('rd=r24:1:1','rd=r24:2:1,1'),('rs1=r16:1:1','rs1=r16:1:0'),
                        ('address=8ce74260 size=4','address=8ce74264 size=4')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                inspect_block(fixture().replace(old,new,1))

    def test_duplicate_invocation_and_missing_event(self):
        lines=fixture().splitlines()
        value=next(line for line in lines if 'FC067_CALC_VALUE event=1 ' in line)
        for source in [fixture()+fixture(),fixture().replace(value+'\n','')]:
            with self.assertRaises(ValueError):
                inspect_block(source)

    def test_rounding_change_and_stale_ssa(self):
        for source in [fixture().replace('selected=1 mxcsr=fffd','selected=1 mxcsr=9ffd'),
                       fixture().replace('reg=16 version=2','reg=16 version=1',1)]:
            with self.assertRaises(ValueError):
                inspect_block(source)

    def test_pc_and_unary_shape(self):
        for old,new in [('pc=8c03c9c2','pc=8c03c9c4'),
                        ('rs1=r19:1:0 rs2=-','rs1=r19:1:0 rs2=i1')]:
            with self.assertRaises(ValueError):
                inspect_block(fixture().replace(old,new,1))


if __name__=='__main__':
    unittest.main()
