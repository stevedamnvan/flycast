"""Synthetic integer-valued transform, with hand-selected exact binary32 goldens."""
import unittest
from extra_writer_inspect import inspect


def fixture():
    lines = []
    def emit(tag, **fields):
        lines.append('FC067_EXTRA_'+tag+' '+' '.join(f'{k}={v}' for k,v in fields.items()))
    emit('FILTER',address='8c10000c',word='3f000001',checks=1)
    emit('SELECTION',generation=1790,candidate=1,target='8ce6e478',pc='8c03c97c',expected_target='8ce6e478')
    emit('ENTRY',block='8c03c95c',ops=31,inputs=21,descriptor=0,fpscr='40001',mxcsr='fffd',cycle=123)
    live = {1:0xfff0,2:4,4:0x8ce6e460,5:0x8c100000,7:2}
    live.update({r:0 for r in range(32,48)})
    live.update({32:0x3f800000,37:0x3f800000,42:0x3f800000,47:0x3f800000})
    for r,v in live.items():
        emit('INPUT',reg=r,value=f'{v:x}',expected=f'{v:x}',exact=1)
    ops=[]
    def reg(r,v): return f'r{r}:1:{v}'
    def op(pc,name,rd,a,b='-',size=0):
        ops.append(dict(index=len(ops),pc=f'{0x8c03c95c+pc:x}',op=name,size=size,rd=rd,rd2='-',rs1=a,rs2=b,rs3='-'))
    for i in range(3):
        op(i*2,'readm',reg(16+i,1),reg(5,i),size=4)
        op(i*2,'add',reg(5,i+1),reg(5,i),'i4')
    op(6,'readm',reg(0,1),reg(5,3),size=4)
    op(8,'readm',reg(19,1),reg(5,3),size=4)
    op(8,'add',reg(5,4),reg(5,3),'i4')
    op(10,'shld',reg(0,2),reg(0,1),reg(2,0))
    op(12,'and',reg(0,3),reg(0,2),reg(1,0))
    op(14,'ftrv','r16:4:2,2,2,2','r16:4:1,1,1,1','r32:16:'+','.join(['0']*16))
    op(16,'add',reg(0,4),reg(0,3),reg(4,0))
    for i in range(3):
        op(18+i*2,'readm',reg(20+i,1),reg(0,4+i),size=4)
        op(18+i*2,'add',reg(0,5+i),reg(0,4+i),'i4')
    for i in range(3): op(24+i*2,'fadd',reg(16+i,3),reg(16+i,2),reg(20+i,1))
    op(30,'sub',reg(7,1),reg(7,0),'i1')
    op(30,'seteq',reg(68,1),reg(7,1),'i0')
    op(32,'sub',reg(0,8),reg(0,7),'i4')
    op(32,'writem','-',reg(0,8),reg(18,3),4)
    op(34,'sub',reg(0,9),reg(0,8),'i4')
    op(34,'writem','-',reg(0,9),reg(17,3),4)
    op(36,'jcond',reg(71,1),reg(68,1))
    op(38,'sub',reg(0,10),reg(0,9),'i4')
    op(38,'writem','-',reg(0,10),reg(16,3),4)
    for row in ops: emit('OP',**row)
    # Identity matrix: input(1,2,3,W), existing(4,5,6), output(5,7,9).
    # These goldens do not call the verifier or its binary32 oracle.
    values = [[0x3f800000],[0x8c100004],[0x40000000],[0x8c100008],
              [0x40400000],[0x8c10000c],[0x3f000001],[0x3f000001],
              [0x8c100010],[0xf0000010],[0x10],
              [0x3f800000,0x40000000,0x40400000,0x3f000001],
              [0x8ce6e470],[0x40800000],[0x8ce6e474],[0x40a00000],
              [0x8ce6e478],[0x40c00000],[0x8ce6e47c],
              [0x40a00000],[0x40e00000],[0x41100000],[1],[0],
              [0x8ce6e478],[],[0x8ce6e474],[],[0],[0x8ce6e470],[]]
    addresses={0:0x8c100000,2:0x8c100004,4:0x8c100008,6:0x8c10000c,7:0x8c10000c,
               13:0x8ce6e470,15:0x8ce6e474,17:0x8ce6e478}
    stores={25:(0x8ce6e478,0x41100000),27:(0x8ce6e474,0x40e00000),30:(0x8ce6e470,0x40a00000)}
    event=0
    for i,row in enumerate(ops):
        if i in addresses:
            event+=1; emit('READ',event=event,index=i,address=f'{addresses[i]:x}',size=4)
        for part,value in enumerate(values[i]):
            event+=1
            r,count,versions=row['rd'][1:].split(':')
            emit('VALUE',event=event,index=i,slot=0,part=part,reg=int(r)+part,
                 version=versions.split(',')[part],count=1,value=f'{value:x}')
        if i in stores:
            address,value=stores[i]; event+=1
            emit('STORE',event=event,pc=row['pc'],address=f'{address:x}',size=4,value=f'{value:x}',actual=f'{value:x}',exact=1)
    emit('EXIT',descriptor=0,events=42,selected=1,mxcsr='fffd')
    lines.append('FC067_CT_UNKNOWN_WRITE slot=4 pc=8c03c97c address=8ce6e478 size=4')
    return '\n'.join(lines)


class ExtraWriterTests(unittest.TestCase):
    def test_explicit_targets_remain_bounded(self):
        text=fixture().replace('generation=1790','generation=1792')
        text=text.replace('8ce6e478','8ce6e488').replace('8ce6e474','8ce6e484').replace('8ce6e470','8ce6e480')
        text=text.replace('8ce6e47c','8ce6e48c').replace('8ce6e460','8ce6e470')
        text='\n'.join(l for l in text.splitlines() if 'FC067_CT_UNKNOWN_WRITE' not in l)
        self.assertTrue(inspect(text,0x8ce6e488,1792,False)['accumulation_verified'])
        with self.assertRaises(ValueError): inspect(text,0x8ce6e478,1792,False)
        with self.assertRaises(ValueError): inspect(text,0x8ce6e488,1791,False)

    def test_independent_goldens(self):
        result=inspect(fixture())
        self.assertEqual([v for a,v in result['stores']],[0x41100000,0x40e00000,0x40a00000])
        self.assertEqual(result['source_words'][3],0x3f000001)
        self.assertFalse(result['complete_source_chain'])
        self.assertFalse(result['world_camera_recovered'])

    def test_negative_controls(self):
        variants=[('generation=1790','generation=1791'),('expected_target=8ce6e478','expected_target=8ce6e474'),
                  ('word=3f000001','word=3f000002'),('op=fadd','op=add'),
                  ('actual=41100000','actual=41100001'),('rs2=r32:16:','rs2=r33:16:'),
                  ('value=40a00000\nFC067_EXTRA_VALUE event=32','value=3f800000\nFC067_EXTRA_VALUE event=32'),
                  ('FC067_EXTRA_VALUE event=41','FC067_EXTRA_VALUE event=40')]
        for old,new in variants:
            with self.subTest(control=old):
                self.assertIn(old,fixture())
                with self.assertRaises(ValueError): inspect(fixture().replace(old,new,1))


if __name__ == '__main__': unittest.main()
